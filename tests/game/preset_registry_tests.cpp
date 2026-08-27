#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_registry.h"
#include "preset/doc/preset_validate.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace {

namespace PD = Preset::Doc;

std::filesystem::path TempRoot(const std::string& name) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("r573_presets_" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(text.data(), (std::streamsize)text.size());
}

PD::Document UserDocument(const std::string& id) {
    PD::Document document;
    document.id = id;
    document.name = "User " + id;
    document.build = "iidx11";
    document.length = 120;
    return document;
}

const PD::Entry* EntryFor(const std::vector<const PD::Entry*>& entries, const std::string& id) {
    for (const PD::Entry* entry : entries) {
        if (entry->document.id == id) return entry;
    }
    return nullptr;
}

int ErrorCount(const PD::Entry& entry) {
    int errors = 0;
    for (const PD::Problem& problem : entry.problems)
        errors += (problem.severity == PD::Severity::Error) ? 1 : 0;
    return errors;
}

}

TEST_CASE("the registry lists every built-in document under its own build") {
    PD::Registry registry;
    registry.Load(TempRoot("empty"));

    const std::vector<const PD::Entry*> iidx10 = registry.ForBuild("iidx10");
    const std::vector<const PD::Entry*> iidx11 = registry.ForBuild("iidx11");
    const std::vector<const PD::Entry*> iidx12 = registry.ForBuild("iidx12");
    CHECK(iidx10.size() + iidx11.size() + iidx12.size() == PD::BuiltIns().size());
    CHECK(iidx12.size() == 7);
    CHECK(registry.ForBuild("iidx13").empty());
    for (const PD::Entry* entry : iidx10) {
        INFO(entry->document.id);
        CHECK(entry->builtin);
        CHECK(entry->path.empty());
        CHECK(entry->document.build == "iidx10");
        CHECK(ErrorCount(*entry) == 0);
    }
    REQUIRE(EntryFor(iidx11, "iidx11-attract") != nullptr);
    REQUIRE(EntryFor(iidx11, "iidx11-ending") != nullptr);
    REQUIRE(registry.Find("iidx11", "iidx11-attract") != nullptr);
    REQUIRE(registry.Find("iidx10", "iidx11-attract") == nullptr);
    REQUIRE(registry.Find("iidx11", "no-such-preset") == nullptr);
}

TEST_CASE("the registry lists user documents with their problems instead of dropping them") {
    const std::filesystem::path root = TempRoot("user");
    WriteFile(root / "iidx11" / "my-screen.json", PD::Save(UserDocument("my-screen")));
    WriteFile(root / "iidx11" / "broken.json", "{ not json");
    WriteFile(root / "iidx11" / "iidx11-attract.json", PD::Save(UserDocument("iidx11-attract")));

    std::vector<PD::ScanStatus> reported;
    PD::Registry registry;
    registry.Load(root, [&reported](const PD::ScanStatus& status) { reported.push_back(status); });

    const std::vector<const PD::Entry*> entries = registry.ForBuild("iidx11");
    const PD::Entry* mine = EntryFor(entries, "my-screen");
    REQUIRE(mine != nullptr);
    CHECK_FALSE(mine->builtin);
    CHECK(mine->path.ends_with("my-screen.json"));
    CHECK(ErrorCount(*mine) == 0);

    int with_the_builtin_id = 0;
    for (const PD::Entry* entry : entries)
        with_the_builtin_id += (entry->document.id == "iidx11-attract") ? 1 : 0;
    CHECK(with_the_builtin_id == 1);
    const PD::Entry* clash = EntryFor(entries, "iidx11-attract");
    REQUIRE(clash != nullptr);
    CHECK(clash->builtin);
    const std::vector<const PD::Entry*> broken = registry.Problems();
    REQUIRE(broken.size() == 2);
    bool saw_parse_error = false;
    bool saw_clash = false;
    for (const PD::Entry* entry : broken) {
        INFO(entry->path);
        CHECK_FALSE(entry->builtin);
        REQUIRE(ErrorCount(*entry) > 0);
        for (const PD::Problem& problem : entry->problems) {
            saw_parse_error = saw_parse_error || problem.message.find("parse") != std::string::npos;
            saw_clash = saw_clash || problem.message.find("built-in") != std::string::npos;
        }
    }
    CHECK(saw_parse_error);
    CHECK(saw_clash);

    REQUIRE_FALSE(reported.empty());
    CHECK(reported.back().done == 3);
    CHECK(reported.back().total == 3);
    bool named_a_file = false;
    for (const PD::ScanStatus& status : reported)
        named_a_file = named_a_file || status.current.ends_with("my-screen.json");
    CHECK(named_a_file);
}

TEST_CASE("the registry resolves one document per id when two user files claim the same one") {
    const std::filesystem::path root = TempRoot("duplicate");
    WriteFile(root / "iidx11" / "first.json", PD::Save(UserDocument("shared-id")));
    WriteFile(root / "iidx11" / "second.json", PD::Save(UserDocument("shared-id")));

    PD::Registry registry;
    registry.Load(root);

    int with_the_id = 0;
    for (const PD::Entry* entry : registry.ForBuild("iidx11"))
        with_the_id += (entry->document.id == "shared-id") ? 1 : 0;
    CHECK(with_the_id == 1);
    REQUIRE(registry.Find("iidx11", "shared-id") != nullptr);

    const std::vector<const PD::Entry*> broken = registry.Problems();
    REQUIRE(broken.size() == 1);
    CHECK(ErrorCount(*broken.front()) > 0);
}

TEST_CASE("the registry keeps user documents of another build out of this build's list") {
    const std::filesystem::path root = TempRoot("builds");
    PD::Document other = UserDocument("other-build");
    other.build = "iidx10";
    WriteFile(root / "iidx10" / "other-build.json", PD::Save(other));

    PD::Registry registry;
    registry.Load(root);
    CHECK(EntryFor(registry.ForBuild("iidx10"), "other-build") != nullptr);
    CHECK(EntryFor(registry.ForBuild("iidx11"), "other-build") == nullptr);
}
