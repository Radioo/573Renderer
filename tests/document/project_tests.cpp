#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/keyframes.h"
#include "document/project.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> Bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::string Text(const std::vector<uint8_t>& bytes) {
    return {bytes.begin(), bytes.end()};
}

}

TEST_CASE("A project round trips through its manifest") {
    const Document::Project project{.build = "iidx33",
                                    .ifs_path = "graphic/title.ifs",
                                    .content = {},
                                    .images = {},
                                    .exported = {}};
    const std::vector<uint8_t> manifest = Document::WriteProject(project);

    const auto read = Document::ReadProject(manifest);
    if (!read) FAIL(read.error());
    CHECK(*read == project);
}

TEST_CASE("Writing the same project twice produces the same bytes") {
    const Document::Project project{
        .build = "iidx33", .ifs_path = "../title.ifs", .content = {}, .images = {}, .exported = {}};
    CHECK(Document::WriteProject(project) == Document::WriteProject(project));

    const auto read = Document::ReadProject(Document::WriteProject(project));
    REQUIRE(read.has_value());
    CHECK(Document::WriteProject(*read) == Document::WriteProject(project));
}

TEST_CASE("A manifest the editor cannot read is refused rather than guessed at") {
    CHECK_FALSE(Document::ReadProject(Bytes("")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes("not json")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes("[]")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes(R"({"build":"iidx33","ifs":"a.ifs"})")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes(R"({"format":1,"ifs":"a.ifs"})")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes(R"({"format":1,"build":"iidx33"})")).has_value());
    CHECK_FALSE(
        Document::ReadProject(Bytes(R"({"format":1,"build":"iidx33","ifs":""})")).has_value());
    CHECK_FALSE(
        Document::ReadProject(Bytes(R"({"format":1,"build":7,"ifs":"a.ifs"})")).has_value());
}

TEST_CASE("A project written in another format says so instead of loading") {
    const auto ahead =
        Document::ReadProject(Bytes(R"({"format":99,"build":"iidx33","ifs":"a.ifs"})"));
    REQUIRE_FALSE(ahead.has_value());
    CHECK(ahead.error().find("99") != std::string::npos);
    CHECK(ahead.error().find(std::to_string(Document::kProjectFormat)) != std::string::npos);
}

TEST_CASE("The manifest is text a person can read and edit") {
    const std::string text = Text(Document::WriteProject({.build = "iidx33",
                                                          .ifs_path = "graphic/title.ifs",
                                                          .content = {},
                                                          .images = {},
                                                          .exported = {}}));
    CHECK(text.find("\"format\": 1") != std::string::npos);
    CHECK(text.find("\"build\": \"iidx33\"") != std::string::npos);
    CHECK(text.find("\"ifs\": \"graphic/title.ifs\"") != std::string::npos);
    CHECK(text.back() == '\n');
}

TEST_CASE("An IFS beside or under the project folder is stored as a relative path") {
    CHECK(Document::StoredIfsPath("C:/work/title", "C:/work/title/title.ifs") == "title.ifs");
    CHECK(Document::StoredIfsPath("C:/work/title", "C:/work/title.ifs") == "../title.ifs");
    CHECK(Document::StoredIfsPath("C:/work/title", "C:/work/data/graphic/title.ifs") ==
          "../data/graphic/title.ifs");
}

TEST_CASE("An IFS with no relative path to the project folder is stored whole") {
    CHECK(Document::StoredIfsPath("C:/work/title", "F:/game/data/title.ifs") ==
          "F:/game/data/title.ifs");
}

TEST_CASE("A stored path resolves back to the IFS it came from") {
    const std::string folder = "C:/work/title";
    for (const std::string& ifs :
         {std::string("C:/work/title/title.ifs"), std::string("C:/work/title.ifs"),
          std::string("F:/game/data/title.ifs")}) {
        const Document::Project project{.build = "iidx33",
                                        .ifs_path = Document::StoredIfsPath(folder, ifs),
                                        .content = {},
                                        .images = {},
                                        .exported = {}};
        CHECK(Document::ResolvedIfsPath(folder, project) == ifs);
    }
}

TEST_CASE("A project moved with its IFS still resolves") {
    const Document::Project project{.build = "iidx33",
                                    .ifs_path = "../data/title.ifs",
                                    .content = {},
                                    .images = {},
                                    .exported = {}};
    CHECK(Document::ResolvedIfsPath("D:/elsewhere/title", project) ==
          "D:/elsewhere/data/title.ifs");
}

TEST_CASE("The manifest sits in the project folder under a fixed name") {
    CHECK(Document::ProjectManifestPath("C:/work/title") == "C:/work/title/project.json");
    CHECK(Document::ProjectManifestPath("C:/work/title/") == "C:/work/title/project.json");
}

namespace {

Document::AuthoredDepth Owned() {
    return Document::AuthoredDepth{
        .animation = "afp/1a2b",
        .depth = 12,
        .first_frame = 4,
        .last_frame = 40,
        .tracks = {Document::Track{
                       .property = "Translation",
                       .keys = {Document::Keyframe{
                                    .frame = 4,
                                    .value = {1000, -2000},
                                    .ease = Document::Ease::Bezier,
                                    .bezier = {.x1 = 0.42, .y1 = 0.0, .x2 = 0.58, .y2 = 1.0}},
                                Document::Keyframe{.frame = 40,
                                                   .value = {3000, -2000},
                                                   .ease = Document::Ease::Hold,
                                                   .bezier = {}}}},
                   Document::Track{.property = "Packed multiply colour",
                                   .keys = {Document::Keyframe{.frame = 4,
                                                               .value = {4294967286},
                                                               .ease = Document::Ease::Linear,
                                                               .bezier = {}},
                                            Document::Keyframe{.frame = 9,
                                                               .value = {0},
                                                               .ease = Document::Ease::Linear,
                                                               .bezier = {}}}}},
        .script = std::nullopt};
}

}

TEST_CASE("What a project owns survives the manifest") {
    const Document::Project project{.build = "iidx33",
                                    .ifs_path = "title.ifs",
                                    .content = {Owned()},
                                    .images = {},
                                    .exported = {}};
    const auto read = Document::ReadProject(Document::WriteProject(project));
    if (!read) FAIL(read.error());
    CHECK(*read == project);
    CHECK(Document::WriteProject(*read) == Document::WriteProject(project));
}

TEST_CASE("A manifest with no owned depths reads as owning nothing") {
    const auto read =
        Document::ReadProject(Bytes(R"({"format":1,"build":"iidx33","ifs":"a.ifs"})"));
    REQUIRE(read.has_value());
    CHECK(read->content.empty());
}

TEST_CASE("Owned depths the editor cannot make sense of are refused") {
    const std::string head = R"({"format":1,"build":"b","ifs":"a.ifs","owns":)";
    CHECK_FALSE(Document::ReadProject(Bytes(head + "7}")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes(head + "[7]}")).has_value());
    CHECK_FALSE(Document::ReadProject(Bytes(head + R"([{"depth":1}]})")).has_value());
    CHECK_FALSE(
        Document::ReadProject(Bytes(head + R"([{"animation":"a","depth":1,"first":5,"last":2,)"
                                           R"("tracks":[]}]})"))
            .has_value());
    CHECK_FALSE(Document::ReadProject(
                    Bytes(head + R"([{"animation":"a","depth":1,"first":0,"last":2,"tracks":)"
                                 R"([{"property":"Translation","keys":[{"frame":0,"value":[1],)"
                                 R"("ease":"spring"}]}]}]})"))
                    .has_value());
    CHECK_FALSE(Document::ReadProject(
                    Bytes(head + R"([{"animation":"a","depth":1,"first":0,"last":2,"tracks":)"
                                 R"([{"property":"Translation","keys":[{"frame":0,"value":[1,2],)"
                                 R"("ease":"hold"},{"frame":1,"value":[1],"ease":"hold"}]}]}]})"))
                    .has_value());
}

TEST_CASE("A keyframe with no bezier keeps none in the manifest") {
    Document::AuthoredDepth depth = Owned();
    depth.tracks.front().keys.front().ease = Document::Ease::Linear;
    const Document::Project project{.build = "iidx33",
                                    .ifs_path = "title.ifs",
                                    .content = {depth},
                                    .images = {},
                                    .exported = {}};
    const std::string text = Text(Document::WriteProject(project));
    CHECK(text.find("\"curve\"") == std::string::npos);

    const auto read = Document::ReadProject(Document::WriteProject(project));
    REQUIRE(read.has_value());
    CHECK(read->content.front().tracks.front().keys.front().bezier == Document::Bezier{});
}
