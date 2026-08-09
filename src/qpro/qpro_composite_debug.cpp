#include <cstdio>
#include "engine_session.h"
#include "qpro/qpro_internal.h"
#include "qpro/qpro_extract.h"
#include "boot.h"
#include "afp_boot.h"
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

void MountHeadItemClipLogged(EngineSession& es, uint32_t pkg, uint32_t sid, const char* layer) {
    AttachClipStream(es, pkg, sid, layer, layer, "QproHeadC");
}

constexpr const char* kBothHeads[] = {"qp_head_f_neutral", "qp_head_b_neutral"};

void HideNonHeadLayers(EngineSession& es, uint32_t sid) {
    HideAllLayersExcept(es, sid, kBothHeads);
}

void LogHeadClipState(EngineSession& es, uint32_t sid) {
    for (const char* L : {"qp_head_f_neutral", "qp_head_b_neutral"}) {
        int const lmc = es.afp.afp_mc_get_id_by_path(sid, L);
        int cur = -1;
        int total = -1;
        if (lmc >= 0 && (es.afp.afp_mc_set != nullptr)) {
            es.afp.afp_mc_set(lmc, 0x1010, &cur);
            es.afp.afp_mc_set(lmc, 0x1011, &total);
        }
        LOG("QproHeadC", "post-attach '%s' lmc=0x%x cur=%d total=%d (eagle clip=600)", L, lmc, cur,
            total);
    }
}

void RenderHeadDumpSweep(EngineSession& es, D3D9State& d3d, uint32_t sid) {
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    const int dumps[] = {0, 60, 120, 240, 360, 480, 540};
    int di = 0;
    for (int f = 0; f <= 540; ++f) {
        for (const char* L : {"qp_head_f_neutral", "qp_head_b_neutral"}) {
            int const lmc = es.afp.afp_mc_get_id_by_path(sid, L);
            if (lmc >= 0 && (es.afp.afp_mc_control_frame != nullptr))
                es.afp.afp_mc_control_frame(lmc, 0xF08, f);
        }
        RenderFrame(es, d3d, px, w, h, false);
        if (std::cmp_less(di, sizeof(dumps) / sizeof(dumps[0])) && f == dumps[di]) {
            int curf = -1;
            int const lmc = es.afp.afp_mc_get_id_by_path(sid, "qp_head_f_neutral");
            if (lmc >= 0 && (es.afp.afp_mc_set != nullptr)) es.afp.afp_mc_set(lmc, 0x1010, &curf);
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

void LogHeadChildren(EngineSession& es, uint32_t sid) {
    for (const char* L : {"qp_head_f_neutral", "qp_head_b_neutral"}) {
        int const lmc = es.afp.afp_mc_get_id_by_path(sid, L);
        int const fc = (lmc >= 0 && (es.afp.afp_mc_get_relative_id != nullptr))
                           ? es.afp.afp_mc_get_relative_id(lmc, 1)
                           : -99;
        LOG("QproHeadC", "post-render '%s' lmc=0x%x first_child=0x%x", L, lmc, fc);
        int c = fc;
        int n = 0;
        while (c >= 0 && n < 24) {
            int ccur = -1;
            int ctot = -1;
            if (es.afp.afp_mc_set != nullptr) {
                es.afp.afp_mc_set(c, 0x1010, &ccur);
                es.afp.afp_mc_set(c, 0x1011, &ctot);
            }
            LOG("QproHeadC", "    child[%d] 0x%x cur=%d total=%d", n, c, ccur, ctot);
            int const nx = es.afp.afp_mc_get_relative_id(c, 3);
            if (nx == c) break;
            c = nx;
            ++n;
        }
    }
}

}

void HeadComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                   const std::string& head_ifs) {
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const head = IfsPath(game_dir, head_ifs);
    AfpManager::UmountPackagesAndData(es.avs);
    int const main_base = AfpD3D9::NextSlot();
    (void)main_base;
    if (!MountAndLoadIfs(main2)) {
        LOG("QproHeadC", "mount qp_main2 FAILED");
        return;
    }
    TexList mainTl;
    ParseTexturelist(es, mainTl, "/afp/packages");

    int const comp_base = AfpD3D9::NextSlot();
    (void)comp_base;
    uint32_t const pkg = AfpManager::LoadCompanion(es, head, Stem(head_ifs));
    LOG("QproHeadC", "LoadCompanion(%s) pkg=0x%x", head_ifs.c_str(), pkg);

    if (!AfpManager::SwitchAnimation(es, "qp_motion", true))
        LOG("QproHeadC", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();

    MountHeadItemClipLogged(es, pkg, sid, "qp_head_f_neutral");
    MountHeadItemClipLogged(es, pkg, sid, "qp_head_b_neutral");
    HideNonHeadLayers(es, sid);
    LogHeadClipState(es, sid);
    AfpManager::SeekFrame(es.afp, 0);

    RenderHeadDumpSweep(es, d3d, sid);
    LogHeadChildren(es, sid);

    if (pkg != 0U) AfpManager::UnloadCompanion(es, pkg);
}

}
