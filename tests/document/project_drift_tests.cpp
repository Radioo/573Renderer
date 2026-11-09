#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/atlas_write.h"
#include "document/project.h"
#include "document/project_drift.h"
#include "document/project_export.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include "sample_package.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUseMatrix = 0x4;

std::string Path() {
    return "afp/" + SamplePackage::HashPath("intro");
}

Document::ImageLoader NoImages() {
    return [](const std::string& file) {
        return Support::Expected<Document::LoadedImage, std::string>(
            Support::Unexpected(file + " was not expected"));
    };
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = SamplePackage::SampleAnimation();
    animation.root.labels = {};
    AfpAnimation::Placement create;
    create.flags = kUseMatrix;
    create.depth = 1;
    create.end_frame = 3;
    create.character = uint16_t{7};
    create.translation = std::array<int32_t, 2>{0, 0};
    animation.root.tags.push_back(AfpAnimation::Tag{create});
    animation.root.frames[0].tag_count = 1;
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

Document::Project Owning(const Document::File& file) {
    const auto animation = file.ReadAnimation(Path());
    REQUIRE(animation.has_value());
    auto owned = Document::OwnDepth(*animation, Document::ClipId{}, Path(), 1, 0);
    const std::string error = owned.has_value() ? std::string() : owned.error();
    INFO(error);
    REQUIRE(owned.has_value());
    return Document::Project{.build = "iidx33",
                             .ifs_path = "intro.ifs",
                             .content = {owned->authored},
                             .images = {},
                             .exported = {}};
}

}

TEST_CASE("Export records a digest for every entry it wrote") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());

    REQUIRE(!project.exported.empty());
    const auto animation =
        std::ranges::find(project.exported, Path(), &Document::ExportedEntry::path);
    REQUIRE(animation != project.exported.end());
    CHECK(animation->digest.size() == 32);
    CHECK(animation->digest == file.EntryDigest(Path()).value_or(""));

    const auto script =
        std::ranges::find(project.exported, "afp/bsi/" + SamplePackage::HashPath("intro"),
                          &Document::ExportedEntry::path);
    CHECK(script != project.exported.end());
}

TEST_CASE("A package nobody touched since the export has drifted in nothing") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    CHECK(Document::ProjectDrift(file, project).empty());
}

TEST_CASE("An entry changed outside the editor is reported by name") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());

    REQUIRE(file.ReplaceEntry("magic", std::vector<uint8_t>{'N', 'G', 'P', 'F', 'X'}).has_value());
    CHECK(Document::ProjectDrift(file, project).empty());

    auto animation = file.ReadAnimation(Path());
    REQUIRE(animation.has_value());
    animation->root.frames.push_back(AfpAnimation::Frame{});
    REQUIRE(file.WriteAnimation(Path(), *animation).has_value());

    const std::vector<Document::DriftedEntry> drift = Document::ProjectDrift(file, project);
    REQUIRE(drift.size() == 2);
    const auto animation_drift = std::ranges::find(drift, Path(), &Document::DriftedEntry::path);
    REQUIRE(animation_drift != drift.end());
    CHECK(animation_drift->kind == Document::DriftKind::Changed);
    const auto script_drift = std::ranges::find(
        drift, "afp/bsi/" + SamplePackage::HashPath("intro"), &Document::DriftedEntry::path);
    REQUIRE(script_drift != drift.end());
    CHECK(script_drift->kind == Document::DriftKind::Changed);
}

TEST_CASE("An entry that is gone is reported as missing, not as changed") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    REQUIRE(file.RemoveEntry(Path()).has_value());

    const std::vector<Document::DriftedEntry> drift = Document::ProjectDrift(file, project);
    REQUIRE(!drift.empty());
    const auto gone = std::ranges::find(drift, Path(), &Document::DriftedEntry::path);
    REQUIRE(gone != drift.end());
    CHECK(gone->kind == Document::DriftKind::Missing);
}

TEST_CASE("Keeping the IFS version detaches the authored content in that entry") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    REQUIRE(project.content.size() == 1);

    Document::KeepIfsVersion(project, Path());
    CHECK(project.content.empty());
    CHECK(std::ranges::find(project.exported, Path(), &Document::ExportedEntry::path) ==
          project.exported.end());
    CHECK(Document::ProjectDrift(file, project).empty());
}

TEST_CASE("Keeping one entry leaves the others owned") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    const std::size_t before = project.exported.size();

    Document::KeepIfsVersion(project, "afp/bsi/" + SamplePackage::HashPath("intro"));
    CHECK(project.exported.size() == before - 1);
    CHECK(project.content.size() == 1);
}

TEST_CASE("What a project exported survives the manifest") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());

    const auto read = Document::ReadProject(Document::WriteProject(project));
    if (!read) FAIL(read.error());
    CHECK(read->exported == project.exported);
    CHECK(Document::ProjectDrift(file, *read).empty());
}

TEST_CASE("An exported list the editor cannot read is refused") {
    const std::string head = R"({"format":1,"build":"b","ifs":"a.ifs","exported":)";
    const auto bytes = [](const std::string& text) {
        return std::vector<uint8_t>(text.begin(), text.end());
    };
    CHECK_FALSE(Document::ReadProject(bytes(head + "7}")).has_value());
    CHECK_FALSE(Document::ReadProject(bytes(head + R"([{"path":"a"}]})")).has_value());
    CHECK_FALSE(Document::ReadProject(bytes(head + R"([{"digest":"a"}]})")).has_value());
    CHECK(Document::ReadProject(bytes(head + R"([{"path":"a","digest":"b"}]})")).has_value());
}

TEST_CASE("What the project writes and the last export did not record is awaiting export") {
    Document::File file = Package();
    Document::Project project = Owning(file);
    CHECK(Document::AwaitingExport(file, project) == 2);
    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    CHECK(Document::AwaitingExport(file, project) == 0);

    auto animation = file.ReadAnimation(Path());
    REQUIRE(animation.has_value());
    animation->root.frames.push_back(AfpAnimation::Frame{});
    REQUIRE(file.WriteAnimation(Path(), *animation).has_value());
    CHECK(Document::AwaitingExport(file, project) == 2);

    REQUIRE(Document::ExportProject(file, project, NoImages()).has_value());
    project.images.push_back(Document::SourceImage{.name = "glow", .file = "sources/glow.png"});
    CHECK(Document::AwaitingExport(file, project) == 2);
}
