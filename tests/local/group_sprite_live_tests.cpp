#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "live_stage.h"
#include "support/env.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint16_t kWidestGroup = 6;
constexpr uint32_t kShortestSpan = 20;
constexpr uint32_t kLaterBy = 10;
const LiveStage::Target kTarget{.package = "led_effects", .animation = "Background_life"};

struct Opened {
    std::string dir;
    std::optional<Document::File> file;
    std::string path;
    AfpAnimation::Animation animation;
};

std::string GameDir() {
    return Support::EnvVar("R573_IIDX_DIR").value_or("");
}

bool Load(Opened& opened) {
    opened.dir = GameDir();
    auto file = Document::File::Open(
        LiveStage::ReadAll(opened.dir + "/data/graphic/1/" + kTarget.package + ".ifs"));
    REQUIRE(file.has_value());
    if (!file) return false;
    opened.path = LiveStage::AnimationPath(*file, kTarget.animation);
    REQUIRE(!opened.path.empty());
    const auto animation = file->ReadAnimation(opened.path);
    REQUIRE(animation.has_value());
    if (!animation) return false;
    opened.animation = *animation;
    opened.file.emplace(*file);
    return true;
}

std::size_t DepthsInside(const AfpAnimation::Animation& animation, uint16_t sprite) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* found = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (found != nullptr && found->id == sprite)
            return Document::DepthRows(found->container).size();
    }
    return 0;
}

std::optional<Document::GroupRange> GroupableRange(const AfpAnimation::Animation& animation) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        for (const Document::Span& span : row.spans) {
            if (span.last_frame - span.first_frame < kShortestSpan) continue;
            for (uint16_t width = kWidestGroup; width > 1; width--) {
                const Document::GroupRange range{.clip = {},
                                                 .first_depth = row.depth,
                                                 .last_depth =
                                                     static_cast<uint16_t>(row.depth + width - 1),
                                                 .first_frame = span.first_frame,
                                                 .last_frame = span.last_frame};
                AfpAnimation::Animation grouped = animation;
                const auto sprite = Document::GroupIntoSprite(grouped, range);
                if (sprite && DepthsInside(grouped, *sprite) >= 2) return range;
            }
        }
    }
    return std::nullopt;
}

std::vector<uint32_t> FramesToCompare(const Document::GroupRange& range, std::size_t frames) {
    const uint32_t first = range.first_frame;
    const uint32_t last = range.last_frame;
    std::set<uint32_t> picked{first, first + 1, (first + last) / 2, last - 1, last};
    if (first > 0) picked.insert(first - 1);
    if (last + 1 < frames) picked.insert(last + 1);
    return {picked.begin(), picked.end()};
}

void CheckGroupDrawsAlike(const Opened& opened, const AfpAnimation::Animation& before,
                          const Document::GroupRange& range) {
    INFO("depths " << range.first_depth << " to " << range.last_depth << ", frames "
                   << range.first_frame << " to " << range.last_frame);
    AfpAnimation::Animation grouped = before;
    const auto sprite = Document::GroupIntoSprite(grouped, range);
    const std::string error = sprite.has_value() ? std::string() : sprite.error();
    INFO(error);
    REQUIRE(sprite.has_value());
    AfpAnimation::Animation removed = grouped;
    REQUIRE(Document::RemoveDepth(removed, {}, range.first_depth, range.first_frame).has_value());

    Document::File shown = *opened.file;
    REQUIRE(shown.WriteAnimation(opened.path, before).has_value());
    Document::File after = *opened.file;
    REQUIRE(after.WriteAnimation(opened.path, grouped).has_value());
    Document::File without = *opened.file;
    REQUIRE(without.WriteAnimation(opened.path, removed).has_value());

    LiveStage::Stage stage(opened.dir, kTarget);
    bool group_drawn = false;
    for (const uint32_t frame : FramesToCompare(range, before.root.frames.size())) {
        INFO("frame " << frame);
        const std::vector<uint8_t> expected = stage.Render(shown, frame);
        CHECK(stage.Render(after, frame) == expected);
        const bool inside = frame >= range.first_frame && frame <= range.last_frame;
        if (inside) group_drawn = group_drawn || stage.Render(without, frame) != expected;
    }
    CHECK(group_drawn);
}

}

TEST_CASE("Depths grouped into a sprite draw every compared frame exactly as before") {
    if (GameDir().empty()) SKIP("R573_IIDX_DIR not set");
    Opened opened;
    if (!Load(opened)) return;
    const std::optional<Document::GroupRange> range = GroupableRange(opened.animation);
    REQUIRE(range.has_value());
    if (!range) return;
    CheckGroupDrawsAlike(opened, opened.animation, *range);
}

TEST_CASE("Depths grouped from a later frame draw every compared frame exactly as before") {
    if (GameDir().empty()) SKIP("R573_IIDX_DIR not set");
    Opened opened;
    if (!Load(opened)) return;
    const std::optional<Document::GroupRange> range = GroupableRange(opened.animation);
    REQUIRE(range.has_value());
    if (!range) return;

    AfpAnimation::Animation later = opened.animation;
    for (const Document::DepthRow& row : Document::DepthRows(opened.animation.root)) {
        if (row.depth < range->first_depth || row.depth > range->last_depth) continue;
        for (const Document::Span& span : row.spans) {
            const Document::Span kept{.first_frame = span.first_frame + kLaterBy,
                                      .last_frame = span.last_frame};
            const auto trimmed = Document::TrimSpan(later, {}, row.depth, span.first_frame, kept);
            const std::string error = trimmed.has_value() ? std::string() : trimmed.error();
            INFO("depth " << row.depth << ": " << error);
            REQUIRE(trimmed.has_value());
        }
    }
    Document::GroupRange shifted = *range;
    shifted.first_frame += kLaterBy;
    CheckGroupDrawsAlike(opened, later, shifted);
}
