#include <cstdio>
#include "qpro_internal.h"
#include "qpro_extract.h"
#include "boot.h"
#include "afp_boot.h"
#include "app_globals.h"
#include "mc_control.h"
#include "render_backend.h"
#include "support/log.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <utility>

namespace QproExtract {
namespace fs = std::filesystem;
using namespace detail;

namespace {

void MountHeadItemClipLogged(uint32_t pkg, uint32_t sid, const char* layer) {
    auto get_afp_info = g_afpu.afpu_afp_get_info_in_package;
    if ((get_afp_info == nullptr) || (g_afp.afp_mc_attach_stream == nullptr) ||
        (g_afp.afp_mc_get_id_by_path == nullptr) || (g_afp.afp_mc_get_relative_id == nullptr)) {
        LOG("QproHeadC", "mount '%s': missing afp fn(s)", layer);
        return;
    }
    int mc = g_afp.afp_mc_get_id_by_path(sid, layer);
    LOG("QproHeadC", "layer '%s' mc=0x%x", layer, mc);
    int n = 0;
    for (int count = 0; mc >= 0 && count < 64; mc = g_afp.afp_mc_get_relative_id(mc, 6), ++count) {
        uint64_t info[8] = {};
        int const gi = get_afp_info(info, pkg, layer);
        auto data_id = (uint32_t)info[3];
        int64_t ar = -999;
        if (gi >= 0) ar = g_afp.afp_mc_attach_stream(mc, data_id);
        LOG("QproHeadC", "  sub-mc=0x%x get_info=%d data_id=0x%x attach=%lld", mc, gi, data_id,
            (long long)ar);
        if (gi < 0) continue;
        if (g_afp.afp_mc_get != nullptr) g_afp.afp_mc_get(mc, 0x101E, 1);
        ++n;
    }
    LOG("QproHeadC", "mounted item clip '%s' onto %d sub-mc(s)", layer, n);
}

void HideNonHeadLayers(uint32_t sid) {
    static const char* kNonHead[] = {
        "qpro_bg",           "qp_cat_1",       "qp_cat_2",          "qp_cat_3",
        "qp_hair_b",         "qp_body_b",      "qp_arm_r_upper",    "qp_arm_r_lower",
        "qp_hand_r_neutral", "qp_leg_r_lower", "qp_leg_r_upper",    "qp_leg_l_lower",
        "qp_leg_l_upper",    "qp_body_f",      "qp_face_neutral",   "qp_hair_f",
        "qp_arm_l_upper",    "qp_arm_l_lower", "qp_hand_l_neutral",
    };
    for (const char* h : kNonHead)
        McControl::SetClipVisible(g_afp, sid, h, false);
}

void LogHeadClipState(uint32_t sid) {
    for (const char* L : {"qp_head_f_neutral", "qp_head_b_neutral"}) {
        int const lmc = g_afp.afp_mc_get_id_by_path(sid, L);
        int cur = -1;
        int total = -1;
        if (lmc >= 0 && (g_afp.afp_mc_set != nullptr)) {
            g_afp.afp_mc_set(lmc, 0x1010, &cur);
            g_afp.afp_mc_set(lmc, 0x1011, &total);
        }
        LOG("QproHeadC", "post-attach '%s' lmc=0x%x cur=%d total=%d (eagle clip=600)", L, lmc, cur,
            total);
    }
}

void RenderHeadDumpSweep(uint32_t sid) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    const int dumps[] = {0, 60, 120, 240, 360, 480, 540};
    int di = 0;
    for (int f = 0; f <= 540; ++f) {
        for (const char* L : {"qp_head_f_neutral", "qp_head_b_neutral"}) {
            int const lmc = g_afp.afp_mc_get_id_by_path(sid, L);
            if (lmc >= 0 && (g_afp.afp_mc_control_frame != nullptr))
                g_afp.afp_mc_control_frame(lmc, 0xF08, f);
        }
        RenderFrame(px, w, h, false);
        if (std::cmp_less(di, sizeof(dumps) / sizeof(dumps[0])) && f == dumps[di]) {
            int curf = -1;
            int const lmc = g_afp.afp_mc_get_id_by_path(sid, "qp_head_f_neutral");
            if (lmc >= 0 && (g_afp.afp_mc_set != nullptr)) g_afp.afp_mc_set(lmc, 0x1010, &curf);
            LOG("QproHeadC", "frame %d: qp_head_f_neutral cur=%d (did deep-goto-play advance it?)",
                f, curf);
            char nm[64];
            snprintf(nm, sizeof(nm), "eaglecomp_f%d.png", f);
            if (w > 0 && h > 0)
                WritePngBGRA((fs::path("screenshots") / nm).string(), px.data(), w, h);
            ++di;
        }
    }
    LOG("QproHeadC", "rendered %dx%d (541 frames, dumped %d)", w, h, di);
}

void LogHeadChildren(uint32_t sid) {
    for (const char* L : {"qp_head_f_neutral", "qp_head_b_neutral"}) {
        int const lmc = g_afp.afp_mc_get_id_by_path(sid, L);
        int const fc = (lmc >= 0 && (g_afp.afp_mc_get_relative_id != nullptr))
                           ? g_afp.afp_mc_get_relative_id(lmc, 1)
                           : -99;
        LOG("QproHeadC", "post-render '%s' lmc=0x%x first_child=0x%x", L, lmc, fc);
        int c = fc;
        int n = 0;
        while (c >= 0 && n < 24) {
            int ccur = -1;
            int ctot = -1;
            if (g_afp.afp_mc_set != nullptr) {
                g_afp.afp_mc_set(c, 0x1010, &ccur);
                g_afp.afp_mc_set(c, 0x1011, &ctot);
            }
            LOG("QproHeadC", "    child[%d] 0x%x cur=%d total=%d", n, c, ccur, ctot);
            int const nx = g_afp.afp_mc_get_relative_id(c, 3);
            if (nx == c) break;
            c = nx;
            ++n;
        }
    }
}

}

void HeadComposite(const std::string& game_dir, const std::string& head_ifs) {
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const head = IfsPath(game_dir, head_ifs);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    int const main_base = AfpD3D9::NextSlot();
    (void)main_base;
    if (!MountAndLoadIfs(main2)) {
        LOG("QproHeadC", "mount qp_main2 FAILED");
        return;
    }
    TexList mainTl;
    ParseTexturelist(mainTl, "/afp/packages");

    int const comp_base = AfpD3D9::NextSlot();
    (void)comp_base;
    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, head, Stem(head_ifs));
    LOG("QproHeadC", "LoadCompanion(%s) pkg=0x%x", head_ifs.c_str(), pkg);

    if (!AfpManager::SwitchAnimation(g_engine, "qp_motion", true))
        LOG("QproHeadC", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();

    MountHeadItemClipLogged(pkg, sid, "qp_head_f_neutral");
    MountHeadItemClipLogged(pkg, sid, "qp_head_b_neutral");
    HideNonHeadLayers(sid);
    LogHeadClipState(sid);
    AfpManager::SeekFrame(g_afp, 0);

    RenderHeadDumpSweep(sid);
    LogHeadChildren(sid);

    if (pkg != 0U) AfpManager::UnloadCompanion(g_engine, pkg);
}

}
