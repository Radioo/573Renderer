#pragma once

#include <cstdint>
#include <string>

namespace AfpProfiles {

struct DllOffsetSet {
    uintptr_t afp_callback_table;
    uintptr_t afp_render_flags;
    uintptr_t afp_nearfar_slot;

    uintptr_t afpu_data_struct;
    uintptr_t afpu_render_context;

    uintptr_t afpu_set_screen_rect_fn;

    uintptr_t afp_table_b_count;
    uintptr_t afpu_shapes_a;
    uintptr_t afpu_shapes_b;
    uintptr_t afpu_drawn;
    uintptr_t afpu_world_mat_type;
    uintptr_t afpu_world_mat;
};

extern const DllOffsetSet kFallbackIidxOffsets;

const DllOffsetSet& ActiveOffsets();
void SetActiveOffsets(const DllOffsetSet& offsets);

struct AfpConfig {
    const char* slug;

    const char* avs_dll = "avs2-core.dll";
    const char* afp_dll = "afp-core.dll";
    const char* afpu_dll = "afp-utils.dll";

    DllOffsetSet offsets;

    bool call_afp_set_stream_nr = true;
    bool call_afp_stream_create_test = false;
    bool call_afp_render_init = true;
    bool call_afpu_render_init = true;
    bool call_afpu_set_config = true;
    bool call_afpu_set_flag_setup = true;
    bool call_afpu_boot = true;

    bool afpu_set_config_safe_clean_pos = false;

    bool call_afp_set_flag_setup = true;

    bool apply_iidx_data_segment_patches = true;

    bool afp_set_afp_data_wide_args = false;

    bool afp_set_verbose_wide_args = false;

    bool scan_arc_containers = false;

    float time_scale = 1.0f;

    bool skip_explicit_afp_set_afp_data = false;
};

const AfpConfig* For(const std::string& slug);

}
