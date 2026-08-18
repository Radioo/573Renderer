#include "editor/frame_inspector_model.h"

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_report.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

Doc::Document ConflictingDocument() {
    Doc::Document document;
    document.id = "frame-inspector";
    document.name = "Frame inspector";
    document.build = "iidx11";
    document.length = 600;

    Doc::Clip draw;
    draw.id = "core_draw";
    draw.start = 0;
    draw.end = 600;
    draw.command = Doc::ModelDraw{.asset = "scene", .model = "core", .alpha = 1.0};

    Doc::Clip fade;
    fade.id = "core_fade";
    fade.start = 100;
    fade.end = 200;
    fade.command = Doc::ModelTween{};
    fade.keys.push_back(Doc::Key{.at = 0, .values = {Doc::KeyValue{.id = "alpha", .value = 0.25}}});

    Doc::Track track;
    track.id = "core";
    track.name = "core";
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    track.clips.push_back(std::move(draw));
    track.clips.push_back(std::move(fade));
    document.tracks.push_back(std::move(track));
    return document;
}

const Editor::FrameRow* Find(const std::vector<Editor::FrameRow>& rows, const std::string& entity,
                             const std::string& label) {
    const auto it = std::ranges::find_if(rows, [&](const Editor::FrameRow& row) {
        return row.entity == entity && row.label == label && !row.header;
    });
    return it == rows.end() ? nullptr : &*it;
}

}

TEST_CASE("the frame inspector names the clip that won a conflicting value",
          "[editor][frame_inspector]") {
    const Doc::Document document = ConflictingDocument();
    const std::vector<int> choices;
    const Preset::Eval::EvalState state;

    const Preset::Eval::FrameState before = Preset::Eval::ResolveFrame(
        Preset::Eval::ResolveInput{.document = &document, .choices = &choices}, 50);
    const std::vector<Editor::FrameRow> before_rows =
        Editor::FrameRows(Preset::Eval::BuildFrameReport(document, before, state));
    const Editor::FrameRow* untouched = Find(before_rows, "core", "alpha");
    REQUIRE(untouched != nullptr);
    CHECK(untouched->clip == "core_draw");
    CHECK(untouched->value == "1");

    const Preset::Eval::FrameState during = Preset::Eval::ResolveFrame(
        Preset::Eval::ResolveInput{.document = &document, .choices = &choices}, 150);
    const std::vector<Editor::FrameRow> rows =
        Editor::FrameRows(Preset::Eval::BuildFrameReport(document, during, state));

    const Editor::FrameRow* alpha = Find(rows, "core", "alpha");
    REQUIRE(alpha != nullptr);
    CHECK(alpha->clip == "core_fade");
    CHECK(alpha->value == "0.25");

    const Editor::FrameRow* position = Find(rows, "core", "position");
    REQUIRE(position != nullptr);
    CHECK(position->clip == "core_draw");
}

TEST_CASE("the frame inspector opens with a header row per entity", "[editor][frame_inspector]") {
    const Doc::Document document = ConflictingDocument();
    const std::vector<int> choices;
    const Preset::Eval::EvalState state;
    const Preset::Eval::FrameState resolved = Preset::Eval::ResolveFrame(
        Preset::Eval::ResolveInput{.document = &document, .choices = &choices}, 150);
    const std::vector<Editor::FrameRow> rows =
        Editor::FrameRows(Preset::Eval::BuildFrameReport(document, resolved, state));

    REQUIRE_FALSE(rows.empty());
    CHECK(rows.front().header);
    CHECK(rows.front().entity == "core");
    const auto camera = std::ranges::find_if(
        rows, [](const Editor::FrameRow& row) { return row.header && row.entity == "camera"; });
    CHECK(camera != rows.end());
}

TEST_CASE("the frame inspector reports each emitter's reach, ring phase and live count",
          "[editor][frame_inspector]") {
    const std::vector<Doc::Document> builtins = Doc::BuiltIns();
    const auto attract = std::ranges::find_if(
        builtins, [](const Doc::Document& doc) { return doc.id == "iidx11-attract"; });
    REQUIRE(attract != builtins.end());

    Preset::Eval::Evaluator evaluator;
    evaluator.Load(std::make_shared<const Doc::Document>(*attract), Preset::AssetLengths{});
    evaluator.Seek(640);

    const std::vector<Editor::FrameRow> rows = Editor::FrameRows(
        Preset::Eval::BuildFrameReport(*attract, evaluator.Current(), evaluator.State()));

    const std::string entity = "fx_warp_in_rotating_and_zooming";
    const Editor::FrameRow* reach = Find(rows, entity, "reach");
    REQUIRE(reach != nullptr);
    CHECK(reach->value == "305 px");
    CHECK(reach->clip == entity);

    const Editor::FrameRow* phase = Find(rows, entity, "ring phase");
    REQUIRE(phase != nullptr);
    CHECK(phase->value == "-231 deg");

    const Editor::FrameRow* live = Find(rows, entity, "live");
    REQUIRE(live != nullptr);
    CHECK(live->value == "960");
}
