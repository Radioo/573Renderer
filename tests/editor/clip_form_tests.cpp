#include "editor/clip_form.h"
#include "editor/command_palette.h"
#include "editor/document_edits.h"
#include "editor/timeline_edits.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

Doc::Clip Clip(const std::string& id, int start, std::optional<int> end, Doc::Command command) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = start;
    clip.end = end;
    clip.command = std::move(command);
    return clip;
}

Doc::Track Track(const std::string& id, Doc::TrackKind kind, std::vector<Doc::Clip> clips) {
    Doc::Track track;
    track.id = id;
    track.name = id;
    track.kind = kind;
    track.target = id;
    track.clips = std::move(clips);
    return track;
}

Doc::Document MakeDocument() {
    Doc::Document document;
    document.id = "palette-test";
    document.name = "Palette test";
    document.build = "iidx11";
    document.length = 600;
    document.assets.push_back(
        Doc::Asset{.id = "red", .kind = Doc::AssetKind::Scene3d, .dir = "data/graph/model/red"});
    document.assets.push_back(Doc::Asset{
        .id = "title", .kind = Doc::AssetKind::Package2d, .dir = "data/graph/sys/title"});
    document.markers.push_back(Doc::Marker{.frame = 100, .label = "warp"});
    document.tracks.push_back(
        Track("core", Doc::TrackKind::Model,
              {Clip("core_warp", 100, 300, Doc::ModelDraw{.asset = "red", .model = "core"})}));
    document.tracks.push_back(Track("cam", Doc::TrackKind::Camera, {}));
    return document;
}

const Editor::PaletteEntry* Entry(const std::vector<Editor::PaletteEntry>& entries,
                                  Doc::CommandType type) {
    for (const Editor::PaletteEntry& entry : entries) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

const Doc::Clip* FindClip(const Doc::Document& document, const std::string& id) {
    const Editor::ClipRef ref = Editor::FindClip(document, id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

}

TEST_CASE("the palette ranks a filter match on the clicked track above every other track",
          "[editor][palette]") {
    const Doc::Document document = MakeDocument();
    const std::vector<Editor::PaletteEntry> entries =
        Editor::PaletteEntries(document, "core", 400, "tw");

    REQUIRE_FALSE(entries.empty());
    CHECK(entries.front().type == Doc::CommandType::ModelTween);
    CHECK(entries.front().section == Editor::PaletteSection::Track);
    CHECK(Entry(entries, Doc::CommandType::CameraTween) != nullptr);
    CHECK(Entry(entries, Doc::CommandType::CameraTween)->section ==
          Editor::PaletteSection::OtherTracks);
    CHECK(Entry(entries, Doc::CommandType::Emitter) == nullptr);

    const std::vector<Editor::PaletteEntry> from_camera =
        Editor::PaletteEntries(document, "cam", 400, "tw");
    REQUIRE(from_camera.size() == entries.size());
    CHECK(from_camera.front().type == Doc::CommandType::CameraTween);
    CHECK(from_camera.front().section == Editor::PaletteSection::Track);
}

TEST_CASE("every palette entry carries a one line summary of its key parameters",
          "[editor][palette]") {
    const Doc::Document document = MakeDocument();
    const std::vector<Editor::PaletteEntry> entries =
        Editor::PaletteEntries(document, "core", 400, "");
    REQUIRE(entries.size() == Doc::kCommandTypeNames.size());
    for (const Editor::PaletteEntry& entry : entries) {
        CHECK_FALSE(entry.name.empty());
        CHECK_FALSE(entry.summary.empty());
        CHECK(entry.summary.find('\n') == std::string::npos);
    }
    CHECK(Entry(entries, Doc::CommandType::OptionSelect)->section ==
          Editor::PaletteSection::Document);
}

TEST_CASE("a second primary inside an existing one is offered disabled with the reason",
          "[editor][palette]") {
    const Doc::Document document = MakeDocument();
    const std::vector<Editor::PaletteEntry> inside =
        Editor::PaletteEntries(document, "core", 200, "");
    const Editor::PaletteEntry* draw = Entry(inside, Doc::CommandType::ModelDraw);
    REQUIRE(draw != nullptr);
    CHECK_FALSE(draw->enabled);
    CHECK(draw->refusal.find("core_warp") != std::string::npos);

    const Editor::PaletteEntry* tween = Entry(inside, Doc::CommandType::ModelTween);
    REQUIRE(tween != nullptr);
    CHECK(tween->enabled);

    const std::vector<Editor::PaletteEntry> outside =
        Editor::PaletteEntries(document, "core", 400, "");
    CHECK(Entry(outside, Doc::CommandType::ModelDraw)->enabled);
}

TEST_CASE("inserting a palette command lands a defaulted clip at the anchor frame",
          "[editor][palette]") {
    Doc::Document document = MakeDocument();
    const std::string id =
        Editor::InsertPaletteCommand(document, "core", Doc::CommandType::ModelTween, 400);

    REQUIRE_FALSE(id.empty());
    const Doc::Clip* clip = FindClip(document, id);
    REQUIRE(clip != nullptr);
    CHECK(clip->start == 400);
    CHECK(Doc::TypeOf(clip->command) == Doc::CommandType::ModelTween);

    const std::string primary =
        Editor::InsertPaletteCommand(document, "core", Doc::CommandType::ModelDraw, 400);
    const Doc::Clip* drawn = FindClip(document, primary);
    REQUIRE(drawn != nullptr);
    CHECK_FALSE(drawn->end.has_value());
    CHECK(std::get<Doc::ModelDraw>(drawn->command).asset == "red");
}

TEST_CASE("adding a track from the modal appends it below the selected one", "[editor][palette]") {
    Doc::Document document = MakeDocument();
    document.tracks.push_back(Track("shield", Doc::TrackKind::Model, {}));
    const Editor::TrackSpec spec{
        .kind = Doc::TrackKind::Sprite, .target = "TITLE", .name = "TITLE", .below_selected = true};
    const std::string id = Editor::InsertTrack(document, spec, "core");

    REQUIRE(document.tracks.size() == 4);
    CHECK(document.tracks[1].id == id);
    CHECK(document.tracks[1].kind == Doc::TrackKind::Sprite);
    CHECK(document.tracks[1].target == "TITLE");
    CHECK(document.tracks[1].clips.empty());
}

TEST_CASE("every catalog command has a form row for every field descriptor", "[editor][form]") {
    for (std::size_t i = 0; i < Doc::kCommandTypeNames.size(); i++) {
        const auto type = (Doc::CommandType)i;
        const Doc::Command& command = Doc::DefaultCommand(type);
        const std::vector<Editor::FormRow> rows = Editor::FormRows(command);
        CHECK(rows.size() == Doc::FieldsFor(type).size());
        for (const Editor::FormRow& row : rows) {
            REQUIRE(row.field != nullptr);
            CHECK(row.at_default);
            CHECK_FALSE(row.default_text.empty());
        }
    }
}

TEST_CASE("a field that differs from the catalog default reports it with the default value",
          "[editor][form]") {
    Doc::Command command = Doc::DefaultCommand(Doc::CommandType::SpriteAnimate);
    std::get<Doc::SpriteAnimate>(command).priority = 15;

    const std::vector<Editor::FormRow> rows = Editor::FormRows(command);
    const auto it = std::ranges::find_if(
        rows, [](const Editor::FormRow& row) { return row.field->id == "priority"; });
    REQUIRE(it != rows.end());
    CHECK_FALSE(it->at_default);
    CHECK(it->default_text == "0");
    CHECK(Editor::AtCatalogDefault(
        command, *Doc::FindField(Doc::FieldsFor(Doc::CommandType::SpriteAnimate), "alpha")));
}

TEST_CASE("a hard range clamps and a soft range only warns", "[editor][form]") {
    const std::span<const Doc::FieldDesc> sprite = Doc::FieldsFor(Doc::CommandType::SpriteDraw);
    const Doc::FieldDesc* alpha = Doc::FindField(sprite, "alpha");
    const Doc::FieldDesc* x = Doc::FindField(sprite, "x");
    REQUIRE(alpha != nullptr);
    REQUIRE(x != nullptr);

    const Doc::ParamValue clamped = Editor::ClampField(*alpha, Doc::ParamValue{4.0});
    REQUIRE(std::holds_alternative<double>(clamped));
    CHECK(std::get<double>(clamped) == 1.0);
    CHECK_FALSE(Editor::OutsideSoftRange(*alpha, clamped));

    const Doc::ParamValue wide = Editor::ClampField(*x, Doc::ParamValue{99999.0});
    REQUIRE(std::holds_alternative<double>(wide));
    CHECK(std::get<double>(wide) == 99999.0);
    CHECK(Editor::OutsideSoftRange(*x, wide));
}

TEST_CASE("a radian field carries a degree readout and a plain field does not", "[editor][form]") {
    const std::span<const Doc::FieldDesc> model = Doc::FieldsFor(Doc::CommandType::ModelDraw);
    const Doc::FieldDesc* rotation = Doc::FindField(model, "rotation");
    const Doc::FieldDesc* scale = Doc::FindField(model, "scale");
    REQUIRE(rotation != nullptr);
    REQUIRE(scale != nullptr);

    const std::string degrees =
        Editor::DegreesText(*rotation, Doc::ParamValue{Doc::Vec3{0.0, 1.5707964, 0.0}});
    CHECK(degrees.find("90.0") != std::string::npos);
    CHECK(Editor::DegreesText(*scale, Doc::ParamValue{Doc::Vec3{1.0, 1.0, 1.0}}).empty());
}

TEST_CASE("converting the fps rescales every clip edge, key, marker and the length",
          "[editor][document]") {
    Doc::Document document = MakeDocument();
    document.tracks[0].clips[0].keys.push_back(Doc::Key{.at = 30});
    document.tracks[0].clips.push_back(
        Clip("core_tail", 300, std::nullopt, Doc::ModelDraw{.asset = "red"}));

    REQUIRE(Editor::ConvertFps(document, 30, Editor::Rounding::Nearest));

    CHECK(document.fps == 30);
    CHECK(document.length.value_or(0) == 300);
    CHECK(document.tracks[0].clips[0].start == 50);
    CHECK(document.tracks[0].clips[0].end.value_or(0) == 150);
    CHECK(document.tracks[0].clips[0].keys[0].at == 15);
    CHECK_FALSE(document.tracks[0].clips[1].end.has_value());
    CHECK(document.markers[0].frame == 50);

    CHECK_FALSE(Editor::ConvertFps(document, 30, Editor::Rounding::Nearest));
}
