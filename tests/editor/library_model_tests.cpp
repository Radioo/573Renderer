#include "editor/library_model.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

Editor::LibraryEntry Entry(const std::string& id, const std::string& name,
                           Editor::LibrarySource s) {
    Editor::LibraryEntry entry;
    entry.id = id;
    entry.name = name;
    entry.build = "iidx11";
    entry.source = s;
    return entry;
}

std::vector<Editor::LibraryEntry> Sample() {
    return {Entry("iidx11-attract", "Attract logo", Editor::LibrarySource::BuiltIn),
            Entry("iidx11-ending", "Ending", Editor::LibrarySource::BuiltIn),
            Entry("attract-remix", "my attract remix", Editor::LibrarySource::User),
            Entry("cube-remix", "cube remix", Editor::LibrarySource::OtherBuild)};
}

}

TEST_CASE("the library groups entries by source in a fixed order", "[editor][library]") {
    const std::vector<Editor::LibraryGroup> groups = Editor::GroupLibrary(Sample(), "");

    REQUIRE(groups.size() == 3);
    CHECK(groups[0].source == Editor::LibrarySource::BuiltIn);
    CHECK(groups[0].label == "Built-in");
    CHECK(groups[0].entries.size() == 2);
    CHECK(groups[1].source == Editor::LibrarySource::User);
    CHECK(groups[1].label == "User");
    CHECK(groups[1].entries.size() == 1);
    CHECK(groups[2].source == Editor::LibrarySource::OtherBuild);
    CHECK(groups[2].label == "Other builds");
    CHECK(groups[2].entries.size() == 1);
}

TEST_CASE("the library filter matches the name and the id, and drops empty groups",
          "[editor][library]") {
    const std::vector<Editor::LibraryGroup> by_name = Editor::GroupLibrary(Sample(), "REMIX");
    REQUIRE(by_name.size() == 2);
    CHECK(by_name[0].source == Editor::LibrarySource::User);
    CHECK(by_name[1].source == Editor::LibrarySource::OtherBuild);

    const std::vector<Editor::LibraryGroup> by_id = Editor::GroupLibrary(Sample(), "iidx11-end");
    REQUIRE(by_id.size() == 1);
    REQUIRE(by_id[0].entries.size() == 1);
    CHECK(by_id[0].entries[0].id == "iidx11-ending");
}

TEST_CASE("an id is a slug of the name", "[editor][library]") {
    CHECK(Editor::Slug("Attract logo") == "attract-logo");
    CHECK(Editor::Slug("  My Attract  Remix!  ") == "my-attract-remix");
    CHECK(Editor::Slug("IIDX 11 / RED: ending") == "iidx-11-red-ending");
    CHECK(Editor::Slug("...") == "preset");
    CHECK(Editor::Slug("") == "preset");
}

TEST_CASE("a generated id is unique against the documents already loaded", "[editor][library]") {
    const std::vector<std::string> taken = {"attract-logo", "attract-logo-2"};
    CHECK(Editor::UniqueId("attract-logo", taken) == "attract-logo-3");
    CHECK(Editor::UniqueId("ending", taken) == "ending");
}

TEST_CASE("duplicating a document appends copy and stays unique", "[editor][library]") {
    std::vector<std::string> taken = {"iidx11-attract"};
    const std::string first = Editor::CopyId("iidx11-attract", taken);
    CHECK(first == "iidx11-attract-copy");
    taken.push_back(first);
    CHECK(Editor::CopyId("iidx11-attract", taken) == "iidx11-attract-copy-2");
}

TEST_CASE("a user document's file sits under its build folder", "[editor][library]") {
    CHECK(Editor::UserRelativePath("iidx11", "attract-remix") == "iidx11/attract-remix.json");
}
