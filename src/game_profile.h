#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GameProfile {

constexpr int kSkip = -1;

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

struct AfpOrdinals {
    int afp_set_afp_data = 0x000;
    int afp_get_afp_data = 0x001;
    int afp_boot = 0x002;
    int afp_shutdown = 0x003;
    int afp_set_flag = 0x005;
    int afp_set_verbose = 0x008;
    int afp_set_global_speed = 0x00a;
    int afp_set_bg_color = 0x00b;
    int afp_do_render = 0x00d;
    int afp_do_update = 0x00e;
    int afp_render_init = 0x00f;
    int afp_render_destroy = 0x010;
    int afp_do_sort_render = 0x011;
    int afp_set_create_level = 0x013;
    int afp_get_create_level = 0x014;
    int afp_stream_control = 0x015;
    int afp_stream_create = 0x018;
    int afp_stream_set_data = 0x019;
    int afp_stream_get_work = 0x01a;
    int afp_set_stream_nr = 0x01d;
    int afp_stream_play = 0x01e;
    int afp_stream_get_name = 0x01f;
    int afp_stream_destroy = 0x020;
    int afp_stream_set_speed = 0x02a;
    int afp_stream_set_matrix = 0x02c;
    int afp_stream_get_matrix = 0x02d;
    int afp_stream_set_translate = 0x02e;
    int afp_set_flag_mask = 0x037;
    int afp_system_dump_layer_info = 0x043;
    int afp_data_get_info = 0x044;
    int afp_data_get_stream_info = 0x045;
    int afp_get_layer_info = 0x046;
    int afp_get_data_id_by_name = 0x047;
    int afp_get_layers_by_nr = 0x04b;
    int afp_mc_get_id_by_path = 0x066;
    int afp_mc_get_relative_id = 0x069;
    int afp_mc_control = 0x071;
    int afp_mc_get = 0x073;
    int afp_ext_command = 0x086;
    int afp_play_work_load_bitmap = 0x087;
};

struct Profile {
    const char* name;
    const char* slug;
    const char* dir_substring;

    const char* avs_dll = "avs2-core.dll";
    const char* afp_dll = "afp-core.dll";
    const char* afpu_dll = "afp-utils.dll";
    const char* game_dll = nullptr;

    AfpOrdinals afp;

    int default_render_w;
    int default_render_h;

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

    bool legacy_afp = false;

    float time_scale = 1.0f;

    bool skip_explicit_afp_set_afp_data = false;
};

const std::vector<Profile>& All();

const Profile* AutoDetect(const std::string& dir);

const Profile* BySlug(const std::string& slug);

}
