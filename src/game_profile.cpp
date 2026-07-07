#include "game_profile.h"

#include <filesystem>
#include <system_error>

#include <cctype>
#include <vector>
#include <string>

namespace GameProfile {

namespace {}
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

const std::vector<Profile> kProfiles = {
    Profile{
        .name = "IIDX 33 (Sparkle Shower)",
        .slug = "iidx33",
        .dir_substring = "iidx",
        .game_dll = "bm2dx.dll",
        .afp = AfpOrdinals{},
        .default_render_w = 1920,
        .default_render_h = 1080,
        .offsets = kIidx33Offsets,
    },
    Profile{
        .name = "SDVX 7 (NABLA)",
        .slug = "sdvx7",
        .dir_substring = "sdvx",
        .game_dll = "soundvoltex.dll",
        .afp = AfpOrdinals{},
        .default_render_w = 1080,
        .default_render_h = 1920,
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
    Profile{
        .name = "DDR World (MDX)",
        .slug = "ddrworld",
        .dir_substring = "mdx",
        .avs_dll = "libavs-win64.dll",
        .afp_dll = "libafp-win64.dll",
        .afpu_dll = "libafputils-win64.dll",
        .game_dll = "gamemdx.dll",
        .afp = AfpOrdinals{},
        .default_render_w = 1280,
        .default_render_h = 720,
        .offsets = kIidx33Offsets,
        .scan_arc_containers = true,
        .legacy_afp = true,
        .time_scale = 1.0F,
    },
    Profile{
        .name = "GITADORA DELTA",
        .slug = "gitadora",
        .dir_substring = "delta",
        .game_dll = "gdxg.dll",
        .afp = AfpOrdinals{},
        .default_render_w = 3840,
        .default_render_h = 2160,
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
};

std::string ToLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char const c : s)
        out.push_back((char)std::tolower((unsigned char)c));
    return out;
}

}

const std::vector<Profile>& All() {
    return kProfiles;
}

namespace {

bool GameDllPresent(const std::string& dir, const char* game_dll) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root(dir);
    const fs::path candidates[] = {
        root / "modules",
        root / "contents" / "modules",
        root,
    };
    for (const auto& c : candidates) {
        if (fs::exists(c / game_dll, ec)) return true;
    }
    return false;
}

}

const Profile* AutoDetect(const std::string& dir) {
    if (dir.empty()) return nullptr;
    const std::string dir_lc = ToLower(dir);
    for (const auto& p : kProfiles) {
        if ((p.dir_substring == nullptr) || (*p.dir_substring == 0)) continue;
        std::string const needle = ToLower(p.dir_substring);
        if (dir_lc.find(needle) != std::string::npos) return &p;
    }
    for (const auto& p : kProfiles) {
        if (p.game_dll == nullptr) continue;
        if (GameDllPresent(dir, p.game_dll)) return &p;
    }
    return nullptr;
}

const Profile* BySlug(const std::string& slug) {
    if (slug.empty()) return nullptr;
    for (const auto& p : kProfiles) {
        if (slug == p.slug) return &p;
    }
    return nullptr;
}

namespace {
const DllOffsetSet* g_active_offsets = &kFallbackIidxOffsets;
}

const DllOffsetSet& ActiveOffsets() {
    return *g_active_offsets;
}

void SetActiveOffsets(const DllOffsetSet& offsets) {
    g_active_offsets = &offsets;
}

}
