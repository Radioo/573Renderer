#include "preset/eval/eval_sprites.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Preset::Eval {

namespace {

TweenValue ScalarOf(float value) {
    TweenValue out;
    out.kind = TweenValue::Kind::Scalar;
    out.scalar = value;
    return out;
}

void SamplePlacementKeys(const Doc::Clip& clip, int frame, SpriteSlot& slot) {
    if (clip.keys.empty()) return;
    const int clip_frame = frame - clip.start;
    TweenValue sampled;
    if (SampleKeys(clip.keys, "x", clip_frame, ScalarOf(slot.x), sampled)) {
        slot.x = sampled.scalar;
        slot.from.x = &clip;
    }
    if (SampleKeys(clip.keys, "y", clip_frame, ScalarOf(slot.y), sampled)) {
        slot.y = sampled.scalar;
        slot.from.y = &clip;
    }
    if (SampleKeys(clip.keys, "alpha", clip_frame, ScalarOf(slot.alpha), sampled)) {
        slot.alpha = sampled.scalar;
        slot.from.alpha = &clip;
    }
    if (SampleKeys(clip.keys, "scale", clip_frame, ScalarOf(slot.scale), sampled)) {
        slot.scale = sampled.scalar;
        slot.from.scale = &clip;
    }
}

void RecordPlacement(const Doc::Clip& clip, SpriteSlot& slot) {
    slot.from = SpriteOrigin{.visible = &clip,
                             .source = &clip,
                             .x = &clip,
                             .y = &clip,
                             .alpha = &clip,
                             .scale = &clip,
                             .blend = &clip,
                             .priority = &clip,
                             .scroll = slot.from.scroll};
}

}

void ApplySpriteDraw(const Doc::Clip& clip, const Doc::SpriteDraw& command, int frame,
                     SpriteSlot& slot, bool with_keys) {
    slot.visible = true;
    slot.animated = false;
    slot.asset = command.asset;
    slot.source = command.cell;
    slot.x = (float)command.x;
    slot.y = (float)command.y;
    slot.alpha = (float)command.alpha;
    slot.scale = (float)command.scale;
    slot.blend = (int)command.blend;
    slot.priority = command.priority;
    slot.timing = GcAnim::Timing{};
    slot.hidden_parts.clear();
    slot.speed = 1.0F;
    slot.offset = 0;
    slot.restart_clock = false;
    slot.draw_start = clip.start;
    RecordPlacement(clip, slot);
    if (with_keys) SamplePlacementKeys(clip, frame, slot);
}

void ApplySpriteAnimate(const Doc::Clip& clip, const Doc::SpriteAnimate& command, int frame,
                        SpriteSlot& slot, bool abuts_previous, bool with_keys) {
    slot.visible = true;
    slot.animated = true;
    slot.asset = command.asset;
    slot.source = command.animation;
    slot.x = (float)command.x;
    slot.y = (float)command.y;
    slot.alpha = (float)command.alpha;
    slot.scale = (float)command.scale;
    slot.blend = 0;
    slot.priority = command.priority;
    slot.timing = GcAnim::Timing{.playback = command.playback,
                                 .loop_start = command.loop_start,
                                 .loop_end = command.loop_end};
    slot.hidden_parts = command.hidden_parts;
    slot.speed = (float)command.speed;
    slot.offset = command.offset;
    const Doc::ClipClock clock =
        command.clock.value_or(abuts_previous ? Doc::ClipClock::Continue : Doc::ClipClock::Restart);
    slot.restart_clock = clock == Doc::ClipClock::Restart;
    slot.draw_start = clip.start;
    RecordPlacement(clip, slot);
    if (with_keys) SamplePlacementKeys(clip, frame, slot);
}

void ApplySpriteScroll(const Doc::Clip& clip, const Doc::SpriteScroll& command, int frame,
                       SpriteSlot& slot, bool with_keys) {
    slot.scroll_x = (float)command.scroll_x;
    slot.scroll_wrap = (float)command.scroll_wrap;
    slot.scroll_offset = (float)command.scroll_offset;
    slot.from.scroll = &clip;
    if (!with_keys || clip.keys.empty()) return;
    const int clip_frame = frame - clip.start;
    TweenValue sampled;
    if (SampleKeys(clip.keys, "scroll_x", clip_frame, ScalarOf(slot.scroll_x), sampled))
        slot.scroll_x = sampled.scalar;
}

std::vector<int> SpriteDrawOrder(const std::vector<SpriteSlot>& sprites) {
    std::vector<int> order;
    order.reserve(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); i++) {
        if (sprites[i].visible) order.push_back((int)i);
    }
    std::ranges::stable_sort(order, [&sprites](const int a, const int b) {
        return sprites[(std::size_t)a].priority > sprites[(std::size_t)b].priority;
    });
    return order;
}

}
