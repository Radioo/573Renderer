#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "preset/preset_tools.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace PD = Preset::Doc;

const PD::Document* Find(const std::vector<PD::Document>& documents, std::string_view id) {
    for (const PD::Document& document : documents) {
        if (document.id == id) return &document;
    }
    return nullptr;
}

std::string Describe(const std::vector<PD::Problem>& problems) {
    std::string out;
    for (const PD::Problem& problem : problems) {
        if (problem.severity != PD::Severity::Error) continue;
        out += "\n  error " + problem.path + ": " + problem.message;
    }
    return out;
}

}

TEST_CASE("every built-in document round trips through JSON byte for byte") {
    for (const PD::Document& document : PD::BuiltIns()) {
        const std::string text = PD::Save(document);
        const PD::Loaded loaded = PD::Load(text);
        INFO(document.id);
        REQUIRE(loaded.has_value());
        REQUIRE(*loaded == document);
        REQUIRE(PD::Save(*loaded) == text);
    }
}

TEST_CASE("the attract built-in carries the five phase markers") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* attract = Find(built_ins, "iidx11-attract");
    REQUIRE(attract != nullptr);
    REQUIRE(attract->markers.size() == 5);
    CHECK(attract->markers[0].frame == 0);
    CHECK(attract->markers[1].frame == 502);
    CHECK(attract->markers[2].frame == 793);
    CHECK(attract->markers[3].frame == 902);
    CHECK(attract->markers[4].frame == 1736);
    REQUIRE(attract->length.has_value());
    CHECK(*attract->length == 2456);
}

TEST_CASE("built-in documents carry unique ids and validate without an error") {
    std::set<std::string> seen;
    for (const PD::Document& document : PD::BuiltIns()) {
        INFO(document.id);
        REQUIRE_FALSE(document.id.empty());
        REQUIRE_FALSE(document.build.empty());
        REQUIRE(seen.insert(document.build + "/" + document.id).second);
        const std::vector<PD::Problem> problems = PD::Validate(document);
        INFO(Describe(problems));
        bool failed = false;
        for (const PD::Problem& problem : problems)
            failed = failed || problem.severity == PD::Severity::Error;
        REQUIRE_FALSE(failed);
    }
}

TEST_CASE("the defaults dump writes every built-in document with no window or device") {
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "r573_dump_defaults_test";
    std::filesystem::remove_all(out);
    REQUIRE(PresetTools::DumpDefaults(out.string()) == 0);

    std::size_t written = 0;
    for (const std::filesystem::directory_entry& build : std::filesystem::directory_iterator(out)) {
        REQUIRE(build.is_directory());
        for (const std::filesystem::directory_entry& file :
             std::filesystem::directory_iterator(build.path())) {
            INFO(file.path().string());
            const std::ifstream stream(file.path(), std::ios::binary);
            REQUIRE(stream.good());
            std::ostringstream text;
            text << stream.rdbuf();
            const PD::Loaded loaded = PD::Load(text.str());
            REQUIRE(loaded.has_value());
            CHECK(loaded->build == build.path().filename().string());
            CHECK(loaded->id == file.path().stem().string());
            written++;
        }
    }
    CHECK(written == PD::BuiltIns().size());
    std::filesystem::remove_all(out);
}

TEST_CASE("the defaults dump refuses to write into an unnamed directory") {
    CHECK(PresetTools::DumpDefaults("") == 2);
}

TEST_CASE("the ending built-in merges both halves into 18 markers and unique clip ids") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx11-ending");
    REQUIRE(ending != nullptr);
    REQUIRE(ending->markers.size() == 18);
    int previous = -1;
    for (const PD::Marker& marker : ending->markers) {
        INFO(marker.label);
        REQUIRE(marker.frame > previous);
        previous = marker.frame;
    }
    std::set<std::string> clip_ids;
    std::set<std::string> track_ids;
    for (const PD::Track& track : ending->tracks) {
        INFO(track.id);
        REQUIRE(track_ids.insert(track.id).second);
        for (const PD::Clip& clip : track.clips) {
            INFO(clip.id);
            REQUIRE(clip_ids.insert(clip.id).second);
        }
    }
}
