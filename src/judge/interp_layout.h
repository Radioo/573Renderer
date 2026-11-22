#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace AfpInterp {

struct HookSite {
    const char* name;
    std::uint32_t rva;
    std::array<std::uint8_t, 10> prologue;
    std::size_t prologue_size;
};

struct Layout {
    HookSite layer_advance;
    HookSite layer_draw;
    HookSite object_init;
    HookSite clip_goto;
    HookSite clip_set_frame;

    std::uint32_t layer_attributes;
    std::uint32_t layer_frozen_bit;
    std::uint32_t layer_id;
    std::uint32_t layer_speed;
    std::uint32_t layer_fps;
    std::uint32_t layer_accumulator;
    std::uint32_t layer_root;

    std::uint32_t object_flags;
    std::uint32_t object_dead_bit;
    std::uint32_t object_script_bit;
    std::uint32_t object_depth;
    std::uint32_t object_created;
    std::uint32_t object_lifetime_end;
    std::uint32_t object_current_frame;
    std::uint32_t object_matrix;
    std::uint32_t object_anchor;
    std::uint32_t object_color;
    std::uint32_t object_matrix3d;
    std::uint32_t object_camera;
    std::uint32_t object_first_child;
    std::uint32_t object_character;
    std::uint32_t object_parent;
    std::uint32_t object_next;

    std::uint32_t camera_fov;
    std::uint32_t camera_fields;
};

using ReadImage = std::function<bool(std::uint32_t rva, std::size_t size, std::uint8_t* out)>;

[[nodiscard]] std::string PrologueMismatch(const Layout& layout, const ReadImage& read);

}
