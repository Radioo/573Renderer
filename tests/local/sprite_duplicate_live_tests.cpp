#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip_edit.h"
#include "document/document.h"
#include "document/group_sprite.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "live_stage.h"
#include "support/env.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kFrame = 400;

struct ShownSprite {
    uint16_t depth = 0;
    uint16_t sprite = 0;
};

bool DefinesSprite(const AfpAnimation::Animation& animation, uint16_t id) {
    return std::ranges::any_of(animation.root.tags, [id](const AfpAnimation::Tag& tag) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        return sprite != nullptr && sprite->id == id;
    });
}

std::optional<ShownSprite> SpriteOnFrame(const AfpAnimation::Animation& animation) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        for (const Document::Span& span : row.spans) {
            if (span.first_frame > kFrame || span.last_frame < kFrame) continue;
            const auto shown = row.shows.find(span.first_frame);
            if (shown != row.shows.end() && DefinesSprite(animation, shown->second))
                return ShownSprite{.depth = row.depth, .sprite = shown->second};
        }
    }
    return std::nullopt;
}

}

TEST_CASE("A depth showing a duplicate of its sprite draws as it did with the original") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto original = Document::File::Open(LiveStage::ReadAll(dir + "/data/graphic/1/title.ifs"));
    REQUIRE(original.has_value());
    if (!original) return;
    const std::string path = LiveStage::AnimationPath(*original, "title");
    REQUIRE(!path.empty());
    auto animation = original->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return;
    const std::optional<ShownSprite> shown = SpriteOnFrame(*animation);
    REQUIRE(shown.has_value());
    if (!shown) return;

    const auto copy = Document::DuplicateSprite(*animation, shown->sprite);
    INFO((copy.has_value() ? std::string() : copy.error()));
    REQUIRE(copy.has_value());
    if (!copy) return;
    CHECK(*copy != shown->sprite);
    const auto used = Document::EditPlacementField(*animation, {}, shown->depth, kFrame,
                                                   "Character", std::to_string(*copy));
    INFO((used.has_value() ? std::string() : used.error()));
    REQUIRE(used.has_value());
    Document::File edited = *original;
    REQUIRE(edited.WriteAnimation(path, *animation).has_value());

    LiveStage::Stage stage(dir, LiveStage::Target{.package = "title", .animation = "title"});
    const std::vector<uint8_t> expected = stage.Render(*original, kFrame);
    CHECK(stage.Render(edited, kFrame) == expected);
}
