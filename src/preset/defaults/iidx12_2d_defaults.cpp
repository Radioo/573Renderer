#include "preset/defaults/defaults_build.h"

#include "formats/gcanim.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <string>
#include <utility>
#include <vector>

namespace Preset::Doc {

using namespace Build;

namespace {

constexpr int kAttractFrames = 2461;
constexpr int kLogoInFrame = 423;
constexpr int kStandbyFrame = 544;
constexpr int kAttractFadeFrame = 2400;
constexpr int kTitlePriority = 15;
constexpr int kCardFrames = 3600;
constexpr int kCardPriority = 31;

std::vector<std::string> TitleChrome() {
    return {"X_COIN_BRINK"};
}

Clip TitleClip(std::string id, std::string animation, int start, int end,
               GcAnim::Playback playback) {
    return Clip{.id = std::move(id),
                .start = start,
                .end = end,
                .command = SpriteAnimate{.asset = "title",
                                         .animation = std::move(animation),
                                         .priority = kTitlePriority,
                                         .playback = playback,
                                         .clock = ClipClock::Restart,
                                         .hidden_parts = TitleChrome()}};
}

Document Iidx12Attract() {
    return Document{
        .id = "iidx12-attract",
        .name = "Title attract",
        .build = "iidx12",
        .length = kAttractFrames,
        .render = RenderSpec{.clear_color = {0.0, 0.0, 0.0}},
        .assets = {Package2dAsset("title", "data/graph/sys/title")},
        .markers = {Marker{.frame = 0, .label = "Intro film"},
                    Marker{.frame = kLogoInFrame, .label = "Logo reveal"},
                    Marker{.frame = kStandbyFrame, .label = "Standby loop"},
                    Marker{.frame = kAttractFadeFrame, .label = "Attract times out"}},
        .tracks = {SpriteTrack(
            "title", "title",
            {TitleClip("title_intro_film", "TITLE", 0, kLogoInFrame, GcAnim::Playback::HoldLast),
             TitleClip("title_logo_reveal", "LOGO_IN", kLogoInFrame, kStandbyFrame,
                       GcAnim::Playback::HoldLast),
             TitleClip("title_standby_loop", "TITLE_TAIKI", kStandbyFrame, kAttractFrames,
                       GcAnim::Playback::Loop)})}};
}

Document Iidx12CardIn() {
    return Document{.id = "iidx12-card-in",
                    .name = "Card in",
                    .build = "iidx12",
                    .length = kCardFrames,
                    .render = RenderSpec{.clear_color = {0.0, 0.0, 0.0}},
                    .assets = {Package2dAsset("card", "data/graph/sys/card")},
                    .markers = {Marker{.frame = 0, .label = "Card hall"}},
                    .tracks = {SpriteTrack(
                        "card_bg", "card_bg",
                        {Clip{.id = "card_bg_hall",
                              .end = kCardFrames,
                              .command = SpriteAnimate{.asset = "card",
                                                       .animation = "CARD_BG",
                                                       .priority = kCardPriority,
                                                       .playback = GcAnim::Playback::HoldLast,
                                                       .hidden_parts = {"T_REMAIN"}}}})}};
}

}

std::vector<Document> Iidx12TwoD() {
    return {Iidx12Attract(), Iidx12CardIn()};
}

}
