#include "backend/afp_profiles.h"

#include <string>
#include <vector>

namespace AfpProfiles {

const DllOffsetSet kFallbackIidxOffsets = {
    .afp_callback_table = 0xE0E08,
    .afp_render_flags = 0xE1134,
    .afp_nearfar_slot = 0xE0E70,
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

constexpr DllOffsetSet kIidx26Offsets = {
    .afp_callback_table = 0x189988,
    .afp_render_flags = 0x189CB4,
    .afp_nearfar_slot = 0x1899F0,
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

const std::vector<AfpConfig> kConfigs = {
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
        .slug = "ddrworld",
        .avs_dll = "libavs-win64.dll",
        .afp_dll = "libafp-win64.dll",
        .afpu_dll = "libafputils-win64.dll",
        .offsets = kIidx33Offsets,
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
