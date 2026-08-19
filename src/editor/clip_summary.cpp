#include "editor/clip_summary.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

std::string Num(double value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.4g", value);
    return buffer;
}

std::string Vec(const Doc::Vec3& value) {
    return Num(value[0]) + " " + Num(value[1]) + " " + Num(value[2]);
}

std::string Name(std::span<const std::string_view> names, int index) {
    return std::string(Doc::NameForIndex(names, index));
}

std::string TypeName(Doc::CommandType type) {
    return Name(Doc::kCommandTypeNames, (int)type);
}

void Append(std::string& out, const std::string& part) {
    if (part.empty()) return;
    if (!out.empty()) out += ", ";
    out += part;
}

std::string Degrees(double radians) {
    return Num(radians * 180.0 / std::numbers::pi);
}

std::string SpinText(const Doc::Vec3& spin) {
    std::size_t axis = 0;
    for (std::size_t i = 1; i < spin.size(); i++) {
        if (std::fabs(spin[i]) > std::fabs(spin[axis])) axis = i;
    }
    if (spin[axis] == 0.0) return {};
    const char letter = "xyz"[axis];
    return std::string("spin ") + letter + " " + Degrees(spin[axis]) + " deg/f";
}

std::string OverrideText(const Doc::OverrideValue& value) {
    if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const auto* number = std::get_if<double>(&value)) return Num(*number);
    if (const auto* text = std::get_if<std::string>(&value)) return *text;
    return Vec(std::get<Doc::Vec3>(value));
}

std::string SpriteDrawText(const Doc::SpriteDraw& command) {
    std::string out = command.cell.empty() ? std::string("cell?") : command.cell;
    Append(out, Name(Doc::kSpriteBlendNames, (int)command.blend));
    Append(out, "prio " + std::to_string(command.priority));
    return out;
}

std::string SpriteAnimateText(const Doc::SpriteAnimate& command) {
    std::string out = command.animation.empty() ? std::string("animation?") : command.animation;
    Append(out, Name(Doc::kPlaybackNames, (int)command.playback));
    Append(out, "prio " + std::to_string(command.priority));
    return out;
}

std::string SpriteScrollText(const Doc::SpriteScroll& command) {
    std::string out = "scroll x " + Num(command.scroll_x) + "/f";
    if (command.scroll_wrap != 0.0) Append(out, "wrap " + Num(command.scroll_wrap));
    return out;
}

std::string EmitterText(const Doc::EmitterCmd& command) {
    std::string out = command.cell.empty() ? std::string("cell?") : command.cell;
    out += " x" + std::to_string(command.count);
    Append(out, Name(Doc::kSpawnNames, (int)command.spawn));
    Append(out,
           "r " + std::to_string(command.radius_from) + ".." + std::to_string(command.radius_to));
    return out;
}

std::string ModelDrawText(const Doc::ModelDraw& command, std::string_view target) {
    std::string named = command.model.empty() ? std::string(target) : command.model;
    std::string out = named.empty() ? std::string("model?") : std::move(named);
    Append(out, Name(Doc::kModelBlendNames, (int)command.blend_mode));
    Append(out, SpinText(command.spin_per_frame));
    return out;
}

std::string ModelMotionText(const Doc::ModelMotionCmd& command) {
    std::string out;
    if (command.orbit.has_value()) Append(out, "orbit r " + Num(command.orbit->radius));
    if (command.spin_kick != 0.0) Append(out, "kick " + Num(command.spin_kick));
    if (command.pulse.has_value())
        Append(out, "pulse " + Name(Doc::kGridNames, (int)command.pulse->grid));
    if (out.empty()) return TypeName(Doc::CommandType::ModelMotion);
    return out;
}

std::string CameraSetText(const Doc::CameraSet& command) {
    std::string out;
    if (command.eye.has_value()) Append(out, "eye " + Vec(*command.eye));
    if (command.at.has_value()) Append(out, "at " + Vec(*command.at));
    if (command.fov_y.has_value()) Append(out, "fov " + Degrees(*command.fov_y) + " deg");
    if (out.empty()) return TypeName(Doc::CommandType::CameraSet);
    return out;
}

std::string LightSetText(const Doc::LightSet& command) {
    std::string out = "light " + std::to_string(command.index);
    if (!command.enabled) {
        Append(out, "off");
        return out;
    }
    if (command.direction.has_value()) Append(out, "dir " + Vec(*command.direction));
    if (command.diffuse.has_value()) Append(out, "diffuse " + Vec(*command.diffuse));
    return out;
}

std::string RenderSettingsText(const Doc::RenderSettingsCmd& command) {
    std::string out;
    if (command.shading.has_value()) Append(out, Name(Doc::kShadingNames, (int)*command.shading));
    if (command.sprite_split_priority.has_value())
        Append(out, "split " + std::to_string(*command.sprite_split_priority));
    if (out.empty()) return TypeName(Doc::CommandType::RenderSettings);
    return out;
}

std::string FogText(const Doc::FogCmd& command) {
    if (!command.enabled) return "fog off";
    return "fog " + Vec(command.color) + ", " + Num(command.start) + " to " + Num(command.end);
}

std::string ClearCycleText(const Doc::ClearCycleCmd& command) {
    return "strobe " + std::to_string(command.strobe_period) + " f, ramp " +
           std::to_string(command.ramp_period) + " f";
}

std::string CameraEaseText(const Doc::CameraEaseCmd& command) {
    return "ease eye " + Vec(command.eye_target) + " at " + Vec(command.at_target) + " by " +
           Num(command.rate);
}

std::string ModelEaseText(const Doc::ModelEaseCmd& command) {
    std::string out =
        "ease " + Name(Doc::kEaseModeNames, (int)command.mode) + " by " + Num(command.rate);
    if (command.scale_target.has_value()) Append(out, "scale " + Vec(*command.scale_target));
    if (command.position_target.has_value()) Append(out, "pos " + Vec(*command.position_target));
    if (command.alpha_target.has_value()) Append(out, "alpha " + Num(*command.alpha_target));
    return out;
}

std::string CameraMotionText(const Doc::CameraMotionCmd& command) {
    return "roll up " + Num(command.up_roll_deg_per_frame) + " deg/f";
}

std::string PolyTileGridText(const Doc::PolyTileGrid& command) {
    std::string out =
        std::to_string(command.rows) + "x" + std::to_string(command.cols) + " movie tiles";
    Append(out, command.texture.has_value() ? command.texture->path : std::string("untextured"));
    Append(out, "seed " + std::to_string(command.lattice_seed));
    return out;
}

std::string RhythmJitterText(const Doc::RhythmJitter& command) {
    std::string out = "jitter span " + std::to_string(command.span);
    Append(out, Name(Doc::kJitterModeNames, (int)command.mode));
    if (!command.models.empty()) Append(out, std::to_string(command.models.size()) + " model(s)");
    return out;
}

}

std::string CommandSummary(const Doc::Command& command, std::string_view target) {
    if (const auto* draw = std::get_if<Doc::SpriteDraw>(&command)) return SpriteDrawText(*draw);
    if (const auto* animate = std::get_if<Doc::SpriteAnimate>(&command))
        return SpriteAnimateText(*animate);
    if (const auto* scroll = std::get_if<Doc::SpriteScroll>(&command))
        return SpriteScrollText(*scroll);
    if (const auto* emitter = std::get_if<Doc::EmitterCmd>(&command)) return EmitterText(*emitter);
    if (const auto* model = std::get_if<Doc::ModelDraw>(&command))
        return ModelDrawText(*model, target);
    if (const auto* motion = std::get_if<Doc::ModelMotionCmd>(&command))
        return ModelMotionText(*motion);
    if (const auto* camera = std::get_if<Doc::CameraSet>(&command)) return CameraSetText(*camera);
    if (const auto* light = std::get_if<Doc::LightSet>(&command)) return LightSetText(*light);
    if (const auto* param = std::get_if<Doc::ParamOverrideCmd>(&command))
        return param->id + " = " + OverrideText(param->value);
    if (const auto* render = std::get_if<Doc::RenderSettingsCmd>(&command))
        return RenderSettingsText(*render);
    if (const auto* seed = std::get_if<Doc::RngSeed>(&command))
        return "seed " + std::to_string(seed->seed);
    if (const auto* beat = std::get_if<Doc::RhythmBeat>(&command)) {
        return "beat " + std::to_string(beat->rate) + " / " + std::to_string(beat->span);
    }
    if (const auto* jitter = std::get_if<Doc::RhythmJitter>(&command))
        return RhythmJitterText(*jitter);
    if (const auto* select = std::get_if<Doc::OptionSelect>(&command))
        return select->option + " = " + select->choice;
    if (const auto* fog = std::get_if<Doc::FogCmd>(&command)) return FogText(*fog);
    if (const auto* cycle = std::get_if<Doc::ClearCycleCmd>(&command))
        return ClearCycleText(*cycle);
    if (const auto* camera_ease = std::get_if<Doc::CameraEaseCmd>(&command))
        return CameraEaseText(*camera_ease);
    if (const auto* model_ease = std::get_if<Doc::ModelEaseCmd>(&command))
        return ModelEaseText(*model_ease);
    if (const auto* motion = std::get_if<Doc::CameraMotionCmd>(&command))
        return CameraMotionText(*motion);
    if (const auto* poly = std::get_if<Doc::PolyTileGrid>(&command)) return PolyTileGridText(*poly);
    return TypeName(Doc::TypeOf(command));
}

std::string ClipSummary(const Doc::Clip& clip, std::string_view target) {
    if (!clip.label.empty()) return clip.label;
    std::string out = CommandSummary(clip.command, target);
    const Doc::CommandType type = Doc::TypeOf(clip.command);
    const bool tween =
        type == Doc::CommandType::ModelTween || type == Doc::CommandType::CameraTween;
    if (tween || !clip.keys.empty()) {
        Append(out, std::to_string(clip.keys.size()) + " key(s)");
    }
    return out;
}

}
