#include "afp_funcs.h"
#include "afpu_funcs.h"
#include <cstdint>
#include <utility>
#include "avs_funcs.h"
#include <cstdio>
#include "afp_boot.h"
#include "engine_session.h"
#include "avs_boot.h"
#include "ifs_inspect.h"
#include "support/log.h"
#include "render_backend.h"
#include "render_seh.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include "app_globals.h"

void AfpManager::Shutdown(EngineSession& es) {
    AfpFuncs& afp = es.afp;
    AfpuFuncs const& afpu = es.afpu;
    if (es.afp_booted) {
        DestroySceneStreams(afp);
        if (afp.afp_render_destroy != nullptr) afp.afp_render_destroy();
        afpu.afpu_shutdown();
        afp.afp_shutdown();
        es.afp_booted = false;
    }
    es.stream_id = 0xFFFFFFFC;
    es.extra_streams.clear();
    es.pkg_id = 0;
    es.anim_name.clear();
}

bool AfpManager::IsBooted() {
    return g_engine.afp_booted;
}
uint32_t AfpManager::StreamId() {
    return g_engine.stream_id;
}
uint32_t AfpManager::PackageId() {
    return g_engine.pkg_id;
}
const std::string& AfpManager::AnimName() {
    return g_engine.anim_name;
}

namespace {
int ReferRootMcId(const AfpFuncs& afp) {
    if (g_engine.stream_id == 0xFFFFFFFC || (int)g_engine.stream_id < 0) return -1;
    if (afp.afp_mc_get_id_by_path == nullptr) return -1;
    int const mc_id = afp.afp_mc_get_id_by_path(g_engine.stream_id, "");
    if (mc_id <= 0) return -1;
    if (afp.afp_stream_control != nullptr) afp.afp_stream_control(6, (uint32_t)mc_id);
    return mc_id;
}
}

std::vector<AfpManager::Label> AfpManager::EnumerateLabels(const AfpFuncs& afp) {
    std::vector<Label> out;
    if (afp.afp_mc_set == nullptr) return out;
    int const mc_id = ReferRootMcId(afp);
    if (mc_id < 0) return out;

    int count = 0;
    int const rc = afp.afp_mc_set(mc_id, 0x101F, &count);
    if (rc < 0 || count <= 0) return out;
    count = std::min(count, 4096);

    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const char* name = nullptr;
        int frame = 0;
        int const r = afp.afp_mc_set(mc_id, 0x1020, i, &name, &frame);
        out.push_back({.name = (r == 0 && (name != nullptr)) ? std::string(name) : std::string(),
                       .frame = frame});
    }
    std::string dbg;
    for (auto& l : out)
        dbg += " '" + l.name + "'@" + std::to_string(l.frame);
    LOG("AFP", "EnumerateLabels: %zu label(s) on '%s':%s", out.size(), g_engine.anim_name.c_str(),
        dbg.c_str());
    return out;
}

bool AfpManager::GotoLabel(const AfpFuncs& afp, const std::string& label) {
    if (afp.afp_mc_control == nullptr) return false;
    int const mc_id = ReferRootMcId(afp);
    if (mc_id < 0) return false;
    constexpr uint32_t kDeepGotoPlayLabel = 0xF09;
    int const rc = afp.afp_mc_control(mc_id, kDeepGotoPlayLabel, label.c_str());
    LOG("AFP", "GotoLabel('%s') mc=0x%08x -> %d", label.c_str(), (uint32_t)mc_id, rc);
    return rc >= 0;
}

bool AfpManager::SeekFrame(const AfpFuncs& afp, int frame) {
    if (afp.afp_mc_control_frame == nullptr) return false;
    int const mc_id = ReferRootMcId(afp);
    if (mc_id < 0) return false;
    frame = std::max(frame, 0);
    constexpr uint32_t kDeepGotoPlayFrame = 0xF08;
    int const rc = afp.afp_mc_control_frame(mc_id, kDeepGotoPlayFrame, frame);
    LOG("AFP", "SeekFrame(%d) mc=0x%08x -> %d", frame, (uint32_t)mc_id, rc);
    return rc >= 0;
}

void AfpManager::SetStreamPaused(const AfpFuncs& afp, bool paused) {
    if (g_engine.stream_id == 0xFFFFFFFC || (int)g_engine.stream_id < 0) return;
    if (afp.afp_stream_set_speed != nullptr)
        afp.afp_stream_set_speed(g_engine.stream_id, paused ? 0.0F : 1.0F);
    if (afp.afp_set_flag_mask != nullptr) afp.afp_set_flag_mask(g_engine.stream_id, 1, 1);
    LOG("AFP", "SetStreamPaused(%s) stream=0x%08x", paused ? "true" : "false", g_engine.stream_id);
}

int AfpManager::GetRootMcId(const AfpFuncs& afp) {
    return ReferRootMcId(afp);
}

std::vector<AfpManager::ChildClip> AfpManager::EnumerateChildClips(const AfpFuncs& afp,
                                                                   bool want_positions, bool* ok) {
    std::vector<ChildClip> out;
    if (ok != nullptr) *ok = false;
    if (afp.afp_mc_enumerate_children == nullptr) return out;
    int const root = ReferRootMcId(afp);
    if (root < 0) return out;

    static thread_local char names[64][128];
    DWORD code = 0;
    int const n = RenderSeh::SafeEnumChildNames(afp.afp_mc_enumerate_children, (uint32_t)root, 3,
                                                names, 64, &code);
    if (n < 0) {
        LOG("AFP",
            "EnumerateChildClips: afp_mc_enumerate_children faulted "
            "(0x%08lx) - keeping last list",
            (unsigned long)code);
        return out;
    }
    if (ok != nullptr) *ok = true;

    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        ChildClip c;
        c.name = names[i];
        if (want_positions && !c.name.empty() && (afp.afp_mc_get_id_by_path != nullptr) &&
            (afp.afp_mc_set != nullptr)) {
            int const cid = afp.afp_mc_get_id_by_path(g_engine.stream_id, c.name.c_str());
            if (cid > 0) {
                float xy[2] = {0.0F, 0.0F};
                if (afp.afp_mc_set(cid, 0x1008, xy) == 0) {
                    c.screen_x = xy[0];
                    c.screen_y = xy[1];
                    c.have_pos = true;
                }
                int cc = -1;
                int ct = -1;
                if (afp.afp_mc_set(cid, 0x1010, &cc) >= 0) {
                    afp.afp_mc_set(cid, 0x1011, &ct);
                    c.cur = cc;
                    c.total = ct;
                    c.have_playhead = true;
                }
            }
        }
        out.push_back(std::move(c));
    }
    return out;
}

namespace {

bool MountCompanionImage(AvsFuncs& avs, const std::string& companion_path, const char* fsroot,
                         const char* mountpoint) {
    namespace fs = std::filesystem;
    fs::path const path(companion_path);
    std::string const parent = path.parent_path().string();
    std::string const filename = path.filename().string();

    if (!AvsManager::MountFsRoot(avs, fsroot, parent)) {
        LOG("AFP", "LoadCompanion: MountFsRoot('%s' <- '%s') failed", fsroot, parent.c_str());
        return false;
    }
    std::string const vfs_src = std::string(fsroot) + "/" + filename;
    if (!AvsManager::MountIfsImage(avs, mountpoint, vfs_src)) {
        LOG("AFP", "LoadCompanion: MountIfsImage('%s' <- '%s') failed", mountpoint,
            vfs_src.c_str());
        if (avs.avs_fs_umount != nullptr) avs.avs_fs_umount(fsroot);
        return false;
    }
    return true;
}

void QueueCompanionAtlasFilters(AvsFuncs& avs, const char* mountpoint,
                                const std::string& pkg_name) {
    auto cf = IfsInspect::ReadAtlasFilters(avs, mountpoint);
    for (auto& af : cf)
        AfpD3D9::EnqueueAtlasFilter(af.mag_filter_d3d, af.min_filter_d3d);
    if (!cf.empty()) {
        LOG("AFP", "LoadCompanion: '%s': %zu atlas filter(s) queued", pkg_name.c_str(), cf.size());
    }
}

}

uint32_t AfpManager::LoadCompanion(EngineSession& es, const std::string& companion_path,
                                   const std::string& pkg_name) {
    AfpuFuncs const& afpu = es.afpu;
    AvsFuncs& avs = es.avs;
    namespace fs = std::filesystem;

    if ((afpu.afpu_ngp_read_local == nullptr) || (afpu.afpu_package_open_streams == nullptr)) {
        LOG("AFP", "LoadCompanion: required AFPU exports missing");
        return 0;
    }
    if (!fs::exists(companion_path)) {
        LOG("AFP", "LoadCompanion: '%s' does not exist", companion_path.c_str());
        return 0;
    }

    int const idx = es.next_companion_idx++;
    char fsroot[64];
    snprintf(fsroot, sizeof(fsroot), "/afp_companion_src_%d", idx);
    char mountpoint[64];
    snprintf(mountpoint, sizeof(mountpoint), "/afp_companion_%d", idx);

    if (!MountCompanionImage(avs, companion_path, fsroot, mountpoint)) return 0;

    int const pkg_id = afpu.afpu_ngp_read_local(pkg_name.c_str(), mountpoint, 0);
    LOG("AFP", "LoadCompanion: afpu_ngp_read_local('%s' @ %s) = 0x%08x", pkg_name.c_str(),
        mountpoint, (unsigned)pkg_id);
    if (pkg_id <= 0) {
        if (avs.avs_fs_umount != nullptr) {
            avs.avs_fs_umount(mountpoint);
            avs.avs_fs_umount(fsroot);
        }
        return 0;
    }

    QueueCompanionAtlasFilters(avs, mountpoint, pkg_name);

    int const streams_ret = afpu.afpu_package_open_streams((uint32_t)pkg_id);
    LOG("AFP", "LoadCompanion: open_streams(0x%x) = %d", (unsigned)pkg_id, streams_ret);

    CompanionRecord rec;
    rec.pkg_id = (uint32_t)pkg_id;
    rec.mountpoint = std::string(mountpoint) + "|" + fsroot;
    es.companions.push_back(std::move(rec));
    return (uint32_t)pkg_id;
}

void AfpManager::UnloadCompanion(EngineSession& es, uint32_t pkg_id) {
    AfpuFuncs const& afpu = es.afpu;
    AvsFuncs const& avs = es.avs;
    if (pkg_id == 0) return;

    auto it = es.companions.begin();
    for (; it != es.companions.end(); ++it) {
        if (it->pkg_id == pkg_id) break;
    }

    if (afpu.afpu_package_control != nullptr) {
        LOG("AFP", "UnloadCompanion: package_control(6, 0x%x)", (unsigned)pkg_id);
        afpu.afpu_package_control(6, pkg_id, nullptr);
    }

    if (it != es.companions.end()) {
        auto sep = it->mountpoint.find('|');
        if ((avs.avs_fs_umount != nullptr) && sep != std::string::npos) {
            std::string const imagefs = it->mountpoint.substr(0, sep);
            std::string const fsroot = it->mountpoint.substr(sep + 1);
            avs.avs_fs_umount(imagefs.c_str());
            avs.avs_fs_umount(fsroot.c_str());
            LOG("AFP", "UnloadCompanion: umounted %s + %s", imagefs.c_str(), fsroot.c_str());
        }
        es.companions.erase(it);
    }
}

bool AfpManager::ForceReplay(EngineSession& es) {
    if ((es.pkg_id == 0U) || es.pkg_id == 0xFFFFFFFE) return false;
    if (es.anim_name.empty()) return false;
    std::string const anim = es.anim_name;
    es.anim_name.clear();
    return SwitchAnimation(es, anim);
}

void AfpManager::DestroyCurrentStream(AfpFuncs& afp) {
    if (g_engine.stream_id == 0xFFFFFFFC) return;
    if (afp.afp_stream_destroy != nullptr) afp.afp_stream_destroy(5, g_engine.stream_id, 0);
    g_engine.stream_id = 0xFFFFFFFC;
    g_engine.anim_name.clear();
}

namespace {

bool LayerInfoDumpEnabled() {
    static int s_dump_enabled = -1;
    if (s_dump_enabled < 0) {
        char v[8] = {};
        DWORD const n = GetEnvironmentVariableA("RENDERER_LAYER_INFO_DUMP", v, sizeof(v));
        s_dump_enabled = (n > 0 && (v[0] != 0) && v[0] != '0') ? 1 : 0;
    }
    return s_dump_enabled != 0;
}

void MaybeDumpLayerInfo(const uint8_t* info, uint32_t flags) {
    if (!LayerInfoDumpEnabled()) return;
    static uint32_t s_last_words[15] = {};
    static uint32_t s_last_stream = 0xFFFFFFFCU;
    static int s_calls_since_log = 0;
    const auto* w = reinterpret_cast<const uint32_t*>(info);
    bool changed = false;
    if (g_engine.stream_id != s_last_stream) {
        s_last_stream = g_engine.stream_id;
        for (auto& v2 : s_last_words)
            v2 = 0xFFFFFFFFU;
        s_calls_since_log = 0;
        changed = true;
    } else {
        for (int i = 0; i < 15; i++) {
            if (w[i] != s_last_words[i]) {
                changed = true;
                break;
            }
        }
    }
    if (changed || ++s_calls_since_log >= 30) {
        LOG("AFP",
            "layer_info: stream=0x%08x flags=0x%08x "
            "[w0..w14]=%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x "
            "%08x %08x",
            g_engine.stream_id, flags, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9],
            w[10], w[11], w[12], w[13], w[14]);
        for (int i = 0; i < 15; i++)
            s_last_words[i] = w[i];
        s_calls_since_log = 0;
    }
}

}

bool AfpManager::IsMasterComplete(const AfpFuncs& afp) {
    if (g_engine.stream_id == 0xFFFFFFFC || (int)g_engine.stream_id < 0) return false;
    if (afp.afp_get_layer_info == nullptr) return false;

    uint8_t info[64] = {};
    int const rc = afp.afp_get_layer_info(g_engine.stream_id, info);
    if (rc < 0) return false;

    uint32_t const flags = *reinterpret_cast<const uint32_t*>(info + 4);
    MaybeDumpLayerInfo(info, flags);
    return (flags & 0xE0000000U) != 0;
}

bool AfpManager::ReadLayerPosition(const AfpFuncs& afp, uint32_t* cur, uint32_t* total) {
    if (g_engine.stream_id == 0xFFFFFFFC || (int)g_engine.stream_id < 0) return false;
    if (afp.afp_get_layer_info == nullptr) return false;
    uint8_t info[64] = {};
    int const rc = afp.afp_get_layer_info(g_engine.stream_id, info);
    if (rc < 0) return false;
    const auto* w = reinterpret_cast<const uint32_t*>(info);
    if (total != nullptr) *total = w[12];
    if (cur != nullptr) *cur = w[13];
    return true;
}

bool AfpManager::ReadMcPlayhead(const AfpFuncs& afp, uint32_t* cur, uint32_t* total,
                                uint32_t* loop_count) {
    if (afp.afp_mc_set == nullptr) return false;
    int const mc_id = ReferRootMcId(afp);
    if (mc_id < 0) return false;
    int c = 0;
    int t = 0;
    int lc = 0;
    if (afp.afp_mc_set(mc_id, 0x1010, &c) < 0) return false;
    afp.afp_mc_set(mc_id, 0x1011, &t);
    afp.afp_mc_set(mc_id, 0x1013, &lc);
    if (cur != nullptr) *cur = (uint32_t)c;
    if (total != nullptr) *total = (uint32_t)t;
    if (loop_count != nullptr) *loop_count = (uint32_t)lc;
    return true;
}

bool AfpManager::ReadLayerAdvanceCounter(const AfpFuncs& afp, uint32_t* counter) {
    if (g_engine.stream_id == 0xFFFFFFFC || (int)g_engine.stream_id < 0) return false;
    if (afp.afp_get_layer_info == nullptr) return false;
    uint8_t info[64] = {};
    int const rc = afp.afp_get_layer_info(g_engine.stream_id, info);
    if (rc < 0) return false;
    const auto* w = reinterpret_cast<const uint32_t*>(info);
    if (counter != nullptr) *counter = w[14];
    return true;
}

void AfpManager::UnloadAllCompanions(EngineSession& es) {
    if (es.companions.empty()) return;
    LOG("AFP", "UnloadAllCompanions: releasing %zu companion package(s)", es.companions.size());
    auto copy = es.companions;
    for (auto& rec : copy) {
        UnloadCompanion(es, rec.pkg_id);
    }
    es.companions.clear();
}

std::string AfpManager::LastCompanionMountPoint() {
    if (g_engine.companions.empty()) return {};
    return g_engine.companions.back().mountpoint;
}

bool AfpManager::SwitchAnimation(EngineSession& es, const std::string& anim_name, bool force) {
    AfpFuncs const& afp = es.afp;
    AfpuFuncs const& afpu = es.afpu;
    if ((es.pkg_id == 0U) || es.pkg_id == 0xFFFFFFFE) {
        LOG("AFP", "SwitchAnimation: no package loaded");
        return false;
    }
    if ((afp.afp_stream_play == nullptr) || (afp.afp_stream_destroy == nullptr)) {
        LOG("AFP", "SwitchAnimation: required exports missing");
        return false;
    }

    if (!force && es.anim_name == anim_name && es.stream_id != 0xFFFFFFFC) {
        return true;
    }

    auto get_afp_info = afpu.afpu_afp_get_info_in_package;
    if (get_afp_info == nullptr) {
        LOG("AFP", "SwitchAnimation: afpu_afp_get_info_in_package unresolved");
        return false;
    }

    uint64_t info[8] = {};
    int const info_ret = get_afp_info(info, es.pkg_id, anim_name.c_str());
    if (info_ret < 0) {
        LOG("AFP", "SwitchAnimation: afpu_afp_get_info('%s') = %d", anim_name.c_str(), info_ret);
        return false;
    }
    auto data_id = (uint32_t)info[3];
    void* data_ptr = (void*)info[2];

    if (es.stream_id != 0xFFFFFFFC) {
        afp.afp_stream_destroy(5, es.stream_id, 0);
        es.stream_id = 0xFFFFFFFC;
        es.anim_name.clear();
    }

    auto old_level = (afp.afp_get_create_level != nullptr) ? afp.afp_get_create_level() : 0;
    if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(0);

    int const r = afp.afp_stream_play(data_id, (const char*)data_ptr, 0, 0);

    if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(old_level);

    if (r < 0) {
        LOG("AFP", "SwitchAnimation: afp_stream_play('%s') = 0x%08x", anim_name.c_str(),
            (unsigned)r);
        return false;
    }
    es.stream_id = (uint32_t)r;
    es.anim_name = anim_name;

    if (afp.afp_set_flag_mask != nullptr) afp.afp_set_flag_mask((uint32_t)r, 513, 513);
    if (afp.afp_stream_set_speed != nullptr) afp.afp_stream_set_speed((uint32_t)r, 1.0F);

    LOG("AFP", "SwitchAnimation: now playing '%s' stream=0x%08x", anim_name.c_str(), (unsigned)r);
    return true;
}

namespace {

bool BuildBitmapStreamArgs(const AfpuFuncs& afpu, uint32_t pkg_id, const std::string& bitmap_name,
                           uint8_t (&stream_args)[40]) {
    auto image_lookup = afpu.afpu_image_lookup;
    if (image_lookup == nullptr) {
        LOG("AFP", "PlayBitmapAnimation: afpu_image_lookup unresolved");
        return false;
    }
    uint8_t image_info[64] = {};
    int rc = image_lookup(image_info, pkg_id, bitmap_name.c_str());
    if (rc != 0) {
        LOG("AFP", "PlayBitmapAnimation: afpu_image_lookup('%s') = %d", bitmap_name.c_str(), rc);
        return false;
    }
    auto build_args = afpu.afpu_image_to_stream_args;
    if (build_args == nullptr) {
        LOG("AFP", "PlayBitmapAnimation: afpu_image_to_stream_args unresolved");
        return false;
    }
    rc = build_args(stream_args, image_info);
    if (rc != 0) {
        LOG("AFP", "PlayBitmapAnimation: image_to_stream_args = %d", rc);
        return false;
    }
    return true;
}

int SwapClipBitmapChain(const AfpFuncs& afp, const uint8_t (&stream_args)[40], uint32_t stream_id,
                        const char* clip_name) {
    if ((afp.afp_mc_get_id_by_path == nullptr) || (afp.afp_mc_get_relative_id == nullptr) ||
        (afp.afp_play_work_load_image == nullptr))
        return 0;
    int mc = afp.afp_mc_get_id_by_path(stream_id, clip_name);
    int n = 0;
    for (int count = 0; mc >= 0 && count < 64; mc = afp.afp_mc_get_relative_id(mc, 6), ++count) {
        afp.afp_play_work_load_image(mc, stream_args, 0);
        if (afp.afp_mc_get != nullptr) afp.afp_mc_get(mc, 0x101E, 1);
        ++n;
    }
    return n;
}

uint32_t CreateBitmapStream(const AfpFuncs& afp, const uint8_t (&stream_args)[40]) {
    auto image_stream_create = afp.afp_image_stream_create;
    if (image_stream_create == nullptr) {
        LOG("AFP", "PlayBitmapAnimation: afp_image_stream_create unresolved");
        return 0xFFFFFFFC;
    }
    auto old_level = (afp.afp_get_create_level != nullptr) ? afp.afp_get_create_level() : 0;
    if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(0);
    uint32_t const stream_id = image_stream_create(stream_args, 0);
    if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(old_level);
    return stream_id;
}

}

int AfpManager::SwapClipBitmapFromCompanion(EngineSession& es, uint32_t pkg_id, uint32_t stream_id,
                                            const char* clip_name) {
    uint8_t stream_args[40] = {};
    if (!BuildBitmapStreamArgs(es.afpu, pkg_id, clip_name, stream_args)) return -1;
    return SwapClipBitmapChain(es.afp, stream_args, stream_id, clip_name);
}

bool AfpManager::PlayBitmapAnimation(EngineSession& es, const std::string& bitmap_name) {
    AfpFuncs const& afp = es.afp;
    AfpuFuncs const& afpu = es.afpu;
    if ((es.pkg_id == 0U) || es.pkg_id == 0xFFFFFFFE) {
        LOG("AFP", "PlayBitmapAnimation: no package loaded");
        return false;
    }
    if (afp.afp_stream_destroy == nullptr) {
        LOG("AFP", "PlayBitmapAnimation: afp_stream_destroy missing");
        return false;
    }

    uint8_t stream_args[40] = {};
    if (!BuildBitmapStreamArgs(afpu, es.pkg_id, bitmap_name, stream_args)) return false;

    if (es.stream_id != 0xFFFFFFFC) {
        afp.afp_stream_destroy(5, es.stream_id, 0);
        es.stream_id = 0xFFFFFFFC;
        es.anim_name.clear();
    }

    uint32_t const stream_id = CreateBitmapStream(afp, stream_args);

    if (stream_id == 0xFFFFFFFC || (int)stream_id < 0) {
        LOG("AFP", "PlayBitmapAnimation: image_stream_create('%s') = 0x%08x", bitmap_name.c_str(),
            (unsigned)stream_id);
        return false;
    }
    es.stream_id = stream_id;
    es.anim_name = bitmap_name;

    if (afp.afp_set_flag_mask != nullptr) afp.afp_set_flag_mask(stream_id, 512, 512);
    if (afp.afp_stream_set_speed != nullptr) afp.afp_stream_set_speed(stream_id, 1.0F);

    LOG("AFP", "PlayBitmapAnimation: now playing bitmap '%s' stream=0x%08x", bitmap_name.c_str(),
        (unsigned)stream_id);
    return true;
}
