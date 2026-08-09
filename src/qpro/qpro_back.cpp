#include <utility>
#include "engine_session.h"
#include "qpro/qpro_internal.h"
#include "qpro/qpro_walk.h"
#include "qpro/qpro_extract.h"
#include "boot.h"
#include "afp_boot.h"
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

int WriteBackRealtimeStill(EngineSession& es, D3D9State& d3d, const std::string& out_path,
                           const std::string& back_ifs, ClipFrames& cf) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    AfpManager::SeekFrame(es.afp, 0);
    RenderFrame(es, d3d, px, w, h, false);
    GrabBackFrame(cf, px, w, h);
    int const n = (!cf.frames.empty() &&
                   WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality))
                      ? 1
                      : 0;
    AfpManager::UnloadPackages(es);
    LOG("BackRT", "%s -> static (%dx%d) n=%d", back_ifs.c_str(), cf.cw, cf.ch, n);
    return n;
}

void CaptureBackRealtimeFrames(EngineSession& es, D3D9State& d3d, ClipFrames& cf, int fps,
                               uint32_t total) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    AfpManager::SeekFrame(es.afp, 0);
    uint32_t s0 = 0;
    uint32_t st = 0;
    uint32_t start_lc = 0;
    AfpManager::ReadMcPlayhead(es.afp, &s0, &st, &start_lc);
    RenderFrame(es, d3d, px, w, h, false);
    GrabBackFrame(cf, px, w, h);
    const int cap = ((int)total * (fps / 8 + 2)) + 300;
    for (int i = 1; i < cap; ++i) {
        if (es.afp.afp_do_update != nullptr)
            RenderSeh::SafeCallUpdate(es.afp.afp_do_update, 1.0F / (float)fps);
        RenderFrame(es, d3d, px, w, h, false);
        GrabBackFrame(cf, px, w, h);
        uint32_t c = 0;
        uint32_t tt = 0;
        uint32_t l = 0;
        AfpManager::ReadMcPlayhead(es.afp, &c, &tt, &l);
        if ((int)c >= (int)total - 1 || l > start_lc) break;
    }
}

}

int RenderBackRealtime(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                       const std::string& back_ifs, int fps, const std::string& out_path) {
    if (fps < 1) fps = 60;
    std::string const path = IfsPath(game_dir, back_ifs);
    AfpManager::UmountPackagesAndData(es.avs);
    (void)AfpD3D9::NextSlot();
    if (!MountAndLoadIfs(path)) {
        LOG("BackRT", "mount %s FAILED", back_ifs.c_str());
        return 0;
    }
    bool const animated = AfpManager::SwitchAnimation(es, "qp_bg", true);
    uint32_t cur = 0;
    uint32_t total = 0;
    uint32_t lc = 0;
    if (animated) AfpManager::ReadMcPlayhead(es.afp, &cur, &total, &lc);
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 4; ++i)
        RenderFrame(es, d3d, px, w, h, true);

    ClipFrames cf;
    cf.fps = fps;
    if (!animated || total <= 1) return WriteBackRealtimeStill(es, d3d, out_path, back_ifs, cf);

    CaptureBackRealtimeFrames(es, d3d, cf, fps, total);
    int n = 0;
    if (cf.frames.size() > 1) {
        if (EncodeClipFrames(out_path, cf, "backrt") > 0) n = 1;
    } else if (!cf.frames.empty() &&
               WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) {
        n = 1;
    }
    AfpManager::UnloadPackages(es);
    LOG("BackRT", "%s -> %d frames @ %d fps (clip total=%u)", back_ifs.c_str(),
        (int)cf.frames.size(), fps, total);
    return n;
}

int ProbeBackNativeFps(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                       const std::string& back_ifs, int fps) {
    if (fps < 1) fps = 60;
    int native_fps = fps;
    AfpManager::UmountPackagesAndData(es.avs);
    const int slot_base = AfpD3D9::NextSlot();
    if (MountAndLoadIfs(IfsPath(game_dir, back_ifs)) &&
        AfpManager::SwitchAnimation(es, "qp_bg", true)) {
        std::vector<uint8_t> tp;
        int tw = 0;
        int th = 0;
        for (int i = 0; i < 4; ++i)
            RenderFrame(es, d3d, tp, tw, th, true);
        uint32_t cA = 0;
        uint32_t pt = 0;
        AfpManager::ReadMcPlayhead(es.afp, &cA, &pt, nullptr);
        const int N = 40;
        for (int i = 0; i < N; ++i) {
            if (es.afp.afp_do_update != nullptr)
                RenderSeh::SafeCallUpdate(es.afp.afp_do_update, 1.0F / (float)fps);
            RenderFrame(es, d3d, tp, tw, th, false);
        }
        uint32_t cB = 0;
        AfpManager::ReadMcPlayhead(es.afp, &cB, nullptr, nullptr);
        int const adv = (int)cB - (int)cA;
        if (adv > 0) {
            int const nf = (int)std::lround((double)adv / (double)N * (double)fps);
            if (nf >= 1) native_fps = nf;
        }
        LOG("BackFps", "%s native-fps: cur %u->%u / %d ticks @ %d => %d (total=%u)",
            back_ifs.c_str(), cA, cB, N, fps, native_fps, pt);
    }
    AfpManager::UnloadPackages(es);
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

BackCompCtx SetupBackCompositeSlots(EngineSession& es, bool own, const std::string& main2,
                                    int pre_comp_base, int pre_main_base) {
    BackCompCtx ctx;
    if (own) {
        ctx.slot_base = AfpD3D9::NextSlot();
        AfpManager::UmountPackagesAndData(es.avs);
        ctx.main_base = AfpD3D9::NextSlot();
        if (!MountAndLoadIfs(main2)) {
            LOG("BackComp", "mount qp_main2 FAILED");
            return ctx;
        }
        TexList mainTl;
        ParseTexturelist(es, mainTl, "/afp/packages");
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

int AttachQproBgStream(EngineSession& es, uint32_t pkg, uint32_t sid) {
    return AttachClipStream(es, pkg, sid, "qpro_bg", "qp_bg").head_mc;
}

void HideAllButQproBg(EngineSession& es, uint32_t sid) {
    for (const char* a : kAllAvatarLayers)
        McControl::SetClipVisible(es.afp, sid, a, std::strcmp(a, "qpro_bg") == 0);
}

int ReadBackTotal(EngineSession& es, int part_mc) {
    int total = 1;
    if (part_mc >= 0 && (es.afp.afp_mc_set != nullptr)) {
        int t = 0;
        if (es.afp.afp_mc_set(part_mc, 0x1011, &t) >= 0 && t > 1) total = t;
    }
    return total;
}

void ReadBackHead(EngineSession& es, int part_mc, int* cur, int* lc) {
    int c = -1;
    int l = -1;
    if (es.afp.afp_stream_control != nullptr) es.afp.afp_stream_control(6, (uint32_t)part_mc);
    if (part_mc >= 0 && (es.afp.afp_mc_set != nullptr)) {
        es.afp.afp_mc_set(part_mc, 0x1010, &c);
        es.afp.afp_mc_set(part_mc, 0x1013, &l);
    }
    if (cur != nullptr) *cur = c;
    if (lc != nullptr) *lc = l;
}

int WriteStillBack(EngineSession& es, D3D9State& d3d, const std::string& out_path) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    RenderFrame(es, d3d, px, w, h, false);
    if (w <= 0 || h <= 0) return 0;
    std::vector<uint8_t> f = px;
    UnpremultiplyBGRA(f);
    return WriteStillAvif(out_path, f.data(), w, h, kQproAvifQuality) ? 1 : 0;
}

int CaptureAnimatedBack(EngineSession& es, D3D9State& d3d, const std::string& out_path,
                        uint32_t sid, int part_mc, int fps, int native_fps, int total,
                        ClipFrames& cf) {
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
    ReadBackHead(es, part_mc, nullptr, &start_lc);
    RenderFrame(es, d3d, px, w, h, false);
    grab();
    for (int i = 1; i < cap; ++i) {
        if (es.afp.afp_do_update != nullptr) RenderSeh::SafeCallUpdate(es.afp.afp_do_update, dt);
        HideAllButQproBg(es, sid);
        RenderFrame(es, d3d, px, w, h, false);
        grab();
        int c = 0;
        int l = 0;
        ReadBackHead(es, part_mc, &c, &l);
        if (c >= total - 1 || l > start_lc) break;
    }
    if (cf.frames.size() > 1) return WriteAnimatedTriple(out_path, cf, "backcomp") ? 1 : 0;
    if (!cf.frames.empty() &&
        WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) {
        return 1;
    }
    return 0;
}

void CleanupBackComposite(EngineSession& es, bool own, uint32_t pkg, int slot_base, int comp_base) {
    AfpManager::DestroyCurrentStream(es.afp);
    if (pkg != 0U) AfpManager::UnloadCompanion(es, pkg);
    if (own) AfpManager::UnloadPackages(es);
    int const base = own ? slot_base : comp_base;
    int const cur = AfpD3D9::NextSlot();
    for (int s = base; s < cur; ++s)
        AfpD3D9::TexDestroy((unsigned)s);
    AfpD3D9::SetNextSlot(base);
}

bool ProbeBackAnimated(EngineSession& es, int part_mc, const std::string& back_ifs, int total) {
    int const vcmds = (part_mc >= 0) ? ClipVisualCmdsAfterFrame0(es.afp, (uint32_t)part_mc) : -1;
    bool const animated = (vcmds != 0);
    LOG("BackAnim", "%s visual-cmds@f>0=%d animated=%d total=%d", back_ifs.c_str(), vcmds,
        (int)animated, total);
    return animated;
}

struct BackCapture {
    bool ok = false;
    BackCompCtx ctx;
    uint32_t pkg = 0;
    uint32_t sid = 0;
    int part_mc = -1;
    int w = 0;
    int h = 0;
};

BackCapture BeginBackCapture(EngineSession& es, D3D9State& d3d, bool own, const std::string& main2,
                             const std::string& item, const std::string& back_ifs,
                             const CompositeShare& share) {
    BackCapture b;
    b.ctx = SetupBackCompositeSlots(es, own, main2, share.comp_base, share.main_base);
    if (!b.ctx.ok) return b;

    b.pkg = AfpManager::LoadCompanion(es, item, Stem(back_ifs));
    if (!AfpManager::SwitchAnimation(es, "qp_motion", true))
        LOG("BackComp", "SwitchAnimation(qp_motion) FAILED");
    b.sid = AfpManager::StreamId();
    b.part_mc = AttachQproBgStream(es, b.pkg, b.sid);

    std::vector<uint8_t> px;
    WarmUpFrames(es, d3d, px, b.w, b.h);
    AfpManager::SeekFrame(es.afp, 0);
    HideAllButQproBg(es, b.sid);
    b.ok = true;
    return b;
}

}

int RenderBackComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                        const std::string& back_ifs, int fps, const std::string& out_path,
                        const BackOptions& opt) {
    if (fps < 1) fps = 60;
    const CompositeShare& share = opt.share;
    std::set<std::string>* const video_out = opt.video_out;
    const bool detect_only = opt.detect_only;
    int native_fps = opt.native_fps;
    const bool own = (share.sid == 0);
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const item = IfsPath(game_dir, back_ifs);
    if (native_fps <= 0)
        native_fps = own ? ProbeBackNativeFps(es, d3d, game_dir, back_ifs, fps) : fps;
    if (native_fps < 1) native_fps = fps;

    const BackCapture cap = BeginBackCapture(es, d3d, own, main2, item, back_ifs, share);
    if (!cap.ok) return 0;
    uint32_t const pkg = cap.pkg;
    uint32_t const sid = cap.sid;
    int const part_mc = cap.part_mc;

    int const total = ReadBackTotal(es, part_mc);
    bool const animated = ProbeBackAnimated(es, part_mc, back_ifs, total);
    if (animated && (video_out != nullptr)) video_out->insert(fs::path(out_path).stem().string());

    ClipFrames cf;
    cf.cw = cap.w;
    cf.ch = cap.h;
    cf.fps = fps;
    int n = 0;
    if (!detect_only) {
        n = (total <= 1 || !animated)
                ? WriteStillBack(es, d3d, out_path)
                : CaptureAnimatedBack(es, d3d, out_path, sid, part_mc, fps, native_fps, total, cf);
    }
    LOG("BackComp", "%s -> %d file(s), %d frames @ %d fps (total=%d native=%d) %s",
        back_ifs.c_str(), n, (int)cf.frames.size(), fps, total, native_fps, out_path.c_str());

    CleanupBackComposite(es, own, pkg, cap.ctx.slot_base, cap.ctx.comp_base);
    return n;
}

void BackComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                   const std::string& back_ifs) {
    HMODULE afpcore = GetModuleHandleA("afp-core.dll");
    auto idx = [&]() -> int { return afpcore ? *(uint16_t*)((uint8_t*)afpcore + 0xE1062) : -1; };
    auto tbl = [&]() -> uint64_t {
        return afpcore ? *(uint64_t*)((uint8_t*)afpcore + 0xE1050) : 0;
    };

    AfpManager::UmountPackagesAndData(es.avs);
    if (!MountAndLoadIfs(IfsPath(game_dir, back_ifs))) {
        LOG("QproIdx", "mount FAILED");
        return;
    }
    if (!AfpManager::SwitchAnimation(es, "qp_bg", true)) {
        LOG("QproIdx", "SwitchAnimation(qp_bg) FAILED - continuing");
    }
    AfpManager::SeekFrame(es.afp, 0);
    LOG("QproIdx", "%s: after setup matrix_idx=%d table=0x%llx", back_ifs.c_str(), idx(),
        (unsigned long long)tbl());

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    int prev = idx();
    for (int i = 0; i < 6; ++i) {
        int const before = idx();
        RenderFrame(es, d3d, px, w, h, true);
        int const after = idx();
        if (after != prev || i < 5) {
            LOG("QproIdx", "frame %d: idx %d -> %d table=0x%llx", i, before, after,
                (unsigned long long)tbl());
        }
        prev = after;
    }
    AfpManager::UnloadPackages(es);
}

}
