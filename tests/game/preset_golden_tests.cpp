#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset_golden_format.h"
#include "preset_legacy_view.h"

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_convert.h"
#include "preset/scene_preset.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <ios>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

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

int ClipFrames(const Preset::Scene& scene, const nlohmann::json& assets) {
    REQUIRE_FALSE(scene.models.empty());
    const std::string dir(scene.models.front().scene_dir);
    INFO("scene3d asset " << dir);
    REQUIRE(assets["scene3d"].contains(dir));
    const auto ticks = assets["scene3d"][dir].get<float>();
    const float speed = scene.models.front().anim_speed;
    REQUIRE(speed > 0.0F);
    return (int)std::lround(ticks / speed);
}

int ExpectedFrames(const Preset::Scene& scene, const nlohmann::json& assets) {
    if (scene.countdown.start_frames > 0) return scene.countdown.start_frames;
    if (scene.id == "iidx11-attract") return kAttractFrames;
    if (scene.id == "iidx11-ending") return kEndingFrames;
    return ClipFrames(scene, assets);
}

std::vector<const Preset::Scene*> AllScenes() {
    std::vector<const Preset::Scene*> scenes;
    for (const std::string_view build : {"iidx10", "iidx11"}) {
        for (const Preset::Scene* scene : Preset::ForBuild(build))
            scenes.push_back(scene);
    }
    return scenes;
}

std::vector<int> ChoicesOf(const Preset::Scene& scene) {
    if (scene.options.empty()) return {-1};
    const std::size_t count = scene.options.front().choices.size();
    std::vector<int> choices;
    choices.reserve(count);
    for (std::size_t i = 0; i < count; i++)
        choices.push_back((int)i);
    return choices;
}

int HiddenModelFrames(const Preset::Scene& scene) {
    return scene.id == "iidx11-attract" ? 610 : 0;
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

Replay ReplayOne(const Preset::Scene& scene, int choice, const PresetGolden::Recording& fixture,
                 const Preset::AssetLengths& lengths) {
    Replay result;
    auto document = std::make_shared<Preset::Doc::Document>(Preset::FromScene(scene, lengths));
    Preset::Eval::Evaluator evaluator;
    evaluator.Load(document, lengths);
    if (choice >= 0) evaluator.SetOption(0, choice);
    const PresetLegacy::Adapter adapter(scene);

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

TEST_CASE("the legacy golden fixture set covers every built-in preset", "[golden]") {
    const nlohmann::json assets = AssetLengthsJson();
    const std::vector<const Preset::Scene*> scenes = AllScenes();
    REQUIRE(scenes.size() == 18);

    std::set<std::string> presets;
    int fixtures = 0;
    for (const Preset::Scene* scene : scenes) {
        presets.insert(std::string(scene->id));
        for (const int choice : ChoicesOf(*scene)) {
            const PresetGolden::Recording record = LoadFixture(std::string(scene->id), choice);
            INFO(scene->id << " choice " << choice);
            CHECK(record.preset == scene->id);
            CHECK(record.build == scene->build);
            CHECK(record.choice == choice);
            CHECK(record.frames == ExpectedFrames(*scene, assets));
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

TEST_CASE("the new evaluator reproduces the legacy golden recording", "[golden]") {
    const Preset::AssetLengths lengths = AssetLengths();
    for (const Preset::Scene* scene : AllScenes()) {
        for (const int choice : ChoicesOf(*scene)) {
            const PresetGolden::Recording fixture = LoadFixture(std::string(scene->id), choice);
            const Preset::Doc::Document document = Preset::FromScene(*scene, lengths);
            INFO(scene->id << " choice " << choice);
            const std::vector<Preset::Doc::Problem> problems = Preset::Doc::Validate(document);
            for (const Preset::Doc::Problem& problem : problems) {
                if (problem.severity != Preset::Doc::Severity::Error) continue;
                FAIL(scene->id << ": converted document error on " << problem.path << ": "
                               << problem.message);
            }
            REQUIRE(document.length.has_value());
            CHECK(*document.length == fixture.frames);

            const Replay replay = ReplayOne(*scene, choice, fixture, lengths);
            CHECK(replay.drift <= kDriftTolerance);
            CHECK(replay.excluded == HiddenModelFrames(*scene));
            if (replay.first_bad < 0) continue;
            FAIL_CHECK(scene->id << " choice " << choice << ": first difference at frame "
                                 << replay.first_bad << ", first full push list at frame "
                                 << replay.detail_bad << "\n  legacy:" << Join(replay.expected)
                                 << "\n  evaluator:" << Join(replay.got));
        }
    }
}
