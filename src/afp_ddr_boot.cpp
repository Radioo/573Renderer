#include <string>
#include <optional>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "afp_ddr.h"
#include "afp_ddr_funcs.h"
#include "afp_ddr_render.h"
#include "avs_funcs.h"
#include "avs_boot.h"
#include "support/dll_loader.h"
#include "render_backend.h"
#include "support/env.h"
#include "support/log.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace DdrAfp {
namespace {
AfpDdrFuncs g_afp;
AfpuDdrFuncs g_afpu;
bool g_booted = false;
int g_stream_data_id = -1;
uint32_t g_layer_id = 0;
uint32_t g_root_stream_id = 0;
uintptr_t g_afpu_base = 0;
uint32_t g_loop_frames = 0;
float g_time_scale = 1.0F;
struct DdrClip {
    std::string name;
    uint32_t stream_id = 0;
    uint32_t layer_id = 0;
};
std::vector<DdrClip> g_clips;
std::string g_active_clip;
}

void SetTimeScale(float s) {
    g_time_scale = (s > 0.0F) ? s : 1.0F;
}

namespace {

void ApplyDdrAfpAttributes() {
    if (g_afp.afp_set_stream_max_nr != nullptr) g_afp.afp_set_stream_max_nr(2048);
    int policy = 1;
    if (std::optional<std::string> e = Support::EnvVar("DDR_POLICY"))
        policy = (int)strtol(e->c_str(), nullptr, 0);
    if (g_afp.afp_set_policy != nullptr) g_afp.afp_set_policy(policy);
    LOG("DDR", "afp_set_policy(%d)...", policy);
    if (g_afp.afp_system_set_attribute != nullptr) {
        if (std::optional<std::string> e = Support::EnvVar("DDR_SYS_ATTR")) {
            int const v = (int)strtol(e->c_str(), nullptr, 0);
            g_afp.afp_system_set_attribute(~0, v);
            LOG("DDR", "afp_system_set_attribute(0xFFFFFFFF, %#x) [DDR_SYS_ATTR]", v);
        } else {
            g_afp.afp_system_set_attribute(16, 16);
            g_afp.afp_system_set_attribute(8, 8);
            g_afp.afp_system_set_attribute(1, 1);
            LOG("DDR",
                "afp_system_set_attribute -> dword_180244FE4=0x819 (game value, bit0 mask on)");
        }
    }
}

void ApplyDdrAfpuAttributes(void* render_params, void* afpu_config) {
    if (g_afpu.afpu_set_afp_render_params != nullptr)
        g_afpu.afpu_set_afp_render_params(render_params);
    if (g_afpu.afpu_set_render_params != nullptr) g_afpu.afpu_set_render_params(afpu_config);

    if (g_afpu.afpu_system_set_parameter != nullptr) g_afpu.afpu_system_set_parameter(1, 4096);
    if (g_afpu.afpu_system_set_attributes != nullptr) {
        if (std::optional<std::string> e = Support::EnvVar("DDR_AFPU_ATTR")) {
            int const v = (int)strtol(e->c_str(), nullptr, 0);
            g_afpu.afpu_system_set_attributes(~0, v);
            LOG("DDR", "afpu_system_set_attributes(0xFFFFFFFF, %#x) [DDR_AFPU_ATTR]", v);
        } else {
            g_afpu.afpu_system_set_attributes(4, 0);
            g_afpu.afpu_system_set_attributes(8, 8);
            g_afpu.afpu_system_set_attributes(16, 16);
        }
    }
}

}

bool Boot(DllLoader& afp_dll, DllLoader& afpu_dll, D3D9State& d3d) {
    LOG("DDR", "Resolving DDR afp/afpu exports (by name)...");
    if (!g_afp.Load(afp_dll)) {
        LOG("DDR", "FAILED to resolve libafp-win64 exports");
        return false;
    }
    if (!g_afpu.Load(afpu_dll)) {
        LOG("DDR", "FAILED to resolve libafputils-win64 exports");
        return false;
    }
    g_afpu_base = reinterpret_cast<uintptr_t>(afpu_dll.Module());

    if (g_afp.afp_ext_command != nullptr) {
        const char* ver = nullptr;
        g_afp.afp_ext_command(9, static_cast<void*>(&ver));
        if (ver != nullptr) LOG("DDR", "AFP version: %s", ver);
    }

    DdrRender::Init(d3d.device, d3d.width, d3d.height);
    DdrRender::SetTexBindResolver(g_afpu.afpu_get_texture_bind_id);

    void* render_params = DdrRender::RenderParams();
    void* afpu_config = DdrRender::AfpuConfig();

    LOG("DDR", "afp_boot(render_params=%p)...", render_params);
    int ret = g_afp.afp_boot(render_params);
    LOG("DDR", "afp_boot -> %d", ret);

    LOG("DDR", "afp_set_stream_max_nr(2048)...");
    ApplyDdrAfpAttributes();

    LOG("DDR", "afpu_boot(0, afpu_config=%p)...", afpu_config);
    ret = g_afpu.afpu_boot(nullptr, afpu_config);
    LOG("DDR", "afpu_boot -> %d", ret);

    ApplyDdrAfpuAttributes(render_params, afpu_config);

    g_booted = true;
    LOG("DDR", "AFP 2.13.7 booted.");
    return true;
}

namespace {

struct PkgClipTable {
    uint16_t count = 0;
    uintptr_t arr = 0;
    uintptr_t rec = 0;
};

PkgClipTable FindPkgClipTable(int data_id) {
    PkgClipTable out;
    if (g_afpu_base == 0U) return out;
    uintptr_t const tbl = *reinterpret_cast<uintptr_t*>(g_afpu_base + 0x48450);
    int const idx = (data_id >> 15) & 0xFF;
    uintptr_t const rec =
        (tbl != 0U) ? *reinterpret_cast<uintptr_t*>(tbl + ((uintptr_t)idx * 8)) : 0;
    if (rec == 0U) return out;
    out.rec = rec;
    out.count = *reinterpret_cast<uint16_t*>(rec + 0x14);
    out.arr = *reinterpret_cast<uintptr_t*>(rec + 0x50);
    return out;
}

const char* PkgClipName(uintptr_t arr, uint16_t i) {
    return *reinterpret_cast<const char**>(arr + ((uintptr_t)i * 40) + 16);
}

uint32_t PkgClipStreamId(uintptr_t arr, uint16_t i) {
    return *reinterpret_cast<uint32_t*>(arr + ((uintptr_t)i * 40) + 32);
}

void EnumeratePackageClips(int data_id, const std::string& pkg_name, uint32_t& root_stream_id,
                           const char*& root_clip_name) {
    PkgClipTable const pkg = FindPkgClipTable(data_id);
    if (pkg.rec == 0U) return;
    const char* pkg_nm = *reinterpret_cast<const char**>(pkg.rec + 0x60);
    LOG("DDR", "package rec=%p clip_count=%u arr=%p name@+96='%s' (expect '%s')", (void*)pkg.rec,
        pkg.count, (void*)pkg.arr, pkg_nm ? pkg_nm : "(null)", pkg_name.c_str());
    g_clips.clear();
    for (uint16_t i = 0; i < pkg.count && i < 24; i++) {
        const char* nm = PkgClipName(pkg.arr, i);
        uint32_t const sid = PkgClipStreamId(pkg.arr, i);
        g_clips.push_back({.name = (nm != nullptr) ? std::string(nm) : std::string(),
                           .stream_id = sid,
                           .layer_id = 0});
        int datafmt_ver = -1;
        int sw = -1;
        int sh = -1;
        if (g_afp.afp_stream_get_info != nullptr) {
            uint8_t si[64] = {0};
            if (g_afp.afp_stream_get_info(sid, si) >= 0) {
                datafmt_ver = *reinterpret_cast<uint16_t*>(si + 2);
                sw = *reinterpret_cast<uint16_t*>(si + 16);
                sh = *reinterpret_cast<uint16_t*>(si + 18);
            }
        }
        LOG("DDR", "  clip[%u] name='%s' stream_id=%#x datafmt_ver=%d size=%dx%d", i,
            nm ? nm : "(null)", sid, datafmt_ver, sw, sh);
        if (i == 0) {
            root_stream_id = sid;
            root_clip_name = nm;
        }
    }
}

void ApplyRootClipOverride(int data_id, uint32_t& root_stream_id, const char*& root_clip_name) {
    std::optional<std::string> const rc = Support::EnvVar("DDR_ROOT_CLIP");
    if (!rc) return;
    PkgClipTable const pkg = FindPkgClipTable(data_id);
    if (pkg.rec == 0U) return;
    for (uint16_t i = 0; i < pkg.count; i++) {
        const char* nm = PkgClipName(pkg.arr, i);
        if ((nm != nullptr) && strcmp(nm, rc->c_str()) == 0) {
            root_stream_id = PkgClipStreamId(pkg.arr, i);
            root_clip_name = nm;
            LOG("DDR", "DDR_ROOT_CLIP override -> clip '%s' stream=%#x", nm, root_stream_id);
            break;
        }
    }
}

void CreateExtraLayersDiag(int data_id) {
    if (!Support::EnvFlag("DDR_EXTRA_LAYERS")) return;
    PkgClipTable const pkg = FindPkgClipTable(data_id);
    int made = 0;
    if (pkg.rec != 0U) {
        for (uint16_t i = 1; i < pkg.count; i++) {
            const char* nm = PkgClipName(pkg.arr, i);
            uint32_t const sid = PkgClipStreamId(pkg.arr, i);
            uint32_t const lid = g_afp.afp_layer_create_with_property(sid, nm, 0, nullptr);
            if ((g_afp.afp_id_is_valid != nullptr) && g_afp.afp_id_is_valid(5, lid) >= 0) {
                if (g_afp.afp_layer_play != nullptr) g_afp.afp_layer_play(lid, 1.0F);
                made++;
            }
        }
    }
    LOG("DDR", "DDR_EXTRA_LAYERS: created+played %d extra layers (advance-context test)", made);
}

void CreateRootLayer(int data_id, uint32_t root_stream_id, const char* root_clip_name) {
    if (g_afp.afp_layer_create_with_property == nullptr) return;
    uint32_t stream_id = 0;
    const char* path = nullptr;
    if ((g_afpu.afpu_get_afp_info_at_package != nullptr) && (root_clip_name != nullptr)) {
        uint8_t info[64] = {0};
        g_afpu.afpu_get_afp_info_at_package(info, static_cast<uint32_t>(data_id), root_clip_name);
        stream_id = *reinterpret_cast<uint32_t*>(info + 24);
        path = *reinterpret_cast<const char**>(info + 16);
    }
    if (stream_id == 0U) {
        stream_id = root_stream_id;
        path = root_clip_name;
    }
    g_root_stream_id = stream_id;
    LOG("DDR", "layer src: stream_id=%#x path='%s'", stream_id, path ? path : "");
    uint32_t const layer_id = g_afp.afp_layer_create_with_property(stream_id, path, 0, nullptr);
    int const valid = (g_afp.afp_id_is_valid != nullptr) ? g_afp.afp_id_is_valid(5, layer_id) : 0;
    LOG("DDR", "afp_layer_create_with_property -> layer=%#x valid=%d", layer_id, valid);
    if (valid >= 0) {
        g_layer_id = layer_id;
        const char* active = "";
        if (path != nullptr) {
            active = path;
        } else if (root_clip_name != nullptr) {
            active = root_clip_name;
        }
        g_active_clip = active;
        for (auto& c : g_clips) {
            if (c.name == g_active_clip) {
                c.layer_id = g_layer_id;
                break;
            }
        }
    }
    if ((g_layer_id != 0U) && (g_afp.afp_layer_set_attribute != nullptr)) {
        int attr = 0;
        if (std::optional<std::string> la = Support::EnvVar("DDR_LAYER_ATTR"))
            attr = (int)strtol(la->c_str(), nullptr, 0);
        if (attr != 0) {
            g_afp.afp_layer_set_attribute(g_layer_id, attr, attr);
            LOG("DDR", "afp_layer_set_attribute(%#x, %#x, %#x)", g_layer_id, attr, attr);
        }
    }
    if ((g_layer_id != 0U) && (g_afp.afp_layer_mc_refer != nullptr) &&
        Support::EnvFlag("DDR_MC_REFER")) {
        int const mc = g_afp.afp_layer_mc_refer(g_layer_id, "/");
        LOG("DDR", "afp_layer_mc_refer(%#x, \"/\") -> %#x", g_layer_id, mc);
    }
    if (g_layer_id != 0U) CreateExtraLayersDiag(data_id);
}

}

bool LoadIfs(AvsFuncs& avs, DllLoader& avs_dll, const std::string& ifs_disk_path,
             const std::string& pkg_name) {
    if (!g_booted) {
        LOG("DDR", "LoadIfs before Boot");
        return false;
    }

    if (!AvsManager::MountIfs(avs, avs_dll, ifs_disk_path)) {
        LOG("DDR", "MountIfs failed for %s", ifs_disk_path.c_str());
        return false;
    }

    int data_id = -1;
    if (g_afpu.afpu_ngp_read_data != nullptr) {
        data_id = g_afpu.afpu_ngp_read_data(pkg_name.c_str(), "/afp/packages", 0);
        LOG("DDR", "afpu_ngp_read_data('%s', /afp/packages) -> %d", pkg_name.c_str(), data_id);
    }
    if (data_id < 0) {
        LOG("DDR", "package read failed");
        return false;
    }
    g_stream_data_id = data_id;

    if (g_afpu.afpu_do_create_stream_all != nullptr) {
        int const r = g_afpu.afpu_do_create_stream_all(
            reinterpret_cast<void*>(static_cast<intptr_t>(data_id)),
            reinterpret_cast<void*>(static_cast<intptr_t>(1)));
        LOG("DDR", "afpu_do_create_stream_all(%d, 1) -> %d", data_id, r);
    }

    uint32_t root_stream_id = 0;
    const char* root_clip_name = nullptr;
    EnumeratePackageClips(data_id, pkg_name, root_stream_id, root_clip_name);
    ApplyRootClipOverride(data_id, root_stream_id, root_clip_name);
    CreateRootLayer(data_id, root_stream_id, root_clip_name);

    return true;
}

namespace {
void AdvanceOnce(float dt) {
    static float const env_scale = []() {
        std::optional<std::string> e = Support::EnvVar("DDR_TIME_SCALE");
        return e ? (float)atof(e->c_str()) : 0.0F;
    }();
    static int const env_mode = Support::EnvInt("DDR_RENDER_MODE").value_or(1);
    const float eff = env_scale > 0.0F ? env_scale : g_time_scale;
    if (env_mode == 3) {
        if (g_afp.afp_do_render != nullptr) g_afp.afp_do_render(dt * eff, 3, 0);
    } else if (env_mode == 2) {
        static bool const adv_dt0 = Support::EnvFlag("DDR_ADV_DT0");
        float const adt = adv_dt0 ? 0.0F : dt * eff;
        if (g_afp.afp_render_init != nullptr) g_afp.afp_render_init();
        if (g_afp.afp_do_render != nullptr) {
            for (unsigned g = 0; g < 8; g++)
                g_afp.afp_do_render(adt, 2, g);
        }
        if (g_afp.afp_render_finish != nullptr) g_afp.afp_render_finish();
    } else {
        if (g_afp.afp_render_init != nullptr) g_afp.afp_render_init();
        if (g_afp.afp_do_render != nullptr) g_afp.afp_do_render(dt * eff, env_mode, 0);
        if (g_afp.afp_render_finish != nullptr) g_afp.afp_render_finish();
    }
}
}

namespace {
uintptr_t LayerStruct() {
    if (g_layer_id == 0U) return 0;
    static auto afp_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("libafp-win64.dll"));
    if (afp_base == 0U) return 0;
    int const group = static_cast<int>((g_layer_id >> 27) & 0xF) - 1;
    if (group < 0 || group >= 2) return 0;
    uintptr_t const table =
        *reinterpret_cast<uintptr_t*>(afp_base + 0x244FD0 + (static_cast<uintptr_t>(group) * 8));
    if (table == 0U) return 0;
    uintptr_t const s =
        *reinterpret_cast<uintptr_t*>(table + (static_cast<uintptr_t>(g_layer_id & 0xFFFF) * 8));
    if ((s == 0U) || *reinterpret_cast<uint32_t*>(s + 28) != g_layer_id) return 0;
    static bool logged = false;
    if (!logged) {
        logged = true;
        LOG("DDR",
            "LayerStruct OK: id=%#x struct=%p struct+28=%#x struct+60(subpos)=%d struct+40=%d",
            g_layer_id, (void*)s, *reinterpret_cast<uint32_t*>(s + 28),
            *reinterpret_cast<int*>(s + 60), (int)*reinterpret_cast<uint8_t*>(s + 40));
    }
    return s;
}
}
namespace {
void ClearSubFrame() {
    uintptr_t const s = LayerStruct();
    if (s == 0U) return;
    *reinterpret_cast<float*>(s + 52) = 0.0F;
    *reinterpret_cast<float*>(s + 56) = 0.0F;
}
}

namespace {
uintptr_t ChildMc(const char* name) {
    static auto afp_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("libafp-win64.dll"));
    if ((afp_base == 0U) || (g_layer_id == 0U) || (g_afp.afp_layer_mc_refer == nullptr)) return 0;
    static uint32_t mc_id = 0;
    static bool tried = false;
    if (!tried) {
        tried = true;
        mc_id = static_cast<uint32_t>(g_afp.afp_layer_mc_refer(g_layer_id, name));
        LOG("DDR", "ChildMc('%s') mc_refer -> %#x (group=%d)", name, mc_id, (mc_id >> 27) & 0xF);
    }
    if (((mc_id >> 27) & 0xF) != 4) return 0;
    uintptr_t const tbl = *reinterpret_cast<uintptr_t*>(afp_base + 0x246640);
    if (tbl == 0U) return 0;
    uintptr_t const s =
        *reinterpret_cast<uintptr_t*>(tbl + (static_cast<uintptr_t>(mc_id & 0xFFFF) * 8));
    if ((s == 0U) || *reinterpret_cast<uint32_t*>(s + 348) != mc_id) return 0;
    return s;
}
}
namespace {
void DumpChild(int frame) {
    static std::optional<std::string> nm = Support::EnvVar("DDR_DUMP_CHILD");
    if (!nm) return;
    uintptr_t const s = ChildMc(nm->c_str());
    if (s == 0U) return;
    for (int base = 0; base < 128; base += 64) {
        char line[1024];
        int o = 0;
        o += snprintf(line + o, sizeof(line) - o, "CHILD f%d [%s]:", frame, nm->c_str());
        for (int i = base; i < base + 64 && o < (int)sizeof(line) - 16; i++) {
            o += snprintf(line + o, sizeof(line) - o, " %d=%d", i * 4,
                          *reinterpret_cast<int*>(s + ((uintptr_t)i * 4)));
        }
        LOG("DDR", "%s", line);
    }
}
}

namespace {
void LogSubPosDiag(int frame) {
    static bool const log_subpos = Support::EnvFlag("DDR_LOG_SUBPOS");
    if (!log_subpos || frame >= 2000 || (g_afp.afp_layer_get_info == nullptr) || (g_layer_id == 0U))
        return;
    int info[64] = {0};
    if (g_afp.afp_layer_get_info(g_layer_id, info, 0) >= 0) {
        LOG("DDR-SP", "frame=%d out0=%d wrapmod=%d subpos=%d counter=%d", frame, info[0], info[6],
            info[13], info[14]);
    }
}

void DumpLoopProbeLabels(uint32_t mc) {
    int total = -1;
    g_afp.afp_mc_get_param(mc, 0x1011, &total);
    LOG("DDR-LOOP", "root mc=%#x total_frame=%d", mc, total);
    const char* names[] = {"loop",    "end",     "start",    "loop_start", "loop_end", "loopstart",
                           "loopend", "loop_in", "loop_out", "in",         "out",      "main",
                           "wait",    "lp",      "anime",    "animation"};
    for (const char* nm : names) {
        int f = -1;
        if (g_afp.afp_mc_get_param(mc, 0x1012, nm, &f) == 0)
            LOG("DDR-LOOP", "  label '%s' = frame %d", nm, f);
    }
}

void DumpLoopDiag(int frame) {
    static bool const dump_loop = Support::EnvFlag("DDR_DUMP_LOOP");
    if (!dump_loop || (g_afp.afp_layer_mc_refer == nullptr) ||
        (g_afp.afp_mc_get_param == nullptr) || (g_layer_id == 0U))
        return;
    static uint32_t mc = 0;
    static bool tried = false;
    if (!tried) {
        tried = true;
        mc = static_cast<uint32_t>(g_afp.afp_layer_mc_refer(g_layer_id, ""));
        DumpLoopProbeLabels(mc);
    }
    if (mc == 0U) return;
    int cur = -1;
    int loops = -1;
    g_afp.afp_mc_get_param(mc, 0x1010, &cur);
    g_afp.afp_mc_get_param(mc, 0x1013, &loops);
    static int prev_cur = -1;
    static int last_wrap = -1;
    if (prev_cur >= 0 && cur < prev_cur - 4) {
        int const gap = (last_wrap >= 0) ? (frame - last_wrap) : -1;
        LOG("DDR-LOOP",
            "frame=%d WRAP current_frame %d -> %d  loop_count=%d  gap_since_prev_wrap=%d", frame,
            prev_cur, cur, loops, gap);
        last_wrap = frame;
    }
    prev_cur = cur;
    if ((frame % 600) == 0)
        LOG("DDR-LOOP", "frame=%d current_frame=%d loop_count=%d", frame, cur, loops);
}

void DisplayFrame(int frame, bool first) {
    static int const disp_mode = Support::EnvInt("DDR_DISPLAY_MODE").value_or(5);
    if (first) LOG("DDR", "RenderFrame %d: afp_do_display(mode=%d)...", frame, disp_mode);
    if (disp_mode == 2 && (g_afp.afp_do_display != nullptr)) {
        for (unsigned g = 0; g < 8; g++)
            g_afp.afp_do_display(2, g);
    } else if ((g_afp.afp_do_display != nullptr) && (g_layer_id != 0U)) {
        g_afp.afp_do_display(5, g_layer_id);
    }
}
}

void RenderFrame(float dt) {
    if (!g_booted) return;
    static int frame = 0;
    bool const first = (frame < 2);
    DdrRender::ResetDrawCount();
    AdvanceOnce(dt);
    DumpChild(frame);
    static bool const frame_lock = Support::EnvFlag("DDR_FRAME_LOCK");
    if (frame_lock) ClearSubFrame();
    LogSubPosDiag(frame);
    DumpLoopDiag(frame);
    DisplayFrame(frame, first);
    frame++;
}

bool IsBooted() {
    return g_booted;
}

uint32_t LayerId() {
    return g_layer_id;
}

uint32_t LoopFrames() {
    return g_loop_frames;
}

uint32_t FrameCounter() {
    if ((g_afp.afp_layer_get_info == nullptr) || (g_layer_id == 0U)) return 0;
    uint32_t info[16] = {0};
    if (g_afp.afp_layer_get_info(g_layer_id, info, 0) < 0) return 0;
    return info[14];
}

namespace {
uint32_t g_root_mc = 0;
}
namespace {
uint32_t g_root_mc_layer = 0;
}

namespace {
uint32_t RootMc() {
    if (g_root_mc_layer != g_layer_id) {
        g_root_mc_layer = g_layer_id;
        g_root_mc = ((g_afp.afp_layer_mc_refer != nullptr) && (g_layer_id != 0U))
                        ? static_cast<uint32_t>(g_afp.afp_layer_mc_refer(g_layer_id, ""))
                        : 0;
    }
    return g_root_mc;
}
}

int ClipCurrentFrame() {
    if (g_afp.afp_mc_get_param == nullptr) return -1;
    uint32_t const mc = RootMc();
    if (mc == 0U) return -1;
    int cur = -1;
    g_afp.afp_mc_get_param(mc, 0x1010, &cur);
    return cur;
}

int ClipLabelFrame(const char* name) {
    if ((g_afp.afp_mc_get_param == nullptr) || (name == nullptr)) return -1;
    uint32_t const mc = RootMc();
    if (mc == 0U) return -1;
    int f = -1;
    if (g_afp.afp_mc_get_param(mc, 0x1012, name, &f) != 0) return -1;
    return f;
}

bool ReadPlayhead(uint32_t* cur, uint32_t* total, uint32_t* raw_loop_count) {
    if (g_afp.afp_mc_get_param == nullptr) return false;
    uint32_t const mc = RootMc();
    if (mc == 0U) return false;
    int c = -1;
    int t = -1;
    int l = -1;
    g_afp.afp_mc_get_param(mc, 0x1010, &c);
    g_afp.afp_mc_get_param(mc, 0x1011, &t);
    g_afp.afp_mc_get_param(mc, 0x1013, &l);
    if (cur != nullptr) *cur = static_cast<uint32_t>(c < 0 ? 0 : c);
    if (total != nullptr) *total = static_cast<uint32_t>(t < 0 ? 0 : t);
    if (raw_loop_count != nullptr) *raw_loop_count = static_cast<uint32_t>(l < 0 ? 0 : l);
    return c >= 0;
}

bool ReadSize(uint32_t* w, uint32_t* h) {
    if ((g_afp.afp_stream_get_info == nullptr) || (g_root_stream_id == 0U)) return false;
    uint8_t si[64] = {0};
    if (g_afp.afp_stream_get_info(g_root_stream_id, si) < 0) return false;
    if (w != nullptr) *w = *reinterpret_cast<uint16_t*>(si + 16);
    if (h != nullptr) *h = *reinterpret_cast<uint16_t*>(si + 18);
    return true;
}

std::vector<Label> EnumerateLabels() {
    std::vector<Label> out;
    static const char* const kNames[] = {"in", "loop", "out", "end"};
    for (const char* nm : kNames) {
        int const f = ClipLabelFrame(nm);
        if (f >= 0) out.push_back({.name = nm, .frame = f});
    }
    return out;
}

void SetPaused(bool paused) {
    if ((g_afp.afp_layer_play != nullptr) && (g_layer_id != 0U))
        g_afp.afp_layer_play(g_layer_id, paused ? 0.0F : 1.0F);
}

bool SeekFrame(int frame) {
    if (g_afp.afp_mc_op_frame == nullptr) return false;
    uint32_t const mc = RootMc();
    if (mc == 0U) return false;
    frame = std::max(frame, 0);
    g_afp.afp_mc_op_frame(mc, 0xF08, frame);
    return true;
}

bool GotoLabel(const std::string& name) {
    if (name.empty()) return false;
    int const f = ClipLabelFrame(name.c_str());
    if (f < 0) return false;
    return SeekFrame(f);
}

std::vector<std::string> ClipNames() {
    std::vector<std::string> out;
    out.reserve(g_clips.size());
    for (auto& c : g_clips)
        out.push_back(c.name);
    return out;
}

std::string ActiveClip() {
    return g_active_clip;
}

namespace {
bool CreateClipLayer(DdrClip& clip, const std::string& name) {
    uint32_t stream_id = 0;
    const char* path = clip.name.c_str();
    if ((g_afpu.afpu_get_afp_info_at_package != nullptr) && g_stream_data_id >= 0) {
        uint8_t info[64] = {0};
        g_afpu.afpu_get_afp_info_at_package(info, (uint32_t)g_stream_data_id, clip.name.c_str());
        stream_id = *reinterpret_cast<uint32_t*>(info + 24);
        const char* p = *reinterpret_cast<const char**>(info + 16);
        if (p != nullptr) path = p;
    }
    if (stream_id == 0U) stream_id = clip.stream_id;
    uint32_t const lid = g_afp.afp_layer_create_with_property(stream_id, path, 0, nullptr);
    if ((g_afp.afp_id_is_valid != nullptr) && g_afp.afp_id_is_valid(5, lid) < 0) {
        LOG("DDR", "SwitchClip: create failed for '%s'", name.c_str());
        return false;
    }
    clip.layer_id = lid;
    g_layer_id = lid;
    return true;
}
}

bool SwitchClip(const std::string& name) {
    if (g_afp.afp_layer_create_with_property == nullptr) return false;
    DdrClip* clip = nullptr;
    for (auto& c : g_clips) {
        if (c.name == name) {
            clip = &c;
            break;
        }
    }
    if (clip == nullptr) {
        LOG("DDR", "SwitchClip: '%s' not in package", name.c_str());
        return false;
    }

    if ((g_layer_id != 0U) && g_layer_id != clip->layer_id && (g_afp.afp_layer_stop != nullptr))
        g_afp.afp_layer_stop(g_layer_id);

    const bool reuse = (clip->layer_id != 0U) && (g_afp.afp_id_is_valid != nullptr) &&
                       g_afp.afp_id_is_valid(5, clip->layer_id) >= 0;
    if (reuse) {
        if (g_afp.afp_layer_stop != nullptr) g_afp.afp_layer_stop(clip->layer_id);
        if (g_afp.afp_layer_play != nullptr) g_afp.afp_layer_play(clip->layer_id, 1.0F);
        g_layer_id = clip->layer_id;
    } else if (!CreateClipLayer(*clip, name)) {
        return false;
    }
    g_root_stream_id = clip->stream_id;
    g_active_clip = clip->name;
    if (g_afp.afp_layer_mc_refer != nullptr) g_afp.afp_layer_mc_refer(g_layer_id, "");
    LOG("DDR", "SwitchClip -> '%s' layer=%#x reuse=%d", name.c_str(), g_layer_id, reuse);
    return true;
}

void Shutdown() {
    if (!g_booted) return;
    g_booted = false;
    g_layer_id = 0;
    g_root_stream_id = 0;
    g_clips.clear();
    g_active_clip.clear();
}

}
