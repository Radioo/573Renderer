#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset_golden_format.h"
#include "preset_legacy_view.h"

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <ios>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace PD = Preset::Doc;

constexpr int kAttractFrames = 2456;
constexpr int kEndingFrames = 4385;
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kDriftTolerance = 1.0e-6F;

std::string ReadFile(const std::string& path) {
    const std::ifstream file(path, std::ios::binary);
    if (!file.good()) return {};
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

nlohmann::json AssetLengthsJson() {
    const std::string text = ReadFile(std::string(R573_FIXTURE_DIR) + "/asset_lengths.json");
    REQUIRE_FALSE(text.empty());
    nlohmann::json doc = nlohmann::json::parse(text, nullptr, false);
    REQUIRE_FALSE(doc.is_discarded());
    return doc;
}

Preset::AssetLengths AssetLengths() {
    const nlohmann::json doc = AssetLengthsJson();
    Preset::AssetLengths lengths;
    for (const auto& [dir, ticks] : doc["scene3d"].items())
        lengths.scene_ticks[dir] = ticks.get<float>();
    for (const auto& [dir, animations] : doc["package2d"].items()) {
        for (const auto& [name, frames] : animations.items())
            lengths.animation_frames[dir][name] = frames.get<int>();
    }
    return lengths;
}

const std::map<std::string, PresetLegacy::Compat>& Compat() {
    static const std::map<std::string, PresetLegacy::Compat> loaded = [] {
        const std::string text = ReadFile(std::string(R573_FIXTURE_DIR) + "/legacy_compat.json");
        std::string err;
        std::map<std::string, PresetLegacy::Compat> map = PresetLegacy::LoadCompat(text, err);
        return map;
    }();
    return loaded;
}

const PresetLegacy::Compat& CompatFor(const std::string& id) {
    const auto found = Compat().find(id);
    INFO("legacy compat for " << id);
    REQUIRE(found != Compat().end());
    return found->second;
}

std::vector<PD::Document> AllDocuments() {
    return PD::BuiltIns();
}

const PD::ModelDraw* LeadDraw(const PD::Document& document, const std::string& model) {
    for (const PD::Track& track : document.tracks) {
        for (const PD::Clip& clip : track.clips) {
            const auto* draw = std::get_if<PD::ModelDraw>(&clip.command);
            if (draw == nullptr) continue;
            const std::string& name = draw->model.empty() ? track.target : draw->model;
            if (name == model) return draw;
        }
    }
    return nullptr;
}

std::string AssetDir(const PD::Document& document, const std::string& id) {
    for (const PD::Asset& asset : document.assets) {
        if (asset.id == id) return asset.dir;
    }
    return {};
}

int ClipFrames(const PD::Document& document, const PresetLegacy::Compat& compat,
               const nlohmann::json& assets) {
    const PD::ModelDraw* draw = LeadDraw(document, compat.lead);
    REQUIRE(draw != nullptr);
    const std::string dir = AssetDir(document, draw->asset);
    INFO("scene3d asset " << dir);
    REQUIRE(assets["scene3d"].contains(dir));
    const auto ticks = assets["scene3d"][dir].get<float>();
    const auto speed = (float)draw->anim_speed;
    REQUIRE(speed > 0.0F);
    return (int)std::lround(ticks / speed);
}

int ExpectedFrames(const PD::Document& document, const PresetLegacy::Compat& compat,
                   const nlohmann::json& assets) {
    if (compat.countdown.start_frames > 0) return compat.countdown.start_frames;
    if (document.id == "iidx11-attract") return kAttractFrames;
    if (document.id == "iidx11-ending") return kEndingFrames;
    return ClipFrames(document, compat, assets);
}

std::vector<int> ChoicesOf(const PD::Document& document) {
    if (document.options.empty()) return {-1};
    const std::size_t count = document.options.front().choices.size();
    std::vector<int> choices;
    choices.reserve(count);
    for (std::size_t i = 0; i < count; i++)
        choices.push_back((int)i);
    return choices;
}

int HiddenModelFrames(const PD::Document& document) {
    return document.id == "iidx11-attract" ? 610 : 0;
}

PresetGolden::Recording LoadFixture(const std::string& preset, int choice) {
    const std::string name = PresetGolden::FixtureName(preset, choice);
    const std::string text = ReadFile(std::string(R573_FIXTURE_DIR) + "/golden/" + name);
    INFO("fixture " << name);
    REQUIRE_FALSE(text.empty());
    PresetGolden::Recording record;
    std::string err;
    REQUIRE(PresetGolden::Parse(text, record, err));
    return record;
}

std::string HashAt(const std::vector<std::string>& hashes, int frame) {
    if (frame < 0 || (std::size_t)frame >= hashes.size()) return {};
    return hashes[(std::size_t)frame];
}

std::string Join(const std::vector<std::string>& calls) {
    std::string out;
    for (const std::string& call : calls) {
        out += "\n    ";
        out += call;
    }
    return out.empty() ? std::string("\n    (no pushes)") : out;
}

struct Replay {
    int first_bad = -1;
    int detail_bad = -1;
    int excluded = 0;
    float drift = 0.0F;
    std::vector<std::string> expected;
    std::vector<std::string> got;
};

std::string HashOf(const std::vector<std::string>& calls, bool excluded) {
    if (!excluded) return PresetGolden::HashFrame(calls);
    return PresetGolden::HashFrame(PresetGolden::WithoutModelTransforms(calls));
}

bool CompareDetail(const PresetLegacy::Adapter& adapter, const PresetGolden::Recording& fixture,
                   const std::vector<std::string>& calls, int frame, bool excluded,
                   Replay& result) {
    const auto detail = fixture.detail.find(frame);
    if (detail == fixture.detail.end()) return true;
    const std::vector<std::string> expected =
        excluded ? adapter.StripHidden(detail->second, frame) : detail->second;
    if (expected != calls) {
        result.detail_bad = frame;
        result.expected = expected;
        result.got = calls;
        return false;
    }
    return HashOf(detail->second, true) == HashAt(fixture.hashes_no_transform, frame);
}

Replay ReplayOne(const PD::Document& source, int choice, const PresetGolden::Recording& fixture,
                 const Preset::AssetLengths& lengths) {
    Replay result;
    auto document = std::make_shared<PD::Document>(source);
    Preset::Eval::Evaluator evaluator;
    evaluator.Load(document, lengths);
    if (choice >= 0) evaluator.SetOption(0, choice);
    const PresetLegacy::Adapter adapter(CompatFor(source.id));

    for (int frame = 0; frame < fixture.frames; frame++) {
        const std::vector<Preset::Eval::Push> pushes = evaluator.RenderFrame(kFrameSeconds);
        const PresetLegacy::FrameCalls filtered = adapter.Filter(pushes, frame);
        if (frame + 1 < fixture.frames)
            result.drift = std::max(result.drift, filtered.countdown_drift);
        const bool excluded = adapter.Excluded(frame);
        if (excluded) result.excluded++;
        const bool detail_ok =
            CompareDetail(adapter, fixture, filtered.calls, frame, excluded, result);
        const std::string want =
            excluded ? HashAt(fixture.hashes_no_transform, frame) : HashAt(fixture.hashes, frame);
        if ((!detail_ok || HashOf(filtered.calls, excluded) != want) && result.first_bad < 0) {
            result.first_bad = frame;
            if (result.got.empty()) result.got = filtered.calls;
        }
        if (result.detail_bad == frame) return result;
    }
    return result;
}

}

TEST_CASE("the frozen legacy view covers every built-in preset", "[golden]") {
    std::string err;
    const std::string text = ReadFile(std::string(R573_FIXTURE_DIR) + "/legacy_compat.json");
    const std::map<std::string, PresetLegacy::Compat> compat = PresetLegacy::LoadCompat(text, err);
    INFO(err);
    REQUIRE(err.empty());
    REQUIRE(compat.size() == 18);
    for (const PD::Document& document : AllDocuments()) {
        INFO(document.id);
        const auto found = compat.find(document.id);
        REQUIRE(found != compat.end());
        CHECK_FALSE(found->second.lead.empty());
        REQUIRE_FALSE(found->second.phase_starts.empty());
        REQUIRE(LeadDraw(document, found->second.lead) != nullptr);
        if (document.markers.empty()) {
            CHECK(found->second.phase_starts == std::vector<int>{0});
            continue;
        }
        REQUIRE(found->second.phase_starts.size() == document.markers.size());
        for (std::size_t i = 0; i < document.markers.size(); i++)
            CHECK(found->second.phase_starts[i] == document.markers[i].frame);
    }
}

TEST_CASE("the legacy golden fixture set covers every built-in preset", "[golden]") {
    const nlohmann::json assets = AssetLengthsJson();
    const std::vector<PD::Document> documents = AllDocuments();
    REQUIRE(documents.size() == 18);

    std::set<std::string> presets;
    int fixtures = 0;
    for (const PD::Document& document : documents) {
        presets.insert(document.id);
        for (const int choice : ChoicesOf(document)) {
            const PresetGolden::Recording record = LoadFixture(document.id, choice);
            INFO(document.id << " choice " << choice);
            CHECK(record.preset == document.id);
            CHECK(record.build == document.build);
            CHECK(record.choice == choice);
            CHECK(record.frames == ExpectedFrames(document, CompatFor(document.id), assets));
            CHECK(record.hashes.size() == (std::size_t)record.frames);
            CHECK(record.hashes_no_transform.size() == (std::size_t)record.frames);
            CHECK_FALSE(record.setup.empty());
            CHECK(record.detail.contains(0));
            fixtures++;
        }
    }
    CHECK(presets.size() == 18);
    CHECK(fixtures == 40);
}

TEST_CASE("the golden frame hash is stable and order sensitive", "[golden]") {
    const std::vector<std::string> calls = {"Scene3dHost::SetModelAlpha('core', 0.5)",
                                            "Scene3dHost::RenderFrame(0.0166666675)"};
    const std::vector<std::string> swapped = {calls[1], calls[0]};
    CHECK(PresetGolden::HashFrame(calls) == PresetGolden::HashFrame(calls));
    CHECK(PresetGolden::HashFrame(calls) != PresetGolden::HashFrame(swapped));
    CHECK(PresetGolden::HashFrame({}) != PresetGolden::HashFrame(calls));
}

TEST_CASE("the default documents reproduce the legacy golden recording", "[golden]") {
    const Preset::AssetLengths lengths = AssetLengths();
    for (const PD::Document& document : AllDocuments()) {
        for (const int choice : ChoicesOf(document)) {
            const PresetGolden::Recording fixture = LoadFixture(document.id, choice);
            INFO(document.id << " choice " << choice);
            const std::vector<PD::Problem> problems = PD::Validate(document);
            for (const PD::Problem& problem : problems) {
                if (problem.severity != PD::Severity::Error) continue;
                FAIL(document.id << ": built-in document error on " << problem.path << ": "
                                 << problem.message);
            }
            REQUIRE(document.length.has_value());
            CHECK(*document.length == fixture.frames);

            const Replay replay = ReplayOne(document, choice, fixture, lengths);
            CHECK(replay.drift <= kDriftTolerance);
            CHECK(replay.excluded == HiddenModelFrames(document));
            if (replay.first_bad < 0) continue;
            FAIL_CHECK(document.id << " choice " << choice << ": first difference at frame "
                                   << replay.first_bad << ", first full push list at frame "
                                   << replay.detail_bad << "\n  legacy:" << Join(replay.expected)
                                   << "\n  evaluator:" << Join(replay.got));
        }
    }
}
