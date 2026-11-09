#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/outline.h"
#include "document/place_image.h"
#include "document/placement_edit.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "document/timeline.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/env.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kViewWidth = 1920;
constexpr uint32_t kViewHeight = 1080;
constexpr uint32_t kShownFrame = 10;
constexpr uint32_t kLastFrame = 20;
constexpr std::size_t kBgraBytes = 4;
constexpr uint32_t kLargestSide = 400;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kNewFrames = 45;
const std::string kNewAnimation = "editor_new_scene";

struct Picked {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string AnimationPath(const Document::File& file) {
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == "title") return child.path;
        }
    }
    return {};
}

std::optional<Picked> LargestFittingImage(const Document::File& file) {
    std::optional<Picked> best;
    for (const Document::Node& node : file.Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Texture) continue;
            const auto details = file.Describe(child.path);
            if (!details || !details->texture) continue;
            const uint32_t area = details->texture->width * details->texture->height;
            if (details->texture->width > kLargestSide || details->texture->height > kLargestSide)
                continue;
            if (best && area <= best->width * best->height) continue;
            best = Picked{.name = details->name,
                          .width = details->texture->width,
                          .height = details->texture->height};
        }
    }
    return best;
}

uint16_t FreeDepth(const Document::File& file, const std::string& path) {
    const auto details = file.Describe(path);
    uint16_t highest = 0;
    if (details && details->animation) {
        for (const Document::DepthRow& row : details->animation->depths)
            highest = std::max(highest, row.depth);
    }
    return static_cast<uint16_t>(highest + 1);
}

std::vector<uint8_t> RenderPixels(PreviewClient::Host& host, const std::string& animation,
                                  std::span<const uint8_t> ifs, bool reload) {
    const auto loaded = host.LoadPackage("title", animation, ifs, reload);
    const std::string load_error = loaded.has_value() ? std::string() : loaded.error();
    INFO(load_error);
    REQUIRE(loaded.has_value());
    REQUIRE(host.Seek(kShownFrame).has_value());
    const auto frame = host.Render();
    REQUIRE(frame.has_value());
    auto reader = SharedTexture::Reader::Create();
    REQUIRE(reader.has_value());
    auto pixels = reader->Read(frame->shared_handle, frame->width, frame->height);
    REQUIRE(pixels.has_value());
    return std::move(*pixels);
}

uint32_t LoadedFrames(const std::string& dir, std::span<const uint8_t> ifs,
                      const std::string& animation) {
    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    REQUIRE((*host)->Boot(dir, "iidx33").has_value());
    const auto loaded = (*host)->LoadPackage("title", animation, ifs, false);
    const std::string load_error = loaded.has_value() ? std::string() : loaded.error();
    INFO(load_error);
    REQUIRE(loaded.has_value());
    return loaded.has_value() ? loaded->frame_count : 0;
}

struct Changed {
    std::size_t inside = 0;
    std::size_t outside = 0;
    std::optional<Document::Box> area;
};

bool Covers(const Document::StageOutline& outline, uint32_t x, uint32_t y) {
    const std::vector<Document::StageOutline> only{outline};
    const std::array<Document::Point, 4> corners{
        Document::Point{x + 0.0, y + 0.0}, Document::Point{x + 1.0, y + 0.0},
        Document::Point{x + 0.0, y + 1.0}, Document::Point{x + 1.0, y + 1.0}};
    return std::ranges::any_of(corners, [&only](const Document::Point& corner) {
        return Document::DepthAt(only, corner).has_value();
    });
}

void Note(Changed& changed, uint32_t x, uint32_t y) {
    if (!changed.area) {
        changed.area =
            Document::Box{.left = x + 0.0, .right = x + 1.0, .top = y + 0.0, .bottom = y + 1.0};
        return;
    }
    changed.area->left = std::min(changed.area->left, x + 0.0);
    changed.area->right = std::max(changed.area->right, x + 1.0);
    changed.area->top = std::min(changed.area->top, y + 0.0);
    changed.area->bottom = std::max(changed.area->bottom, y + 1.0);
}

Changed Compare(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after,
                const Document::StageOutline& outline) {
    Changed changed;
    for (uint32_t y = 0; y < kViewHeight; y++) {
        for (uint32_t x = 0; x < kViewWidth; x++) {
            const std::size_t at = (static_cast<std::size_t>(y) * kViewWidth + x) * kBgraBytes;
            const auto first = static_cast<std::ptrdiff_t>(at);
            const auto last = first + static_cast<std::ptrdiff_t>(kBgraBytes);
            if (std::equal(before.begin() + first, before.begin() + last, after.begin() + first))
                continue;
            Note(changed, x, y);
            if (Covers(outline, x, y)) {
                changed.inside++;
            } else {
                changed.outside++;
            }
        }
    }
    return changed;
}

double Span(const Document::StageOutline& outline, std::size_t axis) {
    double low = outline.corners[0].at(axis);
    double high = low;
    for (const Document::Point& corner : outline.corners) {
        low = std::min(low, corner.at(axis));
        high = std::max(high, corner.at(axis));
    }
    return high - low;
}

struct Placed {
    std::string path;
    uint16_t depth = 0;
};

std::optional<Placed> PlaceFittingImage(Document::File& file, const std::string& path) {
    REQUIRE(!path.empty());
    const std::optional<Picked> image = LargestFittingImage(file);
    REQUIRE(image.has_value());
    if (!image) return std::nullopt;
    const uint16_t depth = FreeDepth(file, path);
    const auto placed = Document::PlaceImage(
        file, path, image->name,
        Document::DepthSpan{
            .clip = {}, .depth = depth, .first_frame = 0, .last_frame = kLastFrame});
    const std::string place_error = placed.has_value() ? std::string() : placed.error();
    INFO(place_error);
    REQUIRE(placed.has_value());
    if (!placed) return std::nullopt;
    const std::map<uint16_t, std::string> shapes = file.ShapeImages(path);
    CHECK(!shapes.empty());
    const auto named = shapes.find(*placed);
    REQUIRE(named != shapes.end());
    CHECK(named->second == image->name);
    return Placed{.path = path, .depth = depth};
}

void CheckDrawnInsideOutline(const std::string& dir, std::span<const uint8_t> original,
                             const Document::File& file, const Placed& placed,
                             const std::string& animation_name) {
    const auto animation = file.ReadAnimation(placed.path);
    REQUIRE(animation.has_value());
    const auto outlines =
        Document::StageOutlines(*animation, {}, kShownFrame, file.ShapeBounds(placed.path));
    const auto outline = std::ranges::find(outlines, placed.depth, &Document::StageOutline::depth);
    REQUIRE(outline != outlines.end());
    const auto edited = file.Encode();
    REQUIRE(edited.has_value());

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    REQUIRE((*host)->Boot(dir, "iidx33").has_value());
    REQUIRE((*host)->Resize(kViewWidth, kViewHeight).has_value());
    const std::vector<uint8_t> before = RenderPixels(**host, animation_name, original, false);
    const std::vector<uint8_t> after = RenderPixels(**host, animation_name, *edited, true);
    REQUIRE(before.size() == static_cast<std::size_t>(kViewWidth) * kViewHeight * kBgraBytes);
    REQUIRE(after.size() == before.size());

    const Changed changed = Compare(before, after, *outline);
    INFO(std::to_string(changed.inside) + " pixels changed inside the outline, " +
         std::to_string(changed.outside) + " outside");
    CHECK(changed.inside > 0);
    CHECK(changed.outside == 0);
    REQUIRE(changed.area.has_value());
    if (!changed.area) return;
    CHECK(changed.area->right - changed.area->left >= Span(*outline, 0) / 2);
    CHECK(changed.area->bottom - changed.area->top >= Span(*outline, 1) / 2);
}

}

TEST_CASE("A package image placed as a new shape draws inside its outline") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    REQUIRE(file.has_value());
    if (!file) return;
    const std::optional<Placed> placed = PlaceFittingImage(*file, AnimationPath(*file));
    REQUIRE(placed.has_value());
    if (!placed) return;
    CheckDrawnInsideOutline(dir, bytes, *file, *placed, "title");
}

TEST_CASE("A moved scaled and turned image draws inside the outline the document works out") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    REQUIRE(file.has_value());
    if (!file) return;
    const std::optional<Placed> placed = PlaceFittingImage(*file, AnimationPath(*file));
    REQUIRE(placed.has_value());
    if (!placed) return;
    auto animation = file->ReadAnimation(placed->path);
    REQUIRE(animation.has_value());
    const auto tag = Document::LivePlacementTag(animation->root, placed->depth, 0);
    REQUIRE(tag.has_value());
    auto* placement =
        std::get_if<AfpAnimation::Placement>(&animation->root.tags[tag.value_or(0)].body);
    REQUIRE(placement != nullptr);
    placement->flags |= kUseMatrix;
    placement->scale = std::array<int32_t, 2>{1536, 768};
    placement->rotate_skew = std::array<int32_t, 2>{300, -300};
    placement->translation = std::array<int32_t, 2>{6000, 3000};
    placement->origin = std::array<int32_t, 2>{400, 200};
    const auto written = file->WriteAnimation(placed->path, *animation);
    const std::string write_error = written.has_value() ? std::string() : written.error();
    INFO(write_error);
    REQUIRE(written.has_value());
    CheckDrawnInsideOutline(dir, bytes, *file, *placed, "title");
}

TEST_CASE("A new animation loads with its frames and draws an image placed on it") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    const std::vector<uint8_t> bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!bytes.empty());
    auto file = Document::File::Open(bytes);
    REQUIRE(file.has_value());
    if (!file) return;
    const auto path = file->AddAnimation(kNewAnimation, *file, AnimationPath(*file), kNewFrames);
    const std::string add_error = path.has_value() ? std::string() : path.error();
    INFO(add_error);
    REQUIRE(path.has_value());
    if (!path) return;
    const auto empty = file->Encode();
    REQUIRE(empty.has_value());
    if (!empty) return;

    CHECK(LoadedFrames(dir, *empty, kNewAnimation) == kNewFrames);

    const std::optional<Placed> placed = PlaceFittingImage(*file, *path);
    REQUIRE(placed.has_value());
    if (!placed) return;
    CHECK(placed->depth == 1);
    CheckDrawnInsideOutline(dir, *empty, *file, *placed, kNewAnimation);

    const auto removed = file->RemoveAnimation(*path);
    const std::string remove_error = removed.has_value() ? std::string() : removed.error();
    INFO(remove_error);
    REQUIRE(removed.has_value());
    const auto without = file->Encode();
    REQUIRE(without.has_value());
    if (!without) return;
    CHECK(LoadedFrames(dir, *without, "title") == LoadedFrames(dir, bytes, "title"));
}

TEST_CASE("A package with no animation plays its first one, copied from another package") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    const std::vector<uint8_t> title_bytes = ReadAll(dir + "/data/graphic/1/title.ifs");
    auto title = Document::File::Open(title_bytes);
    REQUIRE(title.has_value());
    if (!title) return;

    std::optional<Document::File> plain;
    for (const auto& entry : std::filesystem::directory_iterator(dir + "/data/graphic/1")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        auto candidate = Document::File::Open(ReadAll(entry.path()));
        if (!candidate) continue;
        const bool has_animation = std::ranges::any_of(
            candidate->Nodes(), [](const Document::Node& node) { return node.name == "afp"; });
        const bool has_textures = std::ranges::any_of(
            candidate->Nodes(), [](const Document::Node& node) { return node.name == "tex"; });
        if (has_animation || !has_textures) continue;
        INFO(entry.path().filename().string());
        plain = std::move(*candidate);
        break;
    }
    REQUIRE(plain.has_value());
    if (!plain) return;
    const auto path = plain->AddAnimation(kNewAnimation, *title, AnimationPath(*title), kNewFrames);
    const std::string add_error = path.has_value() ? std::string() : path.error();
    INFO(add_error);
    REQUIRE(path.has_value());
    if (!path) return;
    const auto empty = plain->Encode();
    REQUIRE(empty.has_value());
    if (!empty) return;
    CHECK(LoadedFrames(dir, *empty, kNewAnimation) == kNewFrames);

    const std::optional<Placed> placed = PlaceFittingImage(*plain, *path);
    REQUIRE(placed.has_value());
    if (!placed) return;
    CheckDrawnInsideOutline(dir, *empty, *plain, *placed, kNewAnimation);
}
