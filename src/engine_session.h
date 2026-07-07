#pragma once
#include "support/dll_loader.h"
#include "avs_funcs.h"
#include "afp_funcs.h"
#include "afpu_funcs.h"
#include "render_backend.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GameProfile {
struct Profile;
}

struct CompanionRecord {
    uint32_t pkg_id;
    std::string mountpoint;
};

struct EngineSession {
    DllLoader avs_dll;
    DllLoader afp_dll;
    DllLoader afpu_dll;

    AvsFuncs avs;
    AfpFuncs afp;
    AfpuFuncs afpu;

    AfpRenderContext render_ctx;

    const GameProfile::Profile* active_profile = nullptr;
    bool afp_booted = false;
    uint32_t pkg_id = 0;
    uint32_t stream_id = 0xFFFFFFFC;
    std::vector<uint32_t> extra_streams;
    std::string anim_name;
    std::vector<uint32_t> persistent_pkg_ids;
    std::vector<CompanionRecord> companions;
    int next_companion_idx = 0;
};
