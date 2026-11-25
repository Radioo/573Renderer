#include "judge_hooks/afp_interp_hooks.h"

#include "judge/afp_pose.h"
#include "judge/interp_layout.h"
#include "judge/interp_tracks.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <safetyhook/inline_hook.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace AfpInterpHooks {

namespace {

struct LayerState {
    AfpInterp::LayerTracks tracks;
    LayerStats stats;
};

struct Swapped {
    std::uintptr_t object;
    AfpPose::Pose live;
    std::uintptr_t color_pointer;
    float fov;
};

AfpInterp::Layout g_layout{};
bool g_installed = false;
AfpPose::MatrixLerp g_matrix_lerp = AfpPose::MatrixLerp::Decompose;
float g_render_time = 0.0F;
std::set<std::uint32_t> g_interpolated;
std::map<std::uint32_t, LayerState> g_layers;
AfpInterp::LayerTracks* g_advancing = nullptr;
AdvanceObserver g_observer = nullptr;
std::vector<std::array<float, 8>> g_color_blocks;

safetyhook::InlineHook g_advance_hook;
safetyhook::InlineHook g_draw_hook;
safetyhook::InlineHook g_init_hook;
safetyhook::InlineHook g_goto_hook;
safetyhook::InlineHook g_set_frame_hook;

template <typename T> T Read(std::uintptr_t address) {
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

template <typename T> void Write(std::uintptr_t address, const T& value) {
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
}

std::uintptr_t ReadPointer(std::uintptr_t address) {
    return static_cast<std::uintptr_t>(Read<std::uint32_t>(address));
}

void WritePointer(std::uintptr_t address, std::uintptr_t value) {
    Write(address, static_cast<std::uint32_t>(value));
}

AfpPose::Pose ReadPose(std::uintptr_t obj) {
    const AfpInterp::Layout& l = g_layout;
    AfpPose::Pose pose;
    pose.character = Read<std::uint32_t>(obj + l.object_character);
    pose.depth = Read<std::int32_t>(obj + l.object_depth);
    pose.matrix = Read<std::array<float, 6>>(obj + l.object_matrix);
    pose.anchor = Read<std::array<float, 2>>(obj + l.object_anchor);
    if (const std::uintptr_t matrix3d = ReadPointer(obj + l.object_matrix3d)) {
        pose.has_matrix3d = true;
        pose.matrix3d = Read<std::array<float, 16>>(matrix3d);
    }
    if (const std::uintptr_t color = ReadPointer(obj + l.object_color)) {
        pose.has_color = true;
        pose.color = Read<std::array<float, 8>>(color);
    }
    if (const std::uintptr_t camera = ReadPointer(obj + l.object_camera)) {
        pose.has_camera = true;
        pose.camera = Read<std::array<float, 6>>(camera + l.camera_fields);
    }
    return pose;
}

void WriteGeometry(std::uintptr_t obj, const AfpPose::Pose& pose) {
    const AfpInterp::Layout& l = g_layout;
    Write(obj + l.object_matrix, pose.matrix);
    Write(obj + l.object_anchor, pose.anchor);
    if (pose.has_matrix3d) Write(ReadPointer(obj + l.object_matrix3d), pose.matrix3d);
    if (pose.has_camera) Write(ReadPointer(obj + l.object_camera) + l.camera_fields, pose.camera);
}

void Collect(std::uintptr_t first, std::vector<AfpInterp::Sample>& out) {
    const AfpInterp::Layout& l = g_layout;
    for (std::uintptr_t obj = first; obj != 0; obj = ReadPointer(obj + l.object_next)) {
        const auto flags = Read<std::uint32_t>(obj + l.object_flags);
        if ((flags & l.object_dead_bit) != 0) continue;
        out.push_back({.object = obj,
                       .parent = ReadPointer(obj + l.object_parent),
                       .script_controlled = (flags & l.object_script_bit) != 0,
                       .created = Read<std::uint16_t>(obj + l.object_created),
                       .lifetime_end = Read<std::uint16_t>(obj + l.object_lifetime_end),
                       .pose = ReadPose(obj)});
        Collect(ReadPointer(obj + l.object_first_child), out);
    }
}

std::vector<AfpInterp::Sample> LayerSamples(std::uintptr_t layer) {
    std::vector<AfpInterp::Sample> out;
    Collect(ReadPointer(layer + g_layout.layer_root), out);
    return out;
}

AfpInterp::LayerTiming Timing(std::uintptr_t layer) {
    const AfpInterp::Layout& l = g_layout;
    return {.fps = Read<float>(layer + l.layer_fps),
            .speed = Read<float>(layer + l.layer_speed),
            .frozen = (Read<std::uint32_t>(layer + l.layer_attributes) & l.layer_frozen_bit) != 0,
            .accumulator = Read<float>(layer + l.layer_accumulator)};
}

std::optional<std::uint32_t> InterpolatedId(std::uintptr_t layer) {
    if (layer == 0) return std::nullopt;
    const auto id = Read<std::uint32_t>(layer + g_layout.layer_id);
    if (!g_interpolated.contains(id)) return std::nullopt;
    return id;
}

int __cdecl HookedAdvance(std::uintptr_t layer, float dt) {
    const std::optional<std::uint32_t> id = InterpolatedId(layer);
    if (!id) return g_advance_hook.ccall<int>(layer, dt);
    LayerState& state = g_layers[*id];
    const std::vector<AfpInterp::Sample> before = LayerSamples(layer);
    const auto accumulator = Read<float>(layer + g_layout.layer_accumulator);
    state.tracks.BeginAdvance(before);
    g_advancing = &state.tracks;
    const int result = g_advance_hook.ccall<int>(layer, dt);
    g_advancing = nullptr;
    const int steps = AfpInterp::FrameSteps(Timing(layer), dt, accumulator);
    const std::vector<AfpInterp::Sample> after = LayerSamples(layer);
    if (g_observer != nullptr) g_observer(before, after, steps);
    state.tracks.EndAdvance(steps, after);
    state.stats.moving = state.tracks.MovingCount();
    return result;
}

int __cdecl HookedDraw(std::uintptr_t layer) {
    const std::optional<std::uint32_t> id = InterpolatedId(layer);
    if (!id) return g_draw_hook.ccall<int>(layer);
    LayerState& state = g_layers[*id];
    const float alpha = AfpInterp::Alpha(Timing(layer), g_render_time);
    std::vector<std::pair<Swapped, AfpPose::Pose>> swaps;
    for (const AfpInterp::Sample& sample : LayerSamples(layer)) {
        std::optional<AfpPose::Pose> pose =
            state.tracks.DrawPose(sample.object, sample.pose, alpha, g_matrix_lerp);
        if (!pose) continue;
        const std::uintptr_t camera = ReadPointer(sample.object + g_layout.object_camera);
        swaps.emplace_back(
            Swapped{.object = sample.object,
                    .live = sample.pose,
                    .color_pointer = ReadPointer(sample.object + g_layout.object_color),
                    .fov = camera != 0 ? Read<float>(camera + g_layout.camera_fov) : 0.0F},
            *pose);
    }
    g_color_blocks.clear();
    g_color_blocks.reserve(swaps.size());
    for (const auto& [saved, pose] : swaps) {
        WriteGeometry(saved.object, pose);
        if (pose.has_color) {
            g_color_blocks.push_back(pose.color);
            WritePointer(saved.object + g_layout.object_color,
                         reinterpret_cast<std::uintptr_t>(g_color_blocks.back().data()));
        }
        if (pose.has_camera) {
            Write(ReadPointer(saved.object + g_layout.object_camera) + g_layout.camera_fov,
                  AfpPose::CameraFov(pose.camera));
        }
    }
    state.stats.swapped = swaps.size();
    state.stats.alpha = alpha;
    const int result = g_draw_hook.ccall<int>(layer);
    for (const auto& [saved, pose] : swaps) {
        WriteGeometry(saved.object, saved.live);
        WritePointer(saved.object + g_layout.object_color, saved.color_pointer);
        if (saved.live.has_camera) {
            Write(ReadPointer(saved.object + g_layout.object_camera) + g_layout.camera_fov,
                  saved.fov);
        }
    }
    return result;
}

int __cdecl HookedInit(std::uintptr_t obj) {
    for (auto& [id, state] : g_layers)
        state.tracks.Forget(obj);
    return g_init_hook.ccall<int>(obj);
}

int __cdecl HookedGoto(std::uintptr_t clip, int frame) {
    if (g_advancing != nullptr) {
        g_advancing->MarkGoto(clip, Read<std::uint16_t>(clip + g_layout.object_current_frame),
                              static_cast<std::uint16_t>(frame));
    }
    return g_goto_hook.ccall<int>(clip, frame);
}

int __cdecl HookedSetFrame(std::uintptr_t clip, int frame) {
    if (g_advancing != nullptr) g_advancing->MarkJumped(clip);
    return g_set_frame_hook.ccall<int>(clip, frame);
}

std::size_t ImageSize(std::uintptr_t base) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->OptionalHeader.SizeOfImage;
}

std::string Detour(safetyhook::InlineHook& hook, std::uintptr_t base,
                   const AfpInterp::HookSite& site, void* destination) {
    auto created =
        safetyhook::InlineHook::create(reinterpret_cast<void*>(base + site.rva), destination);
    if (!created) return std::string("could not detour ") + site.name;
    hook = std::move(*created);
    return {};
}

}

std::string Install(std::uintptr_t libafp_base, const AfpInterp::Layout& layout) {
    if (g_installed) return {};
    if (libafp_base == 0) return "libafp is not loaded";
    const std::size_t size = ImageSize(libafp_base);
    const AfpInterp::ReadImage read = [&](std::uint32_t rva, std::size_t count, std::uint8_t* out) {
        if (static_cast<std::size_t>(rva) + count > size) return false;
        std::memcpy(out, reinterpret_cast<const void*>(libafp_base + rva), count);
        return true;
    };
    std::string problem = AfpInterp::PrologueMismatch(layout, read);
    if (!problem.empty()) return problem;
    g_layout = layout;
    const std::array<
        std::pair<const AfpInterp::HookSite*, std::pair<safetyhook::InlineHook*, void*>>, 5>
        sites{{{&layout.object_init, {&g_init_hook, reinterpret_cast<void*>(&HookedInit)}},
               {&layout.clip_goto, {&g_goto_hook, reinterpret_cast<void*>(&HookedGoto)}},
               {&layout.clip_set_frame,
                {&g_set_frame_hook, reinterpret_cast<void*>(&HookedSetFrame)}},
               {&layout.layer_advance, {&g_advance_hook, reinterpret_cast<void*>(&HookedAdvance)}},
               {&layout.layer_draw, {&g_draw_hook, reinterpret_cast<void*>(&HookedDraw)}}}};
    for (const auto& [site, target] : sites) {
        problem = Detour(*target.first, libafp_base, *site, target.second);
        if (!problem.empty()) {
            Uninstall();
            return problem;
        }
    }
    g_installed = true;
    return {};
}

void Uninstall() {
    g_advance_hook = {};
    g_draw_hook = {};
    g_init_hook = {};
    g_goto_hook = {};
    g_set_frame_hook = {};
    g_layers.clear();
    g_interpolated.clear();
    g_installed = false;
}

bool Installed() {
    return g_installed;
}

void SetLayerInterpolated(std::uint32_t layer_id, bool on) {
    if (on) {
        g_interpolated.insert(layer_id);
    } else {
        g_interpolated.erase(layer_id);
        g_layers.erase(layer_id);
    }
}

void SetMatrixLerp(AfpPose::MatrixLerp mode) {
    g_matrix_lerp = mode;
}

void SetRenderTime(float seconds_since_advance) {
    g_render_time = seconds_since_advance;
}

void SetAdvanceObserver(AdvanceObserver observer) {
    g_observer = observer;
}

LayerStats StatsFor(std::uint32_t layer_id) {
    const auto found = g_layers.find(layer_id);
    return found == g_layers.end() ? LayerStats{} : found->second.stats;
}

}
