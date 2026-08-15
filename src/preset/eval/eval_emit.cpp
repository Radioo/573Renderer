#include "preset/eval/eval_emit.h"

#include "preset/eval/eval_push.h"
#include "preset/eval/eval_sprites.h"
#include "preset/eval/frame_state.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Preset::Eval {

namespace {

Push FlagPush(PushCall call, const std::string& name, bool flag) {
    Push push;
    push.call = call;
    push.name = name;
    push.flag = flag;
    return push;
}

SpritePlacement PlacementOf(const SpriteSlot& slot) {
    SpritePlacement placement;
    placement.asset = slot.asset;
    placement.target = slot.name;
    placement.name = slot.source;
    placement.animated = slot.animated;
    placement.priority = slot.priority;
    placement.x = slot.x;
    placement.y = slot.y;
    placement.alpha = slot.alpha;
    placement.scale = slot.scale;
    placement.blend = slot.blend;
    placement.timing = slot.timing;
    placement.skip_parts = slot.hidden_parts;
    placement.scroll_x = slot.scroll_x;
    placement.scroll_wrap = slot.scroll_wrap;
    placement.scroll_offset = slot.scroll_offset;
    return placement;
}

const ModelSlot* ModelOf(const FrameState& state, int index) {
    if (index < 0 || (std::size_t)index >= state.models.size()) return nullptr;
    return &state.models[(std::size_t)index];
}

}

Push ScalarPush(PushCall call, const std::string& name, float value, bool legacy) {
    Push push;
    push.call = call;
    push.legacy = legacy;
    push.name = name;
    push.value = value;
    return push;
}

Push IntegerPush(PushCall call, const std::string& name, int index, bool legacy) {
    Push push;
    push.call = call;
    push.legacy = legacy;
    push.name = name;
    push.index = index;
    return push;
}

Push VectorPush(PushCall call, const std::string& name, const Vec3f& value, bool legacy) {
    Push push;
    push.call = call;
    push.legacy = legacy;
    push.name = name;
    push.vec_a = value;
    return push;
}

Push ViewPush(const CameraState& camera, bool legacy) {
    Push push;
    push.call = PushCall::SetView;
    push.legacy = legacy;
    push.vec_a = camera.eye;
    push.vec_b = camera.at;
    push.vec_c = camera.up;
    return push;
}

Push ProjectionOf(const CameraState& camera, bool legacy) {
    Push push;
    push.call = PushCall::SetProjection;
    push.legacy = legacy;
    push.projection = ProjectionPush{.fov_y = camera.fov_y,
                                     .near_z = camera.near_z,
                                     .far_z = camera.far_z,
                                     .aspect = camera.aspect_auto ? 0.0F : camera.aspect_value};
    return push;
}

void EmitRebind(const FrameState& state, std::vector<Push>& out) {
    out.push_back(IntegerPush(PushCall::SetStyle, {}, (int)state.shading, true));
    out.push_back(ViewPush(state.camera, true));
    out.push_back(ProjectionOf(state.camera, true));
    Push lights;
    lights.call = PushCall::SetLights;
    lights.lights.reserve(state.lights.size());
    for (const LightState& light : state.lights) {
        lights.lights.push_back(LightPush{.direction = light.direction,
                                          .diffuse = light.diffuse,
                                          .specular = light.specular,
                                          .enabled = light.enabled});
    }
    out.push_back(std::move(lights));
    for (const ModelSlot& slot : state.models) {
        out.push_back(ScalarPush(PushCall::SetModelAlpha, slot.name, slot.alpha, true));
        out.push_back(ScalarPush(PushCall::SetModelSpeed, slot.name, slot.anim_speed, true));
        out.push_back(IntegerPush(PushCall::SetModelBlend, slot.name, slot.blend_mode, true));
        out.push_back(VectorPush(PushCall::SetModelScale, slot.name, slot.scale, true));
        out.push_back(FlagPush(PushCall::SetModelVisible, slot.name, slot.visible));
    }
    Push placements;
    placements.call = PushCall::SetSprites;
    for (const int index : SpriteDrawOrder(state.sprites))
        placements.sprites.push_back(PlacementOf(state.sprites[(std::size_t)index]));
    out.push_back(std::move(placements));
    if (state.models.empty()) return;
    const ModelSlot& lead = state.models.front();
    out.push_back(ScalarPush(PushCall::SetModelSpeed, lead.name, lead.anim_speed, true));
    out.push_back(ScalarPush(PushCall::SetModelAlpha, lead.name, lead.alpha, true));
    out.push_back(IntegerPush(PushCall::SetModelBlend, lead.name, lead.blend_mode, true));
}

void EmitSpriteScales(const FrameState& state, std::vector<Push>& out) {
    const std::vector<int> order = SpriteDrawOrder(state.sprites);
    for (std::size_t i = 0; i < order.size(); i++) {
        Push push = ScalarPush(PushCall::SetSpriteScale, {},
                               state.sprites[(std::size_t)order[i]].scale, true);
        push.index = (int)i;
        out.push_back(std::move(push));
    }
}

void EmitWrites(const FrameState& state, std::vector<Push>& out) {
    for (const MaterialWrite& write : state.writes) {
        const ModelSlot* slot = ModelOf(state, write.model);
        const std::string name = slot != nullptr ? slot->name : std::string();
        switch (write.kind) {
        case WriteKind::Alpha:
            if (slot != nullptr) {
                out.push_back(
                    ScalarPush(PushCall::SetModelAlpha, name, write.scalar, write.legacy));
            }
            break;
        case WriteKind::AnimSpeed:
            if (slot != nullptr) {
                out.push_back(
                    ScalarPush(PushCall::SetModelSpeed, name, write.scalar, write.legacy));
            }
            break;
        case WriteKind::BlendMode:
            if (slot != nullptr) {
                out.push_back(
                    IntegerPush(PushCall::SetModelBlend, name, write.integer, write.legacy));
            }
            break;
        case WriteKind::ModelScale:
            if (slot != nullptr) {
                out.push_back(
                    VectorPush(PushCall::SetModelScale, name, write.vector, write.legacy));
            }
            break;
        case WriteKind::CameraView:
            out.push_back(ViewPush(write.camera, write.legacy));
            break;
        case WriteKind::CameraProjection:
            out.push_back(ProjectionOf(write.camera, write.legacy));
            break;
        }
    }
}

void EmitUnconditional(const FrameState& state, const std::vector<float>& ticks,
                       const std::vector<float>& clocks, std::vector<Push>& out) {
    for (const ModelSlot& slot : state.models) {
        if (!slot.visible) continue;
        out.push_back(ScalarPush(PushCall::SetModelAlpha, slot.name, slot.alpha, false));
    }
    out.push_back(ViewPush(state.camera, false));
    out.push_back(ProjectionOf(state.camera, false));
    for (std::size_t i = 0; i < state.models.size() && i < ticks.size(); i++)
        out.push_back(ScalarPush(PushCall::SetModelTime, state.models[i].name, ticks[i], false));
    const std::vector<int> order = SpriteDrawOrder(state.sprites);
    for (std::size_t i = 0; i < order.size(); i++) {
        Push push;
        push.call = PushCall::SetSpriteFrame;
        push.legacy = false;
        push.index = (int)i;
        push.index2 = (int)clocks[(std::size_t)order[i]];
        out.push_back(std::move(push));
    }
}

}
