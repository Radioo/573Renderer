#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
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

Preset::AssetLengths TitleLengths() {
    Preset::AssetLengths lengths;
    lengths.animation_frames["data/graph/sys/title"]["TITLE"] = 422;
    lengths.animation_frames["data/graph/sys/title"]["LOGO_IN"] = 120;
    lengths.animation_frames["data/graph/sys/title"]["TITLE_TAIKI"] = 480;
    lengths.animation_frames["data/graph/sys/card"]["CARD_BG"] = 120;
    return lengths;
}

Preset::Eval::Evaluator At(const PD::Document& document, int frame) {
    Preset::Eval::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(document), TitleLengths());
    evaluator.Seek(frame);
    return evaluator;
}

const PD::SpriteAnimate* AnimateOf(const PD::Clip& clip) {
    return std::get_if<PD::SpriteAnimate>(&clip.command);
}

int CountModels(const PD::Document& document) {
    int models = 0;
    for (const PD::Track& track : document.tracks) {
        for (const PD::Clip& clip : track.clips)
            models += std::holds_alternative<PD::ModelDraw>(clip.command) ? 1 : 0;
    }
    return models;
}

const Preset::Eval::SpriteSlot* OnlyVisible(const Preset::Eval::FrameState& state) {
    const Preset::Eval::SpriteSlot* found = nullptr;
    int visible = 0;
    for (const Preset::Eval::SpriteSlot& slot : state.sprites) {
        if (!slot.visible) continue;
        visible++;
        found = &slot;
    }
    return (visible == 1) ? found : nullptr;
}

bool Hides(const PD::SpriteAnimate& animate, const std::string& part) {
    return std::ranges::find(animate.hidden_parts, part) != animate.hidden_parts.end();
}

}

TEST_CASE("the HAPPY SKY attract built-in runs its three title clips on one layer") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* attract = Find(built_ins, "iidx12-attract");
    REQUIRE(attract != nullptr);
    CHECK(attract->build == "iidx12");
    CHECK(attract->length == 2461);
    CHECK(Describe(PD::Validate(*attract)).empty());
    CHECK(attract->render.clear_color == PD::Vec3{0.0, 0.0, 0.0});
    INFO("a 2D-only screen carries no model and no light");
    CHECK(CountModels(*attract) == 0);
    CHECK(attract->lights.empty());
    REQUIRE(attract->assets.size() == 1);
    CHECK(attract->assets[0].dir == "data/graph/sys/title");
    CHECK(attract->assets[0].kind == PD::AssetKind::Package2d);

    REQUIRE(attract->markers.size() == 4);
    CHECK(attract->markers[0].frame == 0);
    CHECK(attract->markers[1].frame == 423);
    CHECK(attract->markers[2].frame == 544);
    CHECK(attract->markers[3].frame == 2400);

    REQUIRE(attract->tracks.size() == 1);
    const PD::Track& track = attract->tracks[0];
    CHECK(track.kind == PD::TrackKind::Sprite);
    REQUIRE(track.clips.size() == 3);

    const std::vector<std::string> names = {"TITLE", "LOGO_IN", "TITLE_TAIKI"};
    const std::vector<int> starts = {0, 423, 544};
    const std::vector<int> ends = {423, 544, 2461};
    const std::vector<GcAnim::Playback> playback = {
        GcAnim::Playback::HoldLast, GcAnim::Playback::HoldLast, GcAnim::Playback::Loop};
    for (std::size_t i = 0; i < track.clips.size(); i++) {
        const PD::Clip& clip = track.clips[i];
        INFO(names[i]);
        const PD::SpriteAnimate* animate = AnimateOf(clip);
        REQUIRE(animate != nullptr);
        CHECK(animate->animation == names[i]);
        CHECK(animate->priority == 15);
        CHECK(animate->playback == playback[i]);
        CHECK(clip.start == starts[i]);
        REQUIRE(clip.end.has_value());
        CHECK(*clip.end == ends[i]);
        INFO("the game unregisters the old layer and registers a fresh one, so each clip restarts");
        CHECK(animate->clock == PD::ClipClock::Restart);
        INFO("the attract shows the game's own title content: the wordmark, the strapline, the "
             "advert block and the version mark all stay, only the coin prompt is hidden");
        CHECK(animate->hidden_parts == std::vector<std::string>{"X_COIN_BRINK"});
        CHECK_FALSE(Hides(*animate, "L_HAPPY1"));
        CHECK_FALSE(Hides(*animate, "2DXVER12"));
    }
}

TEST_CASE("the HAPPY SKY attract hands the layer over the frame after each clip ends") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* attract = Find(built_ins, "iidx12-attract");
    REQUIRE(attract != nullptr);

    const std::vector<std::pair<int, std::string>> expected = {
        {0, "TITLE"},     {422, "TITLE"},       {423, "LOGO_IN"},
        {543, "LOGO_IN"}, {544, "TITLE_TAIKI"}, {2460, "TITLE_TAIKI"}};
    for (const auto& [frame, animation] : expected) {
        INFO("frame " << frame);
        const Preset::Eval::Evaluator evaluator = At(*attract, frame);
        const Preset::Eval::SpriteSlot* slot = OnlyVisible(evaluator.Current());
        REQUIRE(slot != nullptr);
        CHECK(slot->source == animation);
        CHECK(slot->animated);
        CHECK(slot->priority == 15);
        const GcAnim::Playback want =
            (animation == "TITLE_TAIKI") ? GcAnim::Playback::Loop : GcAnim::Playback::HoldLast;
        CHECK(slot->timing.playback == want);
    }

    INFO("each handover restarts the layer clock, exactly as a fresh registration does");
    REQUIRE_FALSE(At(*attract, 423).State().sprite_clock.empty());
    CHECK(At(*attract, 423).State().sprite_clock[0] == 0.0F);
    CHECK(At(*attract, 543).State().sprite_clock[0] == 120.0F);
    CHECK(At(*attract, 544).State().sprite_clock[0] == 0.0F);
}

TEST_CASE("the HAPPY SKY card in built-in holds the card hall plate with no time remain chip") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* card = Find(built_ins, "iidx12-card-in");
    REQUIRE(card != nullptr);
    CHECK(card->build == "iidx12");
    CHECK(card->length == 3600);
    CHECK(Describe(PD::Validate(*card)).empty());
    CHECK(card->render.clear_color == PD::Vec3{0.0, 0.0, 0.0});
    CHECK(CountModels(*card) == 0);
    CHECK(card->lights.empty());
    REQUIRE(card->assets.size() == 1);
    CHECK(card->assets[0].dir == "data/graph/sys/card");
    REQUIRE(card->markers.size() == 1);
    CHECK(card->markers[0].frame == 0);

    REQUIRE(card->tracks.size() == 1);
    REQUIRE(card->tracks[0].clips.size() == 1);
    const PD::SpriteAnimate* animate = AnimateOf(card->tracks[0].clips[0]);
    REQUIRE(animate != nullptr);
    CHECK(animate->animation == "CARD_BG");
    CHECK(animate->priority == 31);
    CHECK(animate->playback == GcAnim::Playback::HoldLast);
    INFO("only T_REMAIN is a separable draw; the version mark and the advert are in the texture");
    CHECK(animate->hidden_parts == std::vector<std::string>{"T_REMAIN"});

    const Preset::Eval::Evaluator evaluator = At(*card, 3599);
    const Preset::Eval::SpriteSlot* slot = OnlyVisible(evaluator.Current());
    REQUIRE(slot != nullptr);
    CHECK(slot->source == "CARD_BG");
    CHECK(slot->priority == 31);
}
