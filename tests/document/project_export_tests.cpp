#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/document.h"
#include "document/keyframes.h"
#include "document/placement_edit.h"
#include "document/atlas_write.h"
#include "document/outline.h"
#include "document/project.h"
#include "document/project_export.h"
#include "support/expected.h"
#include "formats/afp_animation.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/texture_images.h"

#include "sample_package.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint16_t kOwned = 3;
constexpr uint16_t kUntouched = 9;

AfpAnimation::Placement Create(uint16_t depth, int32_t x) {
    AfpAnimation::Placement placement;
    placement.depth = depth;
    placement.end_frame = 6;
    placement.character = uint16_t{7};
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Placement Update(uint16_t depth, int32_t x) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting;
    placement.depth = depth;
    placement.end_frame = 6;
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

void Place(AfpAnimation::Container& clip, uint32_t frame, AfpAnimation::Placement placement) {
    AfpAnimation::Frame& owner = clip.frames[frame];
    const std::size_t at = owner.first_tag + owner.tag_count;
    clip.tags.insert(clip.tags.begin() + static_cast<std::ptrdiff_t>(at),
                     AfpAnimation::Tag{std::move(placement)});
    owner.tag_count++;
    for (std::size_t i = frame + 1; i < clip.frames.size(); i++)
        clip.frames[i].first_tag++;
}

Document::LoadedImage Solid(uint32_t width, uint32_t height, uint8_t shade) {
    Document::LoadedImage image{.width = width, .height = height, .bgra = {}};
    image.bgra.assign(static_cast<std::size_t>(width) * height * 4, shade);
    return image;
}

Document::ImageLoader NoImages() {
    return [](const std::string& file) {
        return Support::Expected<Document::LoadedImage, std::string>(
            Support::Unexpected(file + " was not expected"));
    };
}

std::string Path() {
    return "afp/" + SamplePackage::HashPath("intro");
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = SamplePackage::SampleAnimation();
    animation.root.frames = {};
    for (std::size_t i = 0; i < 6; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    animation.root.labels = {};
    Place(animation.root, 0, Create(kOwned, 0));
    Place(animation.root, 0, Create(kUntouched, 5000));
    Place(animation.root, 4, Update(kOwned, 400));
    Place(animation.root, 4, Update(kUntouched, 5400));
    return animation;
}

Document::File Package() {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    const std::string error = file.has_value() ? std::string() : file.error();
    INFO(error);
    REQUIRE(file.has_value());
    REQUIRE(file->WriteAnimation(Path(), Scene()).has_value());
    return std::move(*file);
}

std::vector<uint32_t> FramesPlacing(const AfpAnimation::Container& clip, uint16_t depth) {
    std::vector<uint32_t> frames;
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const auto* placement =
                std::get_if<AfpAnimation::Placement>(&clip.tags[owner.first_tag + i].body);
            if (placement != nullptr && placement->depth == depth) frames.push_back(frame);
        }
    }
    return frames;
}

Document::Project Owning(const Document::File& file, Document::Ease ease) {
    const auto animation = file.ReadAnimation(Path());
    REQUIRE(animation.has_value());
    auto owned = Document::OwnDepth(*animation, Path(), kOwned, 0);
    REQUIRE(owned.has_value());
    if (ease != Document::Ease::Hold) {
        REQUIRE(Document::SetKeyframeEase(owned->authored.tracks.front(), 0, ease, {}).has_value());
    }
    return Document::Project{
        .build = "iidx33", .ifs_path = "scene.ifs", .content = {owned->authored}, .images = {}};
}

}

TEST_CASE("Exporting an untouched owned depth leaves the package as it was") {
    Document::File file = Package();
    const auto before = file.Encode();
    REQUIRE(before.has_value());

    const Document::Project project = Owning(file, Document::Ease::Hold);
    const auto exported = Document::ExportProject(file, project, NoImages());
    if (!exported) FAIL(exported.error());

    const auto after = file.Encode();
    REQUIRE(after.has_value());
    CHECK(*after == *before);
}

TEST_CASE("Exporting the same project twice produces the same bytes") {
    Document::File first = Package();
    Document::File second = Package();
    const Document::Project project = Owning(first, Document::Ease::Linear);

    REQUIRE(Document::ExportProject(first, project, NoImages()).has_value());
    REQUIRE(Document::ExportProject(second, project, NoImages()).has_value());
    const auto one = first.Encode();
    const auto two = second.Encode();
    REQUIRE(one.has_value());
    REQUIRE(two.has_value());
    CHECK(*one == *two);
}

TEST_CASE("Exporting a project again changes nothing the first export did not") {
    Document::File file = Package();
    const Document::Project project = Owning(file, Document::Ease::Linear);

    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    const auto once = file.Encode();
    REQUIRE(once.has_value());
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    const auto twice = file.Encode();
    REQUIRE(twice.has_value());
    CHECK(*twice == *once);
}

TEST_CASE("An eased keyframe becomes one placement per frame") {
    Document::File file = Package();
    const Document::Project project = Owning(file, Document::Ease::Linear);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());

    const auto animation = file.ReadAnimation(Path());
    REQUIRE(animation.has_value());
    CHECK(FramesPlacing(animation->root, kOwned) == std::vector<uint32_t>{0, 1, 2, 3, 4});
    for (uint32_t frame = 0; frame <= 4; frame++) {
        const auto live = Document::LivePlacementTag(animation->root, kOwned, frame);
        REQUIRE(live.has_value());
        const auto* placement =
            std::get_if<AfpAnimation::Placement>(&animation->root.tags[*live].body);
        REQUIRE(placement != nullptr);
        REQUIRE(placement->translation.has_value());
        CHECK((*placement->translation)[0] == static_cast<int32_t>(frame) * 100);
    }
}

TEST_CASE("Export leaves a depth the project does not own alone") {
    Document::File file = Package();
    const Document::Project project = Owning(file, Document::Ease::Linear);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());

    const auto animation = file.ReadAnimation(Path());
    REQUIRE(animation.has_value());
    CHECK(FramesPlacing(animation->root, kUntouched) == std::vector<uint32_t>{0, 4});
    const auto live = Document::LivePlacementTag(animation->root, kUntouched, 4);
    REQUIRE(live.has_value());
    const auto* placement = std::get_if<AfpAnimation::Placement>(&animation->root.tags[*live].body);
    REQUIRE(placement != nullptr);
    REQUIRE(placement->translation.has_value());
    CHECK((*placement->translation)[0] == 5400);
}

TEST_CASE("A project owning nothing exports without touching the package") {
    Document::File file = Package();
    const auto before = file.Encode();
    REQUIRE(before.has_value());
    const Document::Project project{
        .build = "iidx33", .ifs_path = "scene.ifs", .content = {}, .images = {}};
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    const auto after = file.Encode();
    REQUIRE(after.has_value());
    CHECK(*after == *before);
}

TEST_CASE("Export says which depth it could not write") {
    Document::File file = Package();
    Document::Project project = Owning(file, Document::Ease::Hold);
    project.content.front().depth = 42;
    const auto exported = Document::ExportProject(file, project, NoImages());
    REQUIRE_FALSE(exported.has_value());
    CHECK(exported.error().find("42") != std::string::npos);
}

TEST_CASE("Export says when the animation is not there") {
    Document::File file = Package();
    Document::Project project = Owning(file, Document::Ease::Hold);
    project.content.front().animation = "afp/missing";
    CHECK_FALSE(Document::ExportProject(file, project, NoImages()).has_value());
}

namespace {

Document::ImageLoader Images() {
    return [](const std::string& file) -> Support::Expected<Document::LoadedImage, std::string> {
        if (file == "sources/small.png") return Solid(4, 4, 0x20);
        if (file == "sources/wide.png") return Solid(20, 6, 0x40);
        return Support::Unexpected(file + " is not a source image");
    };
}

Document::Project WithImages() {
    return Document::Project{
        .build = "iidx33",
        .ifs_path = "scene.ifs",
        .content = {},
        .images = {Document::SourceImage{.name = "wide", .file = "sources/wide.png"},
                   Document::SourceImage{.name = "small", .file = "sources/small.png"}}};
}

std::vector<TextureImages::Image> ListedImages(const Document::File& file) {
    const auto bytes = file.Encode();
    REQUIRE(bytes.has_value());
    const auto archive = Ifs::Read(*bytes);
    REQUIRE(archive.has_value());
    const auto tex = std::ranges::find(archive->entries, std::string("tex"), &Ifs::Entry::name);
    REQUIRE(tex != archive->entries.end());
    const auto list =
        std::ranges::find(tex->children, std::string("texturelist_Exml"), &Ifs::Entry::name);
    REQUIRE(list != tex->children.end());
    const auto document = BinaryXml::Read(list->bytes);
    REQUIRE(document.has_value());
    const auto listed = TextureImages::ReadList(*document);
    REQUIRE(listed.has_value());
    return listed->images;
}

}

TEST_CASE("Export packs the project's images into an atlas of their own") {
    Document::File file = Package();
    const std::size_t before = ListedImages(file).size();
    const auto exported = Document::ExportProject(file, WithImages(), Images());
    if (!exported) FAIL(exported.error());

    const std::vector<TextureImages::Image> listed = ListedImages(file);
    CHECK(listed.size() == before + 2);
    const auto small = std::ranges::find(listed, std::string("small"), &TextureImages::Image::name);
    const auto wide = std::ranges::find(listed, std::string("wide"), &TextureImages::Image::name);
    REQUIRE(small != listed.end());
    REQUIRE(wide != listed.end());
    CHECK(small->width == 6);
    CHECK(small->height == 6);
    CHECK(wide->width == 22);
    CHECK(wide->height == 8);
}

TEST_CASE("An exported image carries its pixels as its own entry") {
    Document::File file = Package();
    REQUIRE(Document::ExportProject(file, WithImages(), Images()).has_value());
    const auto entry = file.Describe("tex/" + SamplePackage::HashPath("small"));
    REQUIRE(entry.has_value());
    CHECK(entry->role == Document::Role::Texture);
    CHECK(entry->stored_size > 0);
}

TEST_CASE("Exporting images twice produces the same bytes") {
    Document::File first = Package();
    Document::File second = Package();
    REQUIRE(Document::ExportProject(first, WithImages(), Images()).has_value());
    REQUIRE(Document::ExportProject(second, WithImages(), Images()).has_value());
    const auto one = first.Encode();
    const auto two = second.Encode();
    REQUIRE(one.has_value());
    REQUIRE(two.has_value());
    CHECK(*one == *two);

    REQUIRE(Document::ExportProject(first, WithImages(), Images()).has_value());
    const auto again = first.Encode();
    REQUIRE(again.has_value());
    CHECK(*again == *one);
}

TEST_CASE("Export leaves the atlases it did not make alone") {
    Document::File file = Package();
    const std::vector<TextureImages::Image> before = ListedImages(file);
    REQUIRE(Document::ExportProject(file, WithImages(), Images()).has_value());
    const std::vector<TextureImages::Image> after = ListedImages(file);
    for (const TextureImages::Image& image : before) {
        const auto same = std::ranges::find(after, image.name, &TextureImages::Image::name);
        REQUIRE(same != after.end());
        CHECK(same->width == image.width);
        CHECK(same->height == image.height);
    }
}

TEST_CASE("An image the loader cannot read names itself") {
    Document::File file = Package();
    Document::Project project = WithImages();
    project.images.push_back(Document::SourceImage{.name = "gone", .file = "sources/gone.png"});
    const auto exported = Document::ExportProject(file, project, Images());
    REQUIRE_FALSE(exported.has_value());
    CHECK(exported.error().find("gone") != std::string::npos);
}
