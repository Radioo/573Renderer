#include "qpro/qpro_internal.h"
#include "engine_session.h"

#include "afp_boot.h"
#include "boot.h"
#include "qpro/qpro_dll.h"
#include "qpro/qpro_extract.h"
#include "qpro/qpro_scan.h"
#include "qpro/qpro_status.h"
#include "qpro/qpro_walk.h"
#include "render_backend.h"
#include "support/log.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace QproExtract {

namespace fs = std::filesystem;
using namespace detail;

namespace {

constexpr int kBodyCanvasW = 520;
constexpr int kBodyCanvasH = 704;

const char* kBodyClips[] = {
    "qp_body_f",      "qp_body_b",      "qp_arm_r_upper", "qp_arm_r_lower", "qp_arm_l_upper",
    "qp_arm_l_lower", "qp_leg_r_upper", "qp_leg_r_lower", "qp_leg_l_upper", "qp_leg_l_lower",
};

void HideNonBody(EngineSession& es, uint32_t sid) {
    HideAllLayersExcept(es, sid, kBodyClips);
}

int SwapBodyPieces(EngineSession& es, uint32_t comp_pkg, uint32_t sid) {
    int n = 0;
    for (const char* c : kBodyClips) {
        if (AfpManager::SwapClipBitmapFromCompanion(es, comp_pkg, sid, c) > 0) ++n;
    }
    return n;
}

bool BodyCanvasOk(D3D9State& d3d, size_t count, Result& res) {
    int ow = 0;
    int oh = 0;
    d3d.GetOffscreenSize(ow, oh);
    if (ow == kBodyCanvasW && oh == kBodyCanvasH) return true;
    LOG("Qpro", "BODY needs render size %dx%d (got %dx%d); skipping %zu bodies", kBodyCanvasW,
        kBodyCanvasH, ow, oh, count);
    res.failed += (int)count;
    NoteIssue("all body parts -- render must be 520x704 "
              "(Setup > \"qpro avatar\" preset, then re-Load)",
              true);
    return false;
}

bool MountBodyMain(EngineSession& es, const std::string& game_dir, size_t count, Result& res) {
    AfpManager::UmountPackagesAndData(es.avs);
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    if (!MountAndLoadIfs(main2)) {
        LOG("Qpro", "BODY: qp_main2 mount FAILED; skipping %zu bodies", count);
        res.failed += (int)count;
        NoteIssue("all body parts -- qp_main2.ifs failed to mount", true);
        return false;
    }
    return true;
}

struct BodyPassCtx {
    const std::string* game_dir = nullptr;
    const std::string* out_dir = nullptr;
    const std::string* prefix = nullptr;
    int comp_base = 0;
};

void ExtractOneBody(EngineSession& es, D3D9State& d3d, const BodyPassCtx& ctx,
                    const QproDll::Part& part, size_t idx, Result& res) {
    std::error_code ec;
    std::string const path = IfsPath(*ctx.game_dir, part.ifs);
    std::string const lbl = *ctx.prefix + "_" + std::to_string(idx);
    if (!fs::exists(path, ec)) {
        Note(res, false, lbl, part.ifs, "no source .ifs on disk");
        return;
    }

    AfpD3D9::SetNextSlot(ctx.comp_base);
    uint32_t const pkg = AfpManager::LoadCompanion(es, path, Stem(part.ifs));
    if (pkg == 0U) {
        Note(res, true, lbl, part.ifs, "LoadCompanion failed");
        return;
    }
    if (!AfpManager::SwitchAnimation(es, "qp_motion", true)) {
        LOG("Qpro", "SwitchAnimation(qp_motion) failed for %s - continuing", part.ifs.c_str());
    }
    uint32_t const sid = AfpManager::StreamId();
    int const nsw = SwapBodyPieces(es, pkg, sid);
    if (idx == 0 && nsw == 0) {
        LOG("Qpro", "BODY WARNING: 0 pieces re-pointed - bodies will be blank");
    }
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int s = 0; s < 2; ++s) {
        RenderFrame(es, d3d, px, w, h, true);
        HideNonBody(es, sid);
    }
    AfpManager::SeekFrame(es.afp, 0);
    HideNonBody(es, sid);
    RenderFrame(es, d3d, px, w, h, false);

    bool wrote = false;
    if (w > 0 && h > 0) {
        std::vector<uint8_t> frame = px;
        UnpremultiplyBGRA(frame);
        std::string const out =
            (fs::path(*ctx.out_dir) / (*ctx.prefix + "_" + std::to_string(idx) + ".avif")).string();
        wrote = !frame.empty() && WriteStillAvif(out, frame.data(), w, h, kQproAvifQuality);
    }
    if (wrote) {
        res.images++;
    } else {
        char why[96];
        snprintf(why, sizeof(why), "body render/write failed (got %dx%d, need %dx%d)", w, h,
                 kBodyCanvasW, kBodyCanvasH);
        Note(res, true, lbl, part.ifs, why);
    }

    AfpManager::UnloadCompanion(es, pkg);
    int const cur = AfpD3D9::NextSlot();
    for (int s = ctx.comp_base; s < cur; ++s)
        AfpD3D9::TexDestroy((unsigned)s);
    AfpD3D9::SetNextSlot(ctx.comp_base);
}

}

void RunBodyPass(EngineSession& es, D3D9State& d3d, const QproDll::Parts& parts,
                 const std::string& game_dir, const std::string& out_dir, Result& res,
                 const PartSelection& part_sel) {
    const std::vector<QproDll::Part>& list = parts.of(QproDll::Category::Body);
    const std::string prefix = QproDll::Prefix(QproDll::Category::Body);

    if (!BodyCanvasOk(d3d, list.size(), res)) return;
    if (!MountBodyMain(es, game_dir, list.size(), res)) return;
    if (!AfpManager::SwitchAnimation(es, "qp_motion", true)) {
        LOG("Qpro", "SwitchAnimation(qp_motion) failed for body pass - continuing");
    }
    uint32_t const sid = AfpManager::StreamId();

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 2; ++i)
        RenderFrame(es, d3d, px, w, h, true);
    HideNonBody(es, sid);

    BodyPassCtx ctx;
    ctx.game_dir = &game_dir;
    ctx.out_dir = &out_dir;
    ctx.prefix = &prefix;
    ctx.comp_base = AfpD3D9::NextSlot();
    for (size_t idx = 0; idx < list.size(); ++idx) {
        if (QproLimit() > 0 && (int)idx >= QproLimit()) break;
        if (!part_sel.selected(QproDll::Category::Body, idx)) continue;
        BumpDone();
        if ((idx % 20) == 0) {
            LOG("Qpro", "[body %zu/%zu] %s", idx, list.size(), list[idx].ifs.c_str());
            PublishProgress("assembling bodies");
        }
        ExtractOneBody(es, d3d, ctx, list[idx], idx, res);
    }

    AfpManager::UnloadAllCompanions(es);
    AfpManager::UnloadPackages(es);
    AfpManager::UmountPackagesAndData(es.avs);
}

void BodyOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& body_ifs) {
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const body = IfsPath(game_dir, body_ifs);

    AfpManager::UmountPackagesAndData(es.avs);
    if (!MountAndLoadIfs(main2)) {
        LOG("QproBody", "mount qp_main2 FAILED");
        return;
    }

    uint32_t const pkg = AfpManager::LoadCompanion(es, body, Stem(body_ifs));
    LOG("QproBody", "LoadCompanion(%s) pkg=0x%x", body_ifs.c_str(), pkg);

    if (!AfpManager::SwitchAnimation(es, "qp_motion", true))
        LOG("QproBody", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();

    int const swapped = SwapBodyPieces(es, pkg, sid);
    LOG("QproBody", "re-pointed %d/%d body pieces", swapped,
        (int)(sizeof(kBodyClips) / sizeof(kBodyClips[0])));
    HideNonBody(es, sid);

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 2; ++i) {
        RenderFrame(es, d3d, px, w, h, true);
        HideNonBody(es, sid);
    }
    RenderFrame(es, d3d, px, w, h, false);

    LOG("QproBody", "rendered %dx%d (sid=0x%x)", w, h, sid);
    if (w > 0 && h > 0)
        WritePngBGRA((fs::path("screenshots") / "qpbodyone.png").string(), px.data(), w, h);

    if (pkg != 0U) AfpManager::UnloadCompanion(es, pkg);
}

}
