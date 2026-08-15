#include "preset_push_text.h"

#include "preset_golden_format.h"

#include "formats/gcanim.h"
#include "preset/eval/eval_push.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace PresetPushText {

namespace {

using Preset::Eval::Push;
using Preset::Eval::PushCall;

std::string Num(float value) {
    return PresetGolden::FormatFloat(value);
}

std::string Int(int value) {
    return std::to_string(value);
}

std::string Flag(bool value) {
    return value ? "true" : "false";
}

std::string Str(const std::string& value) {
    return "'" + value + "'";
}

std::string Vec(const std::array<float, 3>& value) {
    return "[" + Num(value[0]) + " " + Num(value[1]) + " " + Num(value[2]) + "]";
}

std::string ProjectionText(const Preset::Eval::ProjectionPush& projection) {
    return "proj[" + Num(projection.fov_y) + " " + Num(projection.near_z) + " " +
           Num(projection.far_z) + " " + Num(projection.aspect) + "]";
}

std::string LightText(const Preset::Eval::LightPush& light) {
    return "light[" + Vec(light.direction) + Vec(light.diffuse) + Vec(light.specular) + "]";
}

std::string TimingText(const GcAnim::Timing& timing) {
    return "timing[" + Int((int)timing.playback) + " " + Int(timing.loop_start) + " " +
           Int(timing.loop_end) + "]";
}

std::string PartsText(const std::vector<std::string>& parts) {
    std::string out = "parts[";
    for (std::size_t i = 0; i < parts.size(); i++) {
        if (i > 0) out += " ";
        out += parts[i];
    }
    return out + "]";
}

std::string PlacementText(const Preset::Eval::SpritePlacement& sprite) {
    return "sprite[" + sprite.name + " " + Flag(sprite.animated) + " " + Int(sprite.priority) +
           " " + Num(sprite.x) + " " + Num(sprite.y) + " " + Num(sprite.alpha) + " " +
           Num(sprite.scale) + " " + Int(sprite.blend) + " " + TimingText(sprite.timing) + " " +
           PartsText(sprite.skip_parts) + " " + Num(sprite.scroll_x) + " " +
           Num(sprite.scroll_wrap) + "]";
}

std::string CellText(const Preset::Eval::CellDraw& cell) {
    return "cell[" + cell.name + " " + Num(cell.x) + " " + Num(cell.y) + " " + Num(cell.alpha) +
           " " + Num(cell.scale) + " " + Int(cell.blend) + "]";
}

}

std::string Format(const Push& push, bool legacy_rotation) {
    std::vector<std::string> args;
    switch (push.call) {
    case PushCall::DrawSprites:
        return PresetGolden::FormatCall("Gc2dHost::DrawSprites",
                                        {Int(push.index), Int(push.index2)});
    case PushCall::DrawParticles:
        for (const Preset::Eval::CellDraw& cell : push.cells)
            args.push_back(CellText(cell));
        return PresetGolden::FormatCall("Gc2dHost::DrawParticles", args);
    case PushCall::RenderFrame:
        return PresetGolden::FormatCall("Scene3dHost::RenderFrame", {Num(push.value)});
    case PushCall::AdvanceSprites:
        return PresetGolden::FormatCall("Gc2dHost::AdvanceSprites", {Num(push.value)});
    case PushCall::SetSprites:
        for (const Preset::Eval::SpritePlacement& sprite : push.sprites)
            args.push_back(PlacementText(sprite));
        return PresetGolden::FormatCall("Gc2dHost::SetSprites", args);
    case PushCall::SetSpriteScale:
        return PresetGolden::FormatCall("Gc2dHost::SetSpriteScale",
                                        {Int(push.index), Num(push.value)});
    case PushCall::SetSpriteFrame:
        return PresetGolden::FormatCall("Gc2dHost::SetSpriteFrame",
                                        {Int(push.index), Int(push.index2)});
    case PushCall::SetStyle:
        return PresetGolden::FormatCall("Scene3dHost::SetStyle", {Int(push.index)});
    case PushCall::SetView:
        return PresetGolden::FormatCall("Scene3dHost::SetView",
                                        {Vec(push.vec_a), Vec(push.vec_b), Vec(push.vec_c)});
    case PushCall::SetProjection:
        return PresetGolden::FormatCall("Scene3dHost::SetProjection",
                                        {ProjectionText(push.projection)});
    case PushCall::SetLights:
        for (const Preset::Eval::LightPush& light : push.lights)
            args.push_back(LightText(light));
        return PresetGolden::FormatCall("Scene3dHost::SetLights", args);
    case PushCall::SetModelAlpha:
        return PresetGolden::FormatCall("Scene3dHost::SetModelAlpha",
                                        {Str(push.name), Num(push.value)});
    case PushCall::SetModelSpeed:
        return PresetGolden::FormatCall("Scene3dHost::SetModelSpeed",
                                        {Str(push.name), Num(push.value)});
    case PushCall::SetModelBlend:
        return PresetGolden::FormatCall("Scene3dHost::SetModelBlendByName",
                                        {Str(push.name), Int(push.index)});
    case PushCall::SetModelScale:
        return PresetGolden::FormatCall("Scene3dHost::SetModelScale",
                                        {Str(push.name), Vec(push.vec_a)});
    case PushCall::SetModelVisible:
        return PresetGolden::FormatCall("Scene3dHost::SetModelVisibleByName",
                                        {Str(push.name), Flag(push.flag)});
    case PushCall::SetModelTransform:
        return PresetGolden::FormatCall("Scene3dHost::SetModelTransform",
                                        {Str(push.name), Vec(push.vec_a),
                                         Vec(legacy_rotation ? push.legacy_vec_b : push.vec_b)});
    case PushCall::SetModelTime:
        return PresetGolden::FormatCall("Scene3dHost::SetModelTime",
                                        {Str(push.name), Num(push.value)});
    }
    return {};
}

}
