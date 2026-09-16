#include <catch2/catch_test_macros.hpp>

#include "document/project.h"

#include <cstdint>
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
    const Document::Project project{.build = "iidx33", .ifs_path = "graphic/title.ifs"};
    const std::vector<uint8_t> manifest = Document::WriteProject(project);

    const auto read = Document::ReadProject(manifest);
    if (!read) FAIL(read.error());
    CHECK(*read == project);
}

TEST_CASE("Writing the same project twice produces the same bytes") {
    const Document::Project project{.build = "iidx33", .ifs_path = "../title.ifs"};
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
    const std::string text =
        Text(Document::WriteProject({.build = "iidx33", .ifs_path = "graphic/title.ifs"}));
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
                                        .ifs_path = Document::StoredIfsPath(folder, ifs)};
        CHECK(Document::ResolvedIfsPath(folder, project) == ifs);
    }
}

TEST_CASE("A project moved with its IFS still resolves") {
    const Document::Project project{.build = "iidx33", .ifs_path = "../data/title.ifs"};
    CHECK(Document::ResolvedIfsPath("D:/elsewhere/title", project) ==
          "D:/elsewhere/data/title.ifs");
}

TEST_CASE("The manifest sits in the project folder under a fixed name") {
    CHECK(Document::ProjectManifestPath("C:/work/title") == "C:/work/title/project.json");
    CHECK(Document::ProjectManifestPath("C:/work/title/") == "C:/work/title/project.json");
}
