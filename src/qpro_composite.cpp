#include "formats/bgra_crop.h"
#include <utility>
#include "media/media_format.h"
#include <d3d9.h>
#include "qpro_internal.h"
#include "qpro_walk.h"
#include "qpro_extract.h"
#include "boot.h"
#include "afp_boot.h"
#include "app_globals.h"
#include "mc_control.h"
#include "render_backend.h"
#include "support/log.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace QproExtract {
namespace fs = std::filesystem;
using namespace detail;

namespace {
const char* kHandSwap[] = {"qp_hand_l", "qp_hand_l2", "qp_hand_r", "qp_hand_r2"};
}

namespace {
int SwapHandPieces(const TexList& mainTl, int main_base, const TexList& compTl, int comp_base) {
    int n = 0;
    for (const char* c : kHandSwap) {
        std::vector<uint8_t> px;
        int w = 0;
        int h = 0;
        if (!ReadPiece(compTl, comp_base, c, px, w, h)) continue;
        const AtlasImage* mim = FindImage(mainTl, c);
        if (mim == nullptr) continue;
        AfpD3D9::TexUpload((unsigned)(main_base + mim->atlas), 0x10, 0, 0, mim->x, mim->y, w, h,
                           px.data());
        ++n;
    }
    return n;
}
}

namespace {

void MountHandItemClipLogged(uint32_t pkg, uint32_t sid, const char* layer) {
    auto get_afp_info = g_afpu.afpu_afp_get_info_in_package;
    if ((get_afp_info == nullptr) || (g_afp.afp_mc_attach_stream == nullptr) ||
        (g_afp.afp_mc_get_id_by_path == nullptr) || (g_afp.afp_mc_get_relative_id == nullptr)) {
        LOG("QproHandC", "mount '%s': missing afp fn(s)", layer);
        return;
    }
    int mc = g_afp.afp_mc_get_id_by_path(sid, layer);
    int n = 0;
    for (int count = 0; mc >= 0 && count < 64; mc = g_afp.afp_mc_get_relative_id(mc, 6), ++count) {
        uint64_t info[8] = {};
        int const gi = get_afp_info(info, pkg, layer);
        auto data_id = (uint32_t)info[3];
        int64_t ar = -999;
        if (gi >= 0) ar = g_afp.afp_mc_attach_stream(mc, data_id);
        LOG("QproHandC", "  '%s' mc=0x%x get_info=%d info[2]=0x%llx data_id=0x%x attach=%lld",
            layer, mc, gi, (unsigned long long)info[2], data_id, (long long)ar);
        if (gi < 0) continue;
        if (g_afp.afp_mc_get != nullptr) g_afp.afp_mc_get(mc, 0x101E, 1);
        ++n;
    }
    LOG("QproHandC", "mounted item clip '%s' onto %d layer mc(s)", layer, n);
}

void HideNonHandLayersAll(uint32_t sid) {
    static const char* kNonHand[] = {
        "qpro_bg",           "qp_cat_1",       "qp_cat_2",        "qp_cat_3",
        "qp_head_b_neutral", "qp_hair_b",      "qp_face_neutral", "qp_hair_f",
        "qp_head_f_neutral", "qp_body_f",      "qp_body_b",       "qp_arm_r_upper",
        "qp_arm_r_lower",    "qp_arm_l_upper", "qp_arm_l_lower",  "qp_leg_r_upper",
        "qp_leg_r_lower",    "qp_leg_l_upper", "qp_leg_l_lower",
    };
    for (const char* h : kNonHand)
        McControl::SetClipVisible(g_afp, sid, h, false);
}

void RenderHandProbeFrames(std::vector<uint8_t>& px, int& w, int& h) {
    for (int i = 0; i < 3; ++i)
        RenderFrame(px, w, h, true);
    AfpD3D9::SetQproDrawProbe(true);
    for (int i = 0; i < 10; ++i)
        RenderFrame(px, w, h, true);
    AfpD3D9::SetQproDrawProbe(false);
    RenderFrame(px, w, h, false);
}

}

void HandComposite(const std::string& game_dir, const std::string& hand_ifs) {
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const hand = IfsPath(game_dir, hand_ifs);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    int const main_base = AfpD3D9::NextSlot();
    if (!MountAndLoadIfs(main2)) {
        LOG("QproHandC", "mount qp_main2 FAILED");
        return;
    }
    TexList mainTl;
    ParseTexturelist(mainTl, "/afp/packages");

    int const comp_base = AfpD3D9::NextSlot();
    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, hand, Stem(hand_ifs));
    LOG("QproHandC", "LoadCompanion(%s) pkg=0x%x", hand_ifs.c_str(), pkg);
    TexList compTl;
    ParseTexturelist(compTl, "/afp_companion_0");

    int const swapped = SwapHandPieces(mainTl, main_base, compTl, comp_base);
    LOG("QproHandC", "texture-swapped %d/%d hand pieces", swapped,
        (int)(sizeof(kHandSwap) / sizeof(kHandSwap[0])));

    if (!AfpManager::SwitchAnimation(g_engine, "qp_motion", true))
        LOG("QproHandC", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();

    MountHandItemClipLogged(pkg, sid, "qp_hand_l_neutral");
    MountHandItemClipLogged(pkg, sid, "qp_hand_r_neutral");
    AfpManager::SeekFrame(g_afp, 0);
    HideNonHandLayersAll(sid);

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    RenderHandProbeFrames(px, w, h);

    LOG("QproHandC", "rendered %dx%d", w, h);
    if (w > 0 && h > 0)
        WritePngBGRA((fs::path("screenshots") / "qphandcomposite.png").string(), px.data(), w, h);

    if (pkg != 0U) AfpManager::UnloadCompanion(g_engine, pkg);
}

constexpr int kSharedX = 36, kSharedY = 18, kSharedW = 448, kSharedH = 376;

namespace {

void MountHandItemClip(uint32_t pkg, uint32_t sid, const char* layer) {
    auto get_afp_info = g_afpu.afpu_afp_get_info_in_package;
    if ((get_afp_info == nullptr) || (g_afp.afp_mc_attach_stream == nullptr) ||
        (g_afp.afp_mc_get_id_by_path == nullptr) || (g_afp.afp_mc_get_relative_id == nullptr)) {
        return;
    }
    int mc = g_afp.afp_mc_get_id_by_path(sid, layer);
    for (int c = 0; mc >= 0 && c < 64; mc = g_afp.afp_mc_get_relative_id(mc, 6), ++c) {
        uint64_t info[8] = {};
        int const gi = get_afp_info(info, pkg, layer);
        if (gi >= 0) g_afp.afp_mc_attach_stream(mc, (uint32_t)info[3]);
        if (gi >= 0 && (g_afp.afp_mc_get != nullptr)) g_afp.afp_mc_get(mc, 0x101E, 1);
    }
}

void HideNonHandLayers(uint32_t sid, char side) {
    HideNonHandLayersAll(sid);
    McControl::SetClipVisible(g_afp, sid, side == 'L' ? "qp_hand_r_neutral" : "qp_hand_l_neutral",
                              false);
}

void ApplyHandHueScope(char side, const TexList& comp_tl, int comp_base) {
    AfpD3D9::ResetHsvScopeRect();
    const char* eff = (side == 'L') ? "qp_hand_l" : "qp_hand_r";
    if (g_hue_scope_enabled && CountWithPrefix(comp_tl, eff) > 1) {
        ScopeHueToImage(FindImage(comp_tl, eff), comp_base);
        if (side == 'L') ScopeHueToImage2(FindImage(comp_tl, "qp_hand_l2"), comp_base);
    }
}

int ProbeHandTotal(int part_mc) {
    int total = 1;
    if (part_mc >= 0 && (g_afp.afp_mc_set != nullptr)) {
        int t = 0;
        if (g_afp.afp_mc_set(part_mc, 0x1011, &t) >= 0 && t > 1) total = (t > 600) ? 600 : t;
    }
    return total;
}

void CaptureHandFrames(int part_mc, int total, ClipFrames& cf) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int K = 0; K < total; ++K) {
        if (part_mc >= 0 && (g_afp.afp_mc_control_frame != nullptr)) {
            if (g_afp.afp_stream_control != nullptr) g_afp.afp_stream_control(6, (uint32_t)part_mc);
            g_afp.afp_mc_control_frame(part_mc, 0xF08, K);
        }
        RenderFrame(px, w, h, false);
        if (w <= 0 || h <= 0) break;
        int cx = kSharedX;
        int cy = kSharedY;
        int cw = kSharedW;
        int ch = kSharedH;
        std::vector<uint8_t> crop = Bgra::Crop(px, w, h, cx, cy, cw, ch);
        if (!crop.empty() && cw == kSharedW && ch == kSharedH) {
            UnpremultiplyBGRA(crop);
            cf.frames[K] = std::move(crop);
        }
    }
}

int WriteHandOutputs(const std::string& out_path, const ClipFrames& cf, int total) {
    int n = 0;
    if (total > 1) {
        n = EncodeClipFrames(out_path, cf, "handcomp");
    } else if (!cf.frames.empty() && !cf.frames[0].empty()) {
        if (WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) n = 1;
        if (g_clip_dump_raw) {
            std::string png = out_path;
            if (png.size() > 5 && png.ends_with(".avif")) png.replace(png.size() - 5, 5, ".png");
            WritePngBGRA(png, cf.frames[0].data(), cf.cw, cf.ch);
        }
    }
    return n;
}

}

int RenderHandComposite(const std::string& game_dir, const std::string& hand_ifs, char side,
                        const std::string& out_path) {
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const hand = IfsPath(game_dir, hand_ifs);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    int const main_base = AfpD3D9::NextSlot();
    if (!MountAndLoadIfs(main2)) {
        LOG("HandComp", "mount qp_main2 FAILED");
        return 0;
    }
    TexList mainTl;
    ParseTexturelist(mainTl, "/afp/packages");
    int const comp_base = AfpD3D9::NextSlot();
    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, hand, Stem(hand_ifs));
    TexList compTl;
    ParseTexturelist(compTl, "/afp_companion_0");
    SwapHandPieces(mainTl, main_base, compTl, comp_base);
    if (!AfpManager::SwitchAnimation(g_engine, "qp_motion", true))
        LOG("HandComp", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();
    MountHandItemClip(pkg, sid, "qp_hand_l_neutral");
    MountHandItemClip(pkg, sid, "qp_hand_r_neutral");
    AfpManager::SeekFrame(g_afp, 0);
    HideNonHandLayers(sid, side);
    ApplyHandHueScope(side, compTl, comp_base);

    const char* layer = (side == 'L') ? "qp_hand_l_neutral" : "qp_hand_r_neutral";
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 6; ++i)
        RenderFrame(px, w, h, true);
    AfpManager::SeekFrame(g_afp, 0);
    int const part_mc =
        (g_afp.afp_mc_get_id_by_path != nullptr) ? g_afp.afp_mc_get_id_by_path(sid, layer) : -1;
    int const total = ProbeHandTotal(part_mc);
    ClipFrames cf;
    cf.cw = kSharedW;
    cf.ch = kSharedH;
    cf.fps = 30;
    cf.frames.assign((size_t)total, {});
    CaptureHandFrames(part_mc, total, cf);
    int const n = WriteHandOutputs(out_path, cf, total);
    if (pkg != 0U) AfpManager::UnloadCompanion(g_engine, pkg);
    LOG("HandComp", "%s side=%c -> %d frame(s), part_total=%d (%s)", hand_ifs.c_str(), side, n,
        total, out_path.c_str());
    return n;
}

namespace {
bool ReadItemAtlas(const TexList& tl, int slot0, const char* name, std::vector<uint8_t>& out,
                   int& w, int& h) {
    const AtlasImage* im = FindImage(tl, name);
    if (im == nullptr) return false;
    auto* tex = AfpD3D9::GetTexture(slot0 + im->atlas);
    if (tex == nullptr) return false;
    std::vector<uint8_t> atlas;
    int aw = 0;
    int ah = 0;
    if (!AfpD3D9::ReadTexturePixels(tex, atlas, aw, ah)) return false;
    int x = im->x;
    int y = im->y;
    w = im->w;
    h = im->h;
    out = Bgra::Crop(atlas, aw, ah, x, y, w, h);
    return !out.empty();
}
}

bool WriteAnimatedTriple(const std::string& base_avif, const ClipFrames& cf, const char* prefix) {
    bool any = false;
    if (EncodeClipFrames(QproFormatPath(base_avif, MediaSink::Format::WebM_VP9), cf, prefix,
                         MediaSink::Format::WebM_VP9) > 0)
        any = true;
    if (EncodeClipFrames(QproFormatPath(base_avif, MediaSink::Format::MP4_HEVC_Alpha), cf, prefix,
                         MediaSink::Format::MP4_HEVC_Alpha) > 0)
        any = true;
    if (!cf.frames.empty() && !cf.frames[0].empty())
        WriteStillAvif(base_avif, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality);
    return any;
}

namespace {

struct ItemCompCtx {
    bool ok = false;
    int comp_base = 0;
    int main_base = 0;
    TexList main_tl;
};

ItemCompCtx SetupItemCompositeSlots(bool own, const std::string& main2,
                                    const CompositeShare& share) {
    ItemCompCtx ctx;
    if (own) {
        if (g_avs.avs_fs_umount != nullptr) {
            g_avs.avs_fs_umount("/afp/packages");
            g_avs.avs_fs_umount("/data");
        }
        ctx.main_base = AfpD3D9::NextSlot();
        if (!MountAndLoadIfs(main2)) {
            LOG("PartComp", "mount qp_main2 FAILED");
            return ctx;
        }
        ParseTexturelist(ctx.main_tl, "/afp/packages");
        ctx.comp_base = AfpD3D9::NextSlot();
    } else {
        ctx.main_base = share.main_base;
        ParseTexturelist(ctx.main_tl, "/afp/packages");
        ctx.comp_base = share.comp_base;
        AfpD3D9::SetNextSlot(ctx.comp_base);
    }
    ctx.ok = true;
    return ctx;
}

void MountItemClip(uint32_t pkg, uint32_t sid, const TexList& comp_tl, const TexList& main_tl,
                   int comp_base, int main_base, const LayerJob& j) {
    auto get_afp_info = g_afpu.afpu_afp_get_info_in_package;
    if ((get_afp_info == nullptr) || (g_afp.afp_mc_get_id_by_path == nullptr)) return;
    uint64_t info[8] = {};
    int gi = (j.item_clip != nullptr) ? get_afp_info(info, pkg, j.item_clip) : -1;
    if (gi < 0 && (j.item_clip != nullptr) && std::strcmp(j.item_clip, j.layer) != 0)
        gi = get_afp_info(info, pkg, j.layer);
    if (gi >= 0 && (g_afp.afp_mc_attach_stream != nullptr) &&
        (g_afp.afp_mc_get_relative_id != nullptr)) {
        int mc = g_afp.afp_mc_get_id_by_path(sid, j.layer);
        for (int c = 0; mc >= 0 && c < 64; mc = g_afp.afp_mc_get_relative_id(mc, 6), ++c) {
            g_afp.afp_mc_attach_stream(mc, (uint32_t)info[3]);
            if (g_afp.afp_mc_get != nullptr) g_afp.afp_mc_get(mc, 0x101E, 1);
        }
    } else if (j.atlas != nullptr) {
        std::vector<uint8_t> px;
        int pw = 0;
        int ph = 0;
        if (ReadItemAtlas(comp_tl, comp_base, j.atlas, px, pw, ph)) {
            const AtlasImage* mim = FindImage(main_tl, j.atlas);
            if (mim != nullptr) {
                AfpD3D9::TexUpload((unsigned)(main_base + mim->atlas), 0x10, 0, 0, mim->x, mim->y,
                                   pw, ph, px.data());
            } else {
                LOG("PartComp", "atlas-swap: avatar has no bitmap '%s'", j.atlas);
            }
        } else {
            LOG("PartComp", "atlas-swap: item has no bitmap '%s'", j.atlas);
        }
    }
}

int ProbeItemTotal(int part_mc) {
    int total = 1;
    if (part_mc >= 0 && (g_afp.afp_mc_set != nullptr)) {
        int t = 0;
        if (g_afp.afp_mc_set(part_mc, 0x1011, &t) >= 0 && t > 1) total = (t > 600) ? 600 : t;
    }
    return total;
}

int ResolveEmitCount(int part_mc, bool animated, int total) {
    int emit = animated ? total : 1;
    if (animated && total > 2 && part_mc >= 0 && (g_afp.afp_mc_control_frame != nullptr) &&
        (g_afp.afp_mc_set != nullptr)) {
        if (g_afp.afp_stream_control != nullptr) g_afp.afp_stream_control(6, (uint32_t)part_mc);
        g_afp.afp_mc_control_frame(part_mc, 0xF08, total - 1);
        std::vector<uint8_t> tp;
        int tw = 0;
        int th = 0;
        for (int i = 0; i < 3; ++i)
            RenderFrame(tp, tw, th, true);
        int c = -1;
        if (g_afp.afp_stream_control != nullptr) g_afp.afp_stream_control(6, (uint32_t)part_mc);
        g_afp.afp_mc_set(part_mc, 0x1010, &c);
        if (c >= 0 && c < total - 1) emit = total - 1;
    }
    return emit;
}

void ApplyJobHueScope(const LayerJob& j, const TexList& comp_tl, int comp_base) {
    AfpD3D9::ResetHsvScopeRect();
    if ((j.hue_eff != nullptr) && g_hue_scope_enabled && CountWithPrefix(comp_tl, j.hue_eff) > 1) {
        ScopeHueToImage(FindImage(comp_tl, j.hue_eff), comp_base);
        if (std::strcmp(j.hue_eff, "qp_hand_l") == 0)
            ScopeHueToImage2(FindImage(comp_tl, "qp_hand_l2"), comp_base);
    }
}

struct JobEnv {
    const std::string* item_ifs = nullptr;
    uint32_t sid = 0;
    int fps = 60;
    bool detect_only = false;
    std::set<std::string>* video_out = nullptr;
    const TexList* comp_tl = nullptr;
    int comp_base = 0;
    std::vector<uint8_t>* px = nullptr;
    int* w = nullptr;
    int* h = nullptr;
};

void CaptureJobFrames(const JobEnv& env, int part_mc, int emit, ClipFrames& cf) {
    for (int K = 0; K < emit; ++K) {
        if (part_mc >= 0 && (g_afp.afp_mc_control_frame != nullptr)) {
            if (g_afp.afp_stream_control != nullptr) g_afp.afp_stream_control(6, (uint32_t)part_mc);
            g_afp.afp_mc_control_frame(part_mc, 0xF08, K);
        }
        RenderFrame(*env.px, *env.w, *env.h, false);
        if (*env.w <= 0 || *env.h <= 0) break;
        cf.cw = *env.w;
        cf.ch = *env.h;
        std::vector<uint8_t> frame = *env.px;
        UnpremultiplyBGRA(frame);
        cf.frames[K] = std::move(frame);
    }
}

int WriteJobOutputs(const LayerJob& j, const ClipFrames& cf, int emit) {
    int n = 0;
    if (emit > 1) {
        if (WriteAnimatedTriple(j.out_path, cf, "partcomp")) n = 1;
    } else if (!cf.frames.empty() && !cf.frames[0].empty()) {
        if (WriteStillAvif(j.out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) n = 1;
        if (g_clip_dump_raw) {
            std::string png = j.out_path;
            if (png.size() > 5 && png.ends_with(".avif")) png.replace(png.size() - 5, 5, ".png");
            WritePngBGRA(png, cf.frames[0].data(), cf.cw, cf.ch);
        }
    }
    if (n == 0) {
        int const pw = (cf.cw > 0) ? cf.cw : 520;
        int const ph = (cf.ch > 0) ? cf.ch : 704;
        std::vector<uint8_t> blank((size_t)pw * ph * 4, 0);
        if (WriteStillAvif(j.out_path, blank.data(), pw, ph, kQproAvifQuality)) n = 1;
    }
    return n;
}

int RenderLayerJob(const JobEnv& env, const LayerJob& j) {
    int const part_mc = (g_afp.afp_mc_get_id_by_path != nullptr)
                            ? g_afp.afp_mc_get_id_by_path(env.sid, j.layer)
                            : -1;
    int const total = ProbeItemTotal(part_mc);
    int const vcmds = (part_mc >= 0) ? ClipVisualCmdsAfterFrame0(g_afp, (uint32_t)part_mc) : -1;
    bool const animated = (vcmds != 0);
    if (g_clip_dump_raw && part_mc >= 0) {
        LOG("PartAnim", "%s layer=%s visual-cmds@f>0=%d animated=%d total=%d",
            env.item_ifs->c_str(), j.layer, vcmds, (int)animated, total);
    }
    if (animated && (env.video_out != nullptr))
        env.video_out->insert(fs::path(j.out_path).stem().string());
    if (env.detect_only) return 0;
    int const emit = ResolveEmitCount(part_mc, animated, total);
    AfpManager::SeekFrame(g_afp, 0);
    for (const char* a : kAllAvatarLayers)
        McControl::SetClipVisible(g_afp, env.sid, a, std::strcmp(a, j.layer) == 0);
    ApplyJobHueScope(j, *env.comp_tl, env.comp_base);
    AfpManager::SeekFrame(g_afp, 0);
    ClipFrames cf;
    cf.cw = *env.w;
    cf.ch = *env.h;
    cf.fps = env.fps;
    cf.frames.assign((size_t)emit, {});
    CaptureJobFrames(env, part_mc, emit, cf);
    int const n = WriteJobOutputs(j, cf, emit);
    LOG("PartComp", "%s layer=%s -> %d file(s), %d/%d frames @ %d fps (%s)", env.item_ifs->c_str(),
        j.layer, n, emit, total, env.fps, j.out_path.c_str());
    return n;
}

}

int RenderItemComposite(const std::string& game_dir, const std::string& item_ifs,
                        const std::vector<LayerJob>& jobs, const CompositeShare& share, int fps,
                        bool detect_only, std::set<std::string>* video_out) {
    if (jobs.empty()) return 0;
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const item = IfsPath(game_dir, item_ifs);
    const bool own = (share.sid == 0);
    const ItemCompCtx ctx = SetupItemCompositeSlots(own, main2, share);
    if (!ctx.ok) return 0;
    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, item, Stem(item_ifs));
    TexList compTl;
    std::string cmp_root = AfpManager::LastCompanionMountPoint();
    size_t const bar = cmp_root.find('|');
    if (bar != std::string::npos) cmp_root.resize(bar);
    ParseTexturelist(compTl, cmp_root.c_str());
    if (!AfpManager::SwitchAnimation(g_engine, "qp_motion", true))
        LOG("PartComp", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();
    for (const auto& j : jobs)
        MountItemClip(pkg, sid, compTl, ctx.main_tl, ctx.comp_base, ctx.main_base, j);
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 6; ++i)
        RenderFrame(px, w, h, true);
    JobEnv env;
    env.item_ifs = &item_ifs;
    env.sid = sid;
    env.fps = fps;
    env.detect_only = detect_only;
    env.video_out = video_out;
    env.comp_tl = &compTl;
    env.comp_base = ctx.comp_base;
    env.px = &px;
    env.w = &w;
    env.h = &h;
    int written = 0;
    for (const auto& j : jobs)
        if (RenderLayerJob(env, j) > 0) ++written;
    AfpManager::DestroyCurrentStream(g_afp);
    if (pkg != 0U) AfpManager::UnloadCompanion(g_engine, pkg);
    if (!own) {
        int const cur = AfpD3D9::NextSlot();
        for (int s = ctx.comp_base; s < cur; ++s)
            AfpD3D9::TexDestroy((unsigned)s);
        AfpD3D9::SetNextSlot(ctx.comp_base);
    }
    return written;
}

int RenderPartComposite(const std::string& game_dir, const std::string& item_ifs, const char* layer,
                        const char* hue_eff, const std::string& out_path) {
    std::vector<LayerJob> const jobs = {{.layer = layer,
                                         .item_clip = layer,
                                         .atlas = nullptr,
                                         .hue_eff = hue_eff,
                                         .out_path = out_path}};
    return RenderItemComposite(game_dir, item_ifs, jobs);
}

namespace {
int DefVisualCmdsAfterFrame0(void* def) {
    if (def == nullptr) return 0;
    auto* dp = (uint8_t*)def;
    uint8_t* ft = *(uint8_t**)(dp + 16);
    uint8_t* pool = *(uint8_t**)(dp + 8);
    if ((ft == nullptr) || (pool == nullptr)) return 0;
    int const frame_count = *(int*)(ft + 4);
    if (frame_count <= 1 || frame_count >= 100000) return 0;
    uint32_t const idx_off = *(uint32_t*)(ft + 16);
    int visual = 0;
    for (int f = 1; f < frame_count; ++f) {
        uint32_t const packed = *(uint32_t*)(ft + idx_off + ((uintptr_t)f * 4));
        int const num = (int)(packed >> 20);
        uint32_t const start = packed & 0xFFFFF;
        for (int c = 0; c < num && c < 4096; ++c) {
            uint32_t const cmd0 = *(uint32_t*)(pool + ((uintptr_t)(start + c) * 24));
            int const op = (int)((cmd0 >> 1) & 0x3FF);
            if (op == 0x7F || op == 0x80 || op == 0x88) ++visual;
        }
    }
    return visual;
}
}

namespace {
void* QpGetDef(void* work) {
    static void* (*fn)(void*) = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE m = GetModuleHandleA("afp-core.dll");
        if (m != nullptr) fn = (void* (*)(void*))((uint8_t*)m + 0x377B0);
    }
    return ((fn != nullptr) && (work != nullptr)) ? fn(work) : nullptr;
}
}

namespace {
int NodeTreeVisualCmds(void* work, int depth) {
    if ((work == nullptr) || depth > 48) return 0;
    int total = DefVisualCmdsAfterFrame0(QpGetDef(work));
    void* child = *(void**)((uint8_t*)work + 72);
    for (int i = 0; (child != nullptr) && total == 0 && i < 8192; ++i) {
        total += NodeTreeVisualCmds(child, depth + 1);
        child = *(void**)((uint8_t*)child + 88);
    }
    return total;
}
}

int ClipVisualCmdsAfterFrame0(const AfpFuncs& afp, uint32_t mc_id) {
    HMODULE afpcore = GetModuleHandleA("afp-core.dll");
    if ((afpcore == nullptr) || (afp.afp_stream_control == nullptr)) return -1;
    using resolve_t = void* (*)(uint32_t);
    auto resolve = (resolve_t)((uint8_t*)afpcore + 0x48AC0);
    int visual = -1;
    __try {
        afp.afp_stream_control(6, mc_id);
        void* work = resolve(mc_id);
        if (work != nullptr) visual = NodeTreeVisualCmds(work, 0);
    } __except (1) {
        visual = -2;
    }
    return visual;
}

namespace {
bool ClipIsAnimated(const AfpFuncs& afp, uint32_t sid, uint32_t root_mc,
                    const std::vector<AfpManager::ChildClip>& kids) {
    if (ClipVisualCmdsAfterFrame0(afp, root_mc) > 0) return true;
    for (const auto& c : kids) {
        if (afp.afp_mc_get_id_by_path == nullptr) break;
        int const cid = afp.afp_mc_get_id_by_path(sid, c.name.c_str());
        if (cid >= 0 && ClipVisualCmdsAfterFrame0(afp, (uint32_t)cid) > 0) return true;
    }
    return false;
}
}

namespace {

void ApplyClipHueScope(const TexList& tl, const std::string& clip, int slot0) {
    AfpD3D9::ResetHsvScopeRect();
    std::string effect = clip;
    size_t const us = effect.rfind('_');
    if (us != std::string::npos) effect.resize(us);
    const AtlasImage* eff_im = FindImage(tl, effect.c_str());
    bool const scoped = g_hue_scope_enabled && ScopeHueToImage(eff_im, slot0);
    if (g_hue_scope_enabled && effect == "qp_hand_l")
        ScopeHueToImage2(FindImage(tl, "qp_hand_l2"), slot0);
    LOG("QproClip", "hue scope: effect='%s' found=%d scoped=%d", effect.c_str(),
        (int)(eff_im != nullptr), (int)scoped);
}

}

void ClipOne(const std::string& game_dir, const std::string& ifs, const std::string& clip) {
    std::string const path = IfsPath(game_dir, ifs);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    int const slot0 = AfpD3D9::NextSlot();
    if (!MountAndLoadIfs(path)) {
        LOG("QproClip", "mount FAILED");
        return;
    }

    TexList tl;
    ParseTexturelist(tl);

    ApplyClipHueScope(tl, clip, slot0);

    bool const sw = AfpManager::SwitchAnimation(g_engine, clip, true);
    uint32_t cur = 0;
    uint32_t total = 0;
    uint32_t lc = 0;
    bool const got = sw && AfpManager::ReadMcPlayhead(g_afp, &cur, &total, &lc);
    LOG("QproClip", "%s clip '%s': sw=%d total=%u (%zu bitmaps)", ifs.c_str(), clip.c_str(),
        (int)sw, got ? total : 0, tl.images.size());

    bool eok = false;
    auto kids = AfpManager::EnumerateChildClips(g_afp, true, &eok);
    LOG("QproClip", "child sub-mcs (enum_ok=%d): %zu", (int)eok, kids.size());
    for (const auto& c : kids) {
        LOG("QproClip", "  child '%s' pos=(%.1f,%.1f) playhead cur=%d total=%d", c.name.c_str(),
            c.screen_x, c.screen_y, c.cur, c.total);
    }

    uint32_t const csid = AfpManager::StreamId();
    int const rootmc = AfpManager::GetRootMcId(g_afp);
    g_clip_dump_raw = true;
    int const rootv = ClipVisualCmdsAfterFrame0(g_afp, (uint32_t)rootmc);
    g_clip_dump_raw = false;
    bool const anim = ClipIsAnimated(g_afp, csid, (uint32_t)rootmc, kids);
    LOG("AnimData", "%s '%s': root visual-cmds@f>0=%d ANIMATED=%d (total=%u kids=%zu)", ifs.c_str(),
        clip.c_str(), rootv, (int)anim, total, kids.size());

    AfpD3D9::SetQproDrawProbe(true);
    g_clip_dump_raw = true;
    int const n =
        (sw && got && total > 1)
            ? RenderClipAvif((fs::path("screenshots") / "qpclip_test.avif").string(), 60, "clip")
            : 0;
    g_clip_dump_raw = false;
    AfpD3D9::SetQproDrawProbe(false);
    AfpD3D9::ResetHsvScopeRect();
    LOG("QproClip", "RenderClipAvif wrote %d frames", n);

    AfpManager::UnloadPackages(g_engine);
}

void DumpIfs(const std::string& ifs_path) {
    int const base = AfpD3D9::NextSlot();
    AfpD3D9::SetNextSlot(base);
    int const slot0 = AfpD3D9::NextSlot();
    LOG("QproDump", "mounting %s (slot base %d)", ifs_path.c_str(), slot0);
    if (!MountAndLoadIfs(ifs_path)) {
        LOG("QproDump", "mount FAILED");
        return;
    }
    TexList tl;
    if (!ParseTexturelist(tl)) {
        LOG("QproDump", "texturelist parse FAILED");
    } else {
        LOG("QproDump", "%zu bitmaps across %d atlas(es):", tl.images.size(), tl.atlas_count);
        std::unordered_map<int, std::vector<uint8_t>> atlas_px;
        std::unordered_map<int, std::pair<int, int>> atlas_dim;
        for (int a = 0; a < tl.atlas_count; ++a) {
            int aw = 0;
            int ah = 0;
            IDirect3DTexture9* tex = AfpD3D9::GetTexture(slot0 + a);
            std::vector<uint8_t> px;
            if ((tex != nullptr) && AfpD3D9::ReadTexturePixels(tex, px, aw, ah)) {
                atlas_px[a] = px;
                atlas_dim[a] = {aw, ah};
            }
            LOG("QproDump", "  atlas %d -> slot %d (%dx%d)", a, slot0 + a, aw, ah);
        }
        for (const AtlasImage& im : tl.images) {
            LOG("QproDump", "  '%s' imgrect=[%u %u %u %u] atlas=%d -> x=%d y=%d w=%d h=%d",
                im.name.c_str(), im.raw[0], im.raw[1], im.raw[2], im.raw[3], im.atlas, im.x, im.y,
                im.w, im.h);
            auto it = atlas_px.find(im.atlas);
            if (it != atlas_px.end()) {
                int const aw = atlas_dim[im.atlas].first;
                int const ah = atlas_dim[im.atlas].second;
                int cx = im.x;
                int cy = im.y;
                int cw = im.w;
                int ch = im.h;
                std::vector<uint8_t> crop = Bgra::Crop(it->second, aw, ah, cx, cy, cw, ch);
                if (!crop.empty()) {
                    WritePngBGRA((fs::path("screenshots") / ("dump_" + im.name + ".png")).string(),
                                 crop.data(), cw, ch);
                }
            }
        }
    }
    AfpManager::UnloadPackages(g_engine);
    AfpD3D9::SetNextSlot(base);
}

}
