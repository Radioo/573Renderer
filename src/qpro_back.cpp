#include <utility>
#include "qpro_internal.h"
#include "qpro_walk.h"
#include "qpro_extract.h"
#include "boot.h"
#include "afp_boot.h"
#include "app_globals.h"
#include "mc_control.h"
#include "render_seh.h"
#include "render_backend.h"
#include "support/log.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include <windows.h>
#include <cmath>

namespace QproExtract {
namespace fs = std::filesystem;
using namespace detail;

namespace {

void GrabBackFrame(ClipFrames& cf, const std::vector<uint8_t>& px, int w, int h) {
    if (w <= 0 || h <= 0) return;
    std::vector<uint8_t> f = px;
    UnpremultiplyBGRA(f);
    cf.cw = w;
    cf.ch = h;
    cf.frames.push_back(std::move(f));
}

int WriteBackRealtimeStill(const std::string& out_path, const std::string& back_ifs,
                           ClipFrames& cf) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    AfpManager::SeekFrame(g_afp, 0);
    RenderFrame(px, w, h, false);
    GrabBackFrame(cf, px, w, h);
    int const n = (!cf.frames.empty() &&
                   WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality))
                      ? 1
                      : 0;
    AfpManager::UnloadPackages(g_engine);
    LOG("BackRT", "%s -> static (%dx%d) n=%d", back_ifs.c_str(), cf.cw, cf.ch, n);
    return n;
}

void CaptureBackRealtimeFrames(ClipFrames& cf, int fps, uint32_t total) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    AfpManager::SeekFrame(g_afp, 0);
    uint32_t s0 = 0;
    uint32_t st = 0;
    uint32_t start_lc = 0;
    AfpManager::ReadMcPlayhead(g_afp, &s0, &st, &start_lc);
    RenderFrame(px, w, h, false);
    GrabBackFrame(cf, px, w, h);
    const int cap = ((int)total * (fps / 8 + 2)) + 300;
    for (int i = 1; i < cap; ++i) {
        if (g_afp.afp_do_update != nullptr)
            RenderSeh::SafeCallUpdate(g_afp.afp_do_update, 1.0F / (float)fps);
        RenderFrame(px, w, h, false);
        GrabBackFrame(cf, px, w, h);
        uint32_t c = 0;
        uint32_t tt = 0;
        uint32_t l = 0;
        AfpManager::ReadMcPlayhead(g_afp, &c, &tt, &l);
        if ((int)c >= (int)total - 1 || l > start_lc) break;
    }
}

}

int RenderBackRealtime(const std::string& game_dir, const std::string& back_ifs, int fps,
                       const std::string& out_path) {
    if (fps < 1) fps = 60;
    std::string const path = IfsPath(game_dir, back_ifs);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    (void)AfpD3D9::NextSlot();
    if (!MountAndLoadIfs(path)) {
        LOG("BackRT", "mount %s FAILED", back_ifs.c_str());
        return 0;
    }
    bool const animated = AfpManager::SwitchAnimation(g_engine, "qp_bg", true);
    uint32_t cur = 0;
    uint32_t total = 0;
    uint32_t lc = 0;
    if (animated) AfpManager::ReadMcPlayhead(g_afp, &cur, &total, &lc);
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 4; ++i)
        RenderFrame(px, w, h, true);

    ClipFrames cf;
    cf.fps = fps;
    if (!animated || total <= 1) return WriteBackRealtimeStill(out_path, back_ifs, cf);

    CaptureBackRealtimeFrames(cf, fps, total);
    int n = 0;
    if (cf.frames.size() > 1) {
        if (EncodeClipFrames(out_path, cf, "backrt") > 0) n = 1;
    } else if (!cf.frames.empty() &&
               WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) {
        n = 1;
    }
    AfpManager::UnloadPackages(g_engine);
    LOG("BackRT", "%s -> %d frames @ %d fps (clip total=%u)", back_ifs.c_str(),
        (int)cf.frames.size(), fps, total);
    return n;
}

int ProbeBackNativeFps(const std::string& game_dir, const std::string& back_ifs, int fps) {
    if (fps < 1) fps = 60;
    int native_fps = fps;
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    const int slot_base = AfpD3D9::NextSlot();
    if (MountAndLoadIfs(IfsPath(game_dir, back_ifs)) &&
        AfpManager::SwitchAnimation(g_engine, "qp_bg", true)) {
        std::vector<uint8_t> tp;
        int tw = 0;
        int th = 0;
        for (int i = 0; i < 4; ++i)
            RenderFrame(tp, tw, th, true);
        uint32_t cA = 0;
        uint32_t pt = 0;
        AfpManager::ReadMcPlayhead(g_afp, &cA, &pt, nullptr);
        const int N = 40;
        for (int i = 0; i < N; ++i) {
            if (g_afp.afp_do_update != nullptr)
                RenderSeh::SafeCallUpdate(g_afp.afp_do_update, 1.0F / (float)fps);
            RenderFrame(tp, tw, th, false);
        }
        uint32_t cB = 0;
        AfpManager::ReadMcPlayhead(g_afp, &cB, nullptr, nullptr);
        int const adv = (int)cB - (int)cA;
        if (adv > 0) {
            int const nf = (int)std::lround((double)adv / (double)N * (double)fps);
            if (nf >= 1) native_fps = nf;
        }
        LOG("BackFps", "%s native-fps: cur %u->%u / %d ticks @ %d => %d (total=%u)",
            back_ifs.c_str(), cA, cB, N, fps, native_fps, pt);
    }
    AfpManager::UnloadPackages(g_engine);
    {
        int const cur = AfpD3D9::NextSlot();
        for (int s = slot_base; s < cur; ++s)
            AfpD3D9::TexDestroy((unsigned)s);
        AfpD3D9::SetNextSlot(slot_base);
    }
    return native_fps;
}

namespace {

struct BackCompCtx {
    bool ok = false;
    int comp_base = 0;
    int main_base = 0;
    int slot_base = -1;
};

BackCompCtx SetupBackCompositeSlots(bool own, const std::string& main2, int pre_comp_base,
                                    int pre_main_base) {
    BackCompCtx ctx;
    if (own) {
        ctx.slot_base = AfpD3D9::NextSlot();
        if (g_avs.avs_fs_umount != nullptr) {
            g_avs.avs_fs_umount("/afp/packages");
            g_avs.avs_fs_umount("/data");
        }
        ctx.main_base = AfpD3D9::NextSlot();
        if (!MountAndLoadIfs(main2)) {
            LOG("BackComp", "mount qp_main2 FAILED");
            return ctx;
        }
        TexList mainTl;
        ParseTexturelist(mainTl, "/afp/packages");
        (void)mainTl;
        ctx.comp_base = AfpD3D9::NextSlot();
    } else {
        ctx.main_base = pre_main_base;
        ctx.comp_base = pre_comp_base;
        AfpD3D9::SetNextSlot(ctx.comp_base);
    }
    ctx.ok = true;
    return ctx;
}

int AttachQproBgStream(uint32_t pkg, uint32_t sid) {
    auto get_afp_info = g_afpu.afpu_afp_get_info_in_package;
    int const part_mc =
        (g_afp.afp_mc_get_id_by_path != nullptr) ? g_afp.afp_mc_get_id_by_path(sid, "qpro_bg") : -1;
    if ((get_afp_info != nullptr) && part_mc >= 0 && (g_afp.afp_mc_attach_stream != nullptr) &&
        (g_afp.afp_mc_get_relative_id != nullptr)) {
        uint64_t info[8] = {};
        if (get_afp_info(info, pkg, "qp_bg") >= 0) {
            int mc = part_mc;
            for (int c = 0; mc >= 0 && c < 64; mc = g_afp.afp_mc_get_relative_id(mc, 6), ++c) {
                g_afp.afp_mc_attach_stream(mc, (uint32_t)info[3]);
                if (g_afp.afp_mc_get != nullptr) g_afp.afp_mc_get(mc, 0x101E, 1);
            }
        }
    }
    return part_mc;
}

void HideAllButQproBg(uint32_t sid) {
    for (const char* a : kAllAvatarLayers)
        McControl::SetClipVisible(g_afp, sid, a, std::strcmp(a, "qpro_bg") == 0);
}

int ReadBackTotal(int part_mc) {
    int total = 1;
    if (part_mc >= 0 && (g_afp.afp_mc_set != nullptr)) {
        int t = 0;
        if (g_afp.afp_mc_set(part_mc, 0x1011, &t) >= 0 && t > 1) total = t;
    }
    return total;
}

void ReadBackHead(int part_mc, int* cur, int* lc) {
    int c = -1;
    int l = -1;
    if (g_afp.afp_stream_control != nullptr) g_afp.afp_stream_control(6, (uint32_t)part_mc);
    if (part_mc >= 0 && (g_afp.afp_mc_set != nullptr)) {
        g_afp.afp_mc_set(part_mc, 0x1010, &c);
        g_afp.afp_mc_set(part_mc, 0x1013, &l);
    }
    if (cur != nullptr) *cur = c;
    if (lc != nullptr) *lc = l;
}

int WriteStillBack(const std::string& out_path) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    RenderFrame(px, w, h, false);
    if (w <= 0 || h <= 0) return 0;
    std::vector<uint8_t> f = px;
    UnpremultiplyBGRA(f);
    return WriteStillAvif(out_path, f.data(), w, h, kQproAvifQuality) ? 1 : 0;
}

int CaptureAnimatedBack(const std::string& out_path, uint32_t sid, int part_mc, int fps,
                        int native_fps, int total, ClipFrames& cf) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    auto grab = [&]() {
        if (w <= 0 || h <= 0) return;
        std::vector<uint8_t> f = px;
        UnpremultiplyBGRA(f);
        cf.cw = w;
        cf.ch = h;
        cf.frames.push_back(std::move(f));
    };
    const float dt = (1.0F / (float)fps) * ((float)native_fps / 60.0F);
    const int cap = (int)(((long long)total * fps) / (native_fps > 0 ? native_fps : fps)) + 90;
    int start_lc = 0;
    ReadBackHead(part_mc, nullptr, &start_lc);
    RenderFrame(px, w, h, false);
    grab();
    for (int i = 1; i < cap; ++i) {
        if (g_afp.afp_do_update != nullptr) RenderSeh::SafeCallUpdate(g_afp.afp_do_update, dt);
        HideAllButQproBg(sid);
        RenderFrame(px, w, h, false);
        grab();
        int c = 0;
        int l = 0;
        ReadBackHead(part_mc, &c, &l);
        if (c >= total - 1 || l > start_lc) break;
    }
    if (cf.frames.size() > 1) return WriteAnimatedTriple(out_path, cf, "backcomp") ? 1 : 0;
    if (!cf.frames.empty() &&
        WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) {
        return 1;
    }
    return 0;
}

void CleanupBackComposite(bool own, uint32_t pkg, int slot_base, int comp_base) {
    AfpManager::DestroyCurrentStream(g_afp);
    if (pkg != 0U) AfpManager::UnloadCompanion(g_engine, pkg);
    if (own) AfpManager::UnloadPackages(g_engine);
    int const base = own ? slot_base : comp_base;
    int const cur = AfpD3D9::NextSlot();
    for (int s = base; s < cur; ++s)
        AfpD3D9::TexDestroy((unsigned)s);
    AfpD3D9::SetNextSlot(base);
}

}

int RenderBackComposite(const std::string& game_dir, const std::string& back_ifs, int fps,
                        const std::string& out_path, int native_fps, const CompositeShare& share,
                        bool detect_only, std::set<std::string>* video_out) {
    if (fps < 1) fps = 60;
    const bool own = (share.sid == 0);
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const item = IfsPath(game_dir, back_ifs);
    if (native_fps <= 0) native_fps = own ? ProbeBackNativeFps(game_dir, back_ifs, fps) : fps;
    if (native_fps < 1) native_fps = fps;

    const BackCompCtx ctx = SetupBackCompositeSlots(own, main2, share.comp_base, share.main_base);
    if (!ctx.ok) return 0;

    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, item, Stem(back_ifs));
    if (!AfpManager::SwitchAnimation(g_engine, "qp_motion", true))
        LOG("BackComp", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();
    int const part_mc = AttachQproBgStream(pkg, sid);

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 6; ++i)
        RenderFrame(px, w, h, true);
    AfpManager::SeekFrame(g_afp, 0);
    HideAllButQproBg(sid);

    int const total = ReadBackTotal(part_mc);
    int const vcmds = (part_mc >= 0) ? ClipVisualCmdsAfterFrame0(g_afp, (uint32_t)part_mc) : -1;
    bool const animated = (vcmds != 0);
    LOG("BackAnim", "%s visual-cmds@f>0=%d animated=%d total=%d", back_ifs.c_str(), vcmds,
        (int)animated, total);
    if (animated && (video_out != nullptr)) video_out->insert(fs::path(out_path).stem().string());

    ClipFrames cf;
    cf.cw = w;
    cf.ch = h;
    cf.fps = fps;
    int n = 0;
    if (detect_only) {
        n = 0;
    } else if (total <= 1 || !animated) {
        n = WriteStillBack(out_path);
    } else {
        n = CaptureAnimatedBack(out_path, sid, part_mc, fps, native_fps, total, cf);
    }
    LOG("BackComp", "%s -> %d file(s), %d frames @ %d fps (total=%d native=%d) %s",
        back_ifs.c_str(), n, (int)cf.frames.size(), fps, total, native_fps, out_path.c_str());

    CleanupBackComposite(own, pkg, ctx.slot_base, ctx.comp_base);
    return n;
}

void BackComposite(const std::string& game_dir, const std::string& back_ifs) {
    HMODULE afpcore = GetModuleHandleA("afp-core.dll");
    auto idx = [&]() -> int { return afpcore ? *(uint16_t*)((uint8_t*)afpcore + 0xE1062) : -1; };
    auto tbl = [&]() -> uint64_t {
        return afpcore ? *(uint64_t*)((uint8_t*)afpcore + 0xE1050) : 0;
    };

    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    if (!MountAndLoadIfs(IfsPath(game_dir, back_ifs))) {
        LOG("QproIdx", "mount FAILED");
        return;
    }
    AfpManager::SwitchAnimation(g_engine, "qp_bg", true);
    AfpManager::SeekFrame(g_afp, 0);
    LOG("QproIdx", "%s: after setup matrix_idx=%d table=0x%llx", back_ifs.c_str(), idx(),
        (unsigned long long)tbl());

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    int prev = idx();
    for (int i = 0; i < 6; ++i) {
        int const before = idx();
        RenderFrame(px, w, h, true);
        int const after = idx();
        if (after != prev || i < 5) {
            LOG("QproIdx", "frame %d: idx %d -> %d table=0x%llx", i, before, after,
                (unsigned long long)tbl());
        }
        prev = after;
    }
    AfpManager::UnloadPackages(g_engine);
}

}
