#include "backend/afp_profiles.h"
#include "judge/interp_layout.h"

#include <string>
#include <vector>

namespace AfpProfiles {

const DllOffsetSet kFallbackIidxOffsets = {
    .afp_callback_table = 0xE0E08,
    .afp_render_flags = 0xE1134,
    .afp_nearfar_slot = 0xE0E70,
    .afp_mc_work_from_id = 0x48AC0,
    .afp_mc_def_from_work = 0x377B0,
    .afp_matrix_stack = 0xE1050,
    .afp_matrix_depth = 0xE1062,
    .afpu_data_struct = 0x281F0,
    .afpu_render_context = 0x28880,
    .afpu_set_screen_rect_fn = 0x18550,
    .afp_table_b_count = 0xE1142,
    .afpu_shapes_a = 0x288AC,
    .afpu_shapes_b = 0x288B0,
    .afpu_drawn = 0x289E4,
    .afpu_world_mat_type = 0x2B8C5,
    .afpu_world_mat = 0x2B880,
};

namespace {

constexpr DllOffsetSet kIidx33Offsets = {
    .afp_callback_table = 0xE0E08,
    .afp_render_flags = 0xE1134,
    .afp_nearfar_slot = 0xE0E70,
    .afp_mc_work_from_id = 0x48AC0,
    .afp_mc_def_from_work = 0x377B0,
    .afp_matrix_stack = 0xE1050,
    .afp_matrix_depth = 0xE1062,
    .afpu_data_struct = 0x281F0,
    .afpu_render_context = 0x28880,
    .afpu_set_screen_rect_fn = 0x18550,
    .afp_table_b_count = 0xE1142,
    .afpu_shapes_a = 0x288AC,
    .afpu_shapes_b = 0x288B0,
    .afpu_drawn = 0x289E4,
    .afpu_world_mat_type = 0x2B8C5,
    .afpu_world_mat = 0x2B880,
};

constexpr DllOffsetSet kDdrWorldOffsets = {
    .afp_callback_table = 0xE0E08,
    .afp_render_flags = 0xE1134,
    .afp_nearfar_slot = 0xE0E70,
    .afp_mc_work_from_id = 0,
    .afp_mc_def_from_work = 0,
    .afp_matrix_stack = 0,
    .afp_matrix_depth = 0,
    .afpu_data_struct = 0x281F0,
    .afpu_render_context = 0x28880,
    .afpu_set_screen_rect_fn = 0x18550,
    .afp_table_b_count = 0xE1142,
    .afpu_shapes_a = 0x288AC,
    .afpu_shapes_b = 0x288B0,
    .afpu_drawn = 0x289E4,
    .afpu_world_mat_type = 0x2B8C5,
    .afpu_world_mat = 0x2B880,
};

constexpr DllOffsetSet kIidx34Offsets = {
    .afp_callback_table = 0xF20A8,
    .afp_render_flags = 0xF23D4,
    .afp_nearfar_slot = 0xF2110,
    .afp_mc_work_from_id = 0x49D60,
    .afp_mc_def_from_work = 0x52D00,
    .afp_matrix_stack = 0xF2340,
    .afp_matrix_depth = 0xF2352,
    .afpu_data_struct = 0x2B2D0,
    .afpu_render_context = 0x2B8B8,
    .afpu_set_screen_rect_fn = 0x15790,
    .afp_table_b_count = 0,
    .afpu_shapes_a = 0,
    .afpu_shapes_b = 0,
    .afpu_drawn = 0,
    .afpu_world_mat_type = 0,
    .afpu_world_mat = 0,
};

constexpr DllOffsetSet kIidx26Offsets = {
    .afp_callback_table = 0x189988,
    .afp_render_flags = 0x189CB4,
    .afp_nearfar_slot = 0x1899F0,
    .afp_mc_work_from_id = 0,
    .afp_mc_def_from_work = 0,
    .afp_matrix_stack = 0,
    .afp_matrix_depth = 0,
    .afpu_data_struct = 0x431A0,
    .afpu_render_context = 0x43850,
    .afpu_set_screen_rect_fn = 0x30CB0,
    .afp_table_b_count = 0,
    .afpu_shapes_a = 0,
    .afpu_shapes_b = 0,
    .afpu_drawn = 0,
    .afpu_world_mat_type = 0,
    .afpu_world_mat = 0,
};

constexpr DllOffsetSet kSdvx7Offsets = {
    .afp_callback_table = 0xED008,
    .afp_render_flags = 0xED334,
    .afp_nearfar_slot = 0xED070,
    .afp_mc_work_from_id = 0,
    .afp_mc_def_from_work = 0,
    .afp_matrix_stack = 0,
    .afp_matrix_depth = 0,
    .afpu_data_struct = 0x2B2C0,
    .afpu_render_context = 0x2B958,
    .afpu_set_screen_rect_fn = 0x199B0,
    .afp_table_b_count = 0,
    .afpu_shapes_a = 0,
    .afpu_shapes_b = 0,
    .afpu_drawn = 0,
    .afpu_world_mat_type = 0,
    .afpu_world_mat = 0,
};

constexpr DllOffsetSet kGitadoraDeltaOffsets = {
    .afp_callback_table = 0xEE048,
    .afp_render_flags = 0xEE374,
    .afp_nearfar_slot = 0xEE0B0,
    .afp_mc_work_from_id = 0,
    .afp_mc_def_from_work = 0,
    .afp_matrix_stack = 0,
    .afp_matrix_depth = 0,
    .afpu_data_struct = 0x2A2D0,
    .afpu_render_context = 0x2A8A8,
    .afpu_set_screen_rect_fn = 0x15720,
    .afp_table_b_count = 0,
    .afpu_shapes_a = 0,
    .afpu_shapes_b = 0,
    .afpu_drawn = 0,
    .afpu_world_mat_type = 0,
    .afpu_world_mat = 0,
};

constexpr DllOffsetSet kT44Offsets = {
    .afp_callback_table = 0xE0E08,
    .afp_render_flags = 0xE1134,
    .afp_nearfar_slot = 0xE0E70,
    .afp_mc_work_from_id = 0,
    .afp_mc_def_from_work = 0,
    .afp_matrix_stack = 0,
    .afp_matrix_depth = 0,
    .afpu_data_struct = 0x281F0,
    .afpu_render_context = 0x28880,
    .afpu_set_screen_rect_fn = 0x18810,
    .afp_table_b_count = 0,
    .afpu_shapes_a = 0,
    .afpu_shapes_b = 0,
    .afpu_drawn = 0,
    .afpu_world_mat_type = 0,
    .afpu_world_mat = 0,
};

constexpr DllOffsetSet kPopn29Offsets = {
    .afp_callback_table = 0xF20A8,
    .afp_render_flags = 0xF23D4,
    .afp_nearfar_slot = 0xF2110,
    .afp_mc_work_from_id = 0,
    .afp_mc_def_from_work = 0,
    .afp_matrix_stack = 0,
    .afp_matrix_depth = 0,
    .afpu_data_struct = 0x2B2D0,
    .afpu_render_context = 0x2B8B8,
    .afpu_set_screen_rect_fn = 0x15790,
    .afp_table_b_count = 0,
    .afpu_shapes_a = 0,
    .afpu_shapes_b = 0,
    .afpu_drawn = 0,
    .afpu_world_mat_type = 0,
    .afpu_world_mat = 0,
};

constexpr AfpInterp::Layout kLibafp2103Interp = {
    .layer_advance = {.name = "AfpLayer_Advance",
                      .rva = 0x6F3C,
                      .prologue = {0x57, 0x56, 0x55, 0x53, 0x83, 0xEC, 0x1C},
                      .prologue_size = 7},
    .layer_draw = {.name = "AfpLayer_DrawFrame",
                   .rva = 0x7C70,
                   .prologue = {0x57, 0x56, 0x55, 0x53, 0x81, 0xEC, 0x10, 0x01, 0x00, 0x00},
                   .prologue_size = 10},
    .object_init = {.name = "AfpObj_Init",
                    .rva = 0x23E90,
                    .prologue = {0x55, 0x8B, 0x6C, 0x24, 0x08},
                    .prologue_size = 5},
    .clip_goto = {.name = "AfpClip_GotoFrame",
                  .rva = 0x25C1C,
                  .prologue = {0x57, 0x56, 0x55, 0x53, 0x8B, 0x6C, 0x24, 0x14},
                  .prologue_size = 8},
    .clip_set_frame = {.name = "AfpClip_SetFrameDirect",
                       .rva = 0x25BEC,
                       .prologue = {0x57, 0x56, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0x74},
                       .prologue_size = 8},
    .layer_attributes = 0,
    .layer_frozen_bit = 0x40,
    .layer_id = 20,
    .layer_speed = 44,
    .layer_fps = 48,
    .layer_accumulator = 56,
    .layer_root = 164,
    .object_flags = 36,
    .object_dead_bit = 0x4,
    .object_script_bit = 0x40000,
    .object_depth = 60,
    .object_created = 142,
    .object_lifetime_end = 144,
    .object_current_frame = 140,
    .object_matrix = 68,
    .object_anchor = 108,
    .object_color = 116,
    .object_matrix3d = 120,
    .object_camera = 124,
    .object_first_child = 156,
    .object_character = 160,
    .object_parent = 172,
    .object_next = 180,
    .camera_fov = 4,
    .camera_fields = 8,
};

constexpr float kSdvx1FixedStep = 1.0F / 60.0F;

const std::vector<AfpConfig> kConfigs = {
    AfpConfig{
        .slug = "iidx34",
        .offsets = kIidx34Offsets,
    },
    AfpConfig{
        .slug = "iidx33",
        .offsets = kIidx33Offsets,
    },
    AfpConfig{
        .slug = "iidx26",
        .offsets = kIidx26Offsets,

        .call_afp_set_stream_nr = true,
        .call_afp_stream_create_test = false,
        .call_afp_render_init = false,
        .call_afpu_render_init = true,
        .call_afpu_set_config = true,
        .call_afpu_set_flag_setup = true,
        .afpu_set_flag_calls = {{.flags = 4, .mask = 0},
                                {.flags = 8, .mask = 8},
                                {.flags = 16, .mask = 16}},
        .call_afpu_boot = true,
        .afpu_set_config_safe_clean_pos = true,
        .call_afp_set_flag_setup = true,
        .afp_set_flag_calls = {{.flags = 16, .mask = 16}, {.flags = 8, .mask = 8}},
        .apply_iidx_data_segment_patches = true,
        .afp_set_afp_data_wide_args = false,
        .afp_set_verbose_wide_args = false,
        .skip_explicit_afp_set_afp_data = true,
    },
    AfpConfig{
        .slug = "sdvx7",
        .offsets = kSdvx7Offsets,

        .call_afp_set_stream_nr = true,
        .call_afp_stream_create_test = false,
        .call_afp_render_init = true,
        .call_afpu_render_init = true,
        .call_afpu_set_config = true,
        .call_afpu_set_flag_setup = false,
        .call_afpu_boot = true,
        .afpu_set_config_safe_clean_pos = true,
        .call_afp_set_flag_setup = false,

        .apply_iidx_data_segment_patches = true,

        .afp_set_afp_data_wide_args = true,
        .afp_set_verbose_wide_args = true,
    },
    AfpConfig{
        .slug = "iidx24",
        .avs_dll = "libavs-win32.dll",
        .afp_dll = "libafp-win32.dll",
        .afpu_dll = "libafputils-win32.dll",
        .avs_generation = AvsGeneration::Avs2161,
        .offsets = kIidx33Offsets,
    },
    AfpConfig{
        .slug = "iidx20",
        .avs_dll = "libavs-win32.dll",
        .afp_dll = "libafp-win32.dll",
        .afpu_dll = "libafputils-win32.dll",
        .avs_generation = AvsGeneration::Avs2158,
        .offsets = kIidx33Offsets,
    },
    AfpConfig{
        .slug = "iidx18",
        .avs_dll = "libavs-win32.dll",
        .afp_dll = "libafp-win32.dll",
        .afpu_dll = nullptr,
        .avs_generation = AvsGeneration::Avs2134,
        .offsets = kIidx33Offsets,
        .scan_txp2_packages = true,
    },
    AfpConfig{
        .slug = "iidx19",
        .avs_dll = "libavs-win32.dll",
        .afp_dll = "libafp-win32.dll",
        .afpu_dll = nullptr,
        .avs_generation = AvsGeneration::Avs2134,
        .offsets = kIidx33Offsets,
        .scan_txp2_packages = true,
    },
    AfpConfig{
        .slug = "sdvx1",
        .avs_dll = "libavs-win32.dll",
        .afp_dll = "libafp-win32.dll",
        .afpu_dll = nullptr,
        .avs_generation = AvsGeneration::Avs2158,
        .offsets = kIidx33Offsets,
        .scan_txp2_packages = true,
        .interp_layout = &kLibafp2103Interp,
        .fixed_step_seconds = kSdvx1FixedStep,
    },
    AfpConfig{
        .slug = "ddrworld",
        .avs_dll = "libavs-win64.dll",
        .afp_dll = "libafp-win64.dll",
        .afpu_dll = "libafputils-win64.dll",
        .offsets = kDdrWorldOffsets,
        .scan_arc_containers = true,
        .time_scale = 1.0F,
    },
    AfpConfig{
        .slug = "gitadora",
        .offsets = kGitadoraDeltaOffsets,

        .call_afp_set_stream_nr = true,
        .call_afp_stream_create_test = false,
        .call_afp_render_init = false,
        .call_afpu_render_init = true,
        .call_afpu_set_config = true,
        .call_afpu_set_flag_setup = true,
        .call_afpu_boot = true,
        .afpu_set_config_safe_clean_pos = true,
        .call_afp_set_flag_setup = true,
        .apply_iidx_data_segment_patches = true,
        .afp_set_afp_data_wide_args = false,
        .afp_set_verbose_wide_args = false,
        .skip_explicit_afp_set_afp_data = true,
    },
    AfpConfig{
        .slug = "t44",
        .offsets = kT44Offsets,

        .call_afp_set_stream_nr = true,
        .call_afp_stream_create_test = false,
        .call_afp_render_init = false,
        .call_afpu_render_init = true,
        .call_afpu_set_config = true,
        .call_afpu_set_flag_setup = true,
        .call_afpu_boot = true,
        .afpu_set_config_safe_clean_pos = true,
        .call_afp_set_flag_setup = true,
        .apply_iidx_data_segment_patches = true,
        .afp_set_afp_data_wide_args = false,
        .afp_set_verbose_wide_args = false,
        .skip_explicit_afp_set_afp_data = true,
    },
    AfpConfig{
        .slug = "popn29",
        .offsets = kPopn29Offsets,

        .call_afp_set_stream_nr = true,
        .call_afp_stream_create_test = false,
        .call_afp_render_init = false,
        .call_afpu_render_init = true,
        .call_afpu_set_config = true,
        .call_afpu_set_flag_setup = true,
        .afpu_set_flag_calls = {{.flags = 4, .mask = 0},
                                {.flags = 8, .mask = 8},
                                {.flags = 16, .mask = 16}},
        .call_afpu_boot = true,
        .afpu_set_config_safe_clean_pos = true,
        .call_afp_set_flag_setup = true,
        .afp_set_flag_calls = {{.flags = 16, .mask = 16}, {.flags = 8, .mask = 8}},
        .apply_iidx_data_segment_patches = true,
        .afp_set_afp_data_wide_args = false,
        .afp_set_verbose_wide_args = false,
        .skip_explicit_afp_set_afp_data = true,
    },
};

const DllOffsetSet* g_active_offsets = &kFallbackIidxOffsets;

}

const DllOffsetSet& ActiveOffsets() {
    return *g_active_offsets;
}

void SetActiveOffsets(const DllOffsetSet& offsets) {
    g_active_offsets = &offsets;
}

const AfpConfig* For(const std::string& slug) {
    for (const auto& c : kConfigs) {
        if (slug == c.slug) return &c;
    }
    return nullptr;
}

}
