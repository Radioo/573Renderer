#include "qpro/qpro_extract.h"
#include "engine_session.h"
#include "qpro/qpro_internal.h"
#include "qpro/qpro_status.h"

#include "afp_boot.h"
#include "boot.h"
#include "qpro/qpro_dll.h"
#include "render_backend.h"
#include "state/app_state.h"
#include "support/log.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace QproExtract {

namespace fs = std::filesystem;
using namespace detail;

namespace {
std::mutex g_mu;
Status g_status;
int g_total = 0;
int g_done = 0;
}

void PublishProgress(const char* stage) {
    {
        std::scoped_lock const lk(g_mu);
        g_status.done = g_done;
        g_status.total = g_total;
    }
    float const frac = g_total > 0 ? (float)g_done / (float)g_total : -1.0F;
    char buf[96];
    snprintf(buf, sizeof(buf), "qpro: %s (%d / %d)", stage, g_done, g_total);
    App::Global().UpdateLoadStage(buf, frac);
}

void Note(Result& res, bool failure, const std::string& label, const std::string& ifs,
          const std::string& reason) {
    if (failure) {
        ++res.failed;
    } else {
        ++res.skipped;
    }
    std::string const text = label + " (" + ifs + ") -- " + reason;
    LOG("Qpro", "%s %s", failure ? "FAILED" : "skipped", text.c_str());
    std::scoped_lock const lk(g_mu);
    g_status.issues.push_back({.text = text, .failure = failure});
}

void NoteIssue(const std::string& text, bool failure) {
    std::scoped_lock const lk(g_mu);
    g_status.issues.push_back({.text = text, .failure = failure});
}

void BumpDone() {
    ++g_done;
}

int DoneCount() {
    return g_done;
}

int TotalCount() {
    return g_total;
}

namespace {

bool WantsCat(const CategorySel& s, QproDll::Category c) {
    using C = QproDll::Category;
    switch (c) {
    case C::Head:
        return s.head;
    case C::Hand:
        return s.hand;
    case C::Hair:
        return s.hair;
    case C::Face:
        return s.face;
    case C::Body:
        return s.body;
    case C::Back:
        return s.back;
    default:
        return false;
    }
}

std::string ResolveOutRoot(const std::string& out_dir) {
    std::string clean = out_dir;
    while (!clean.empty() && (clean.back() == '/' || clean.back() == '\\'))
        clean.pop_back();
    return (fs::path(clean) / "qpro_assets").string();
}

void BeginRunStatus(const std::string& out_root) {
    {
        std::scoped_lock const lk(g_mu);
        g_status = Status{};
        g_status.running = true;
        g_status.output_dir = out_root;
    }
    App::Global().BeginLoad("qpro asset extraction");
    g_done = 0;
    g_total = 0;
}

void FailRunStatus(const std::string& error) {
    std::scoped_lock const lk(g_mu);
    g_status.running = false;
    g_status.finished = true;
    g_status.error = error;
}

void FinishRunStatus(const Result& res) {
    std::scoped_lock const lk(g_mu);
    g_status.running = false;
    g_status.finished = true;
    g_status.done = g_total;
    g_status.images = res.images;
    g_status.skipped = res.skipped;
    g_status.failed = res.failed;
}

int CountSelectedParts(const Options& opt, const QproDll::Parts& parts) {
    int sel_total = 0;
    for (int ci = 0; ci < (int)QproDll::Category::Count; ++ci) {
        if (WantsCat(opt.parts, (QproDll::Category)ci))
            sel_total += (int)parts.of((QproDll::Category)ci).size();
    }
    return sel_total;
}

std::vector<int> MeasureBackNativeFps(EngineSession& es, D3D9State& d3d, const Options& opt,
                                      const QproDll::Parts& parts, int q_limit) {
    std::vector<int> back_native_fps;
    if (!opt.parts.back) return back_native_fps;
    std::error_code ec;
    const std::vector<QproDll::Part>& blist = parts.of(QproDll::Category::Back);
    back_native_fps.assign(blist.size(), opt.fps);
    for (size_t bi = 0; bi < blist.size(); ++bi) {
        if (q_limit > 0 && std::cmp_greater_equal(bi, q_limit)) break;
        if (!opt.part_sel.selected(QproDll::Category::Back, bi)) continue;
        if (fs::exists(IfsPath(opt.game_dir, blist[bi].ifs), ec))
            back_native_fps[bi] = ProbeBackNativeFps(es, d3d, opt.game_dir, blist[bi].ifs, opt.fps);
        if ((bi % 8) == 0) {
            char b[96];
            snprintf(b, sizeof(b), "qpro: measuring background rate (%zu / %zu)", bi + 1,
                     blist.size());
            App::Global().UpdateLoadStage(b, ProgressFrac());
        }
    }
    return back_native_fps;
}

struct CatMount {
    uint32_t sid = 0;
    int comp_base = -1;
    int main_base = -1;
};

CatMount MountCategorySweep(EngineSession& es, const Options& opt) {
    CatMount m;
    const bool need_cat_mount =
        opt.parts.hand || opt.parts.face || opt.parts.hair || opt.parts.head || opt.parts.back;
    if (!need_cat_mount) return m;
    AfpManager::UmountPackagesAndData(es.avs);
    const std::string cat_main2 = IfsPath(opt.game_dir, "qp_main2.ifs");
    m.main_base = AfpD3D9::NextSlot();
    if (MountAndLoadIfs(cat_main2)) {
        TexList mtl;
        ParseTexturelist(es, mtl, "/afp/packages");
        (void)mtl;
        m.comp_base = AfpD3D9::NextSlot();
        if (!AfpManager::SwitchAnimation(es, "qp_motion", true)) {
            LOG("Qpro", "category sweep: SwitchAnimation(qp_motion) failed - continuing");
        }
        m.sid = AfpManager::StreamId();
    } else {
        LOG("Qpro", "category sweep: qp_main2 mount FAILED - all category parts -> placeholders");
    }
    return m;
}

int EmitPlaceholder(const std::string& out_root, QproDll::Category c, const std::string& pre,
                    size_t i) {
    int n = 0;
    std::string const oprefix = (fs::path(out_root) / (pre + "_" + std::to_string(i))).string();
    for (const LayerJob& j : CompositeJobs(c, oprefix)) {
        std::vector<uint8_t> blank((size_t)520 * 704 * 4, 0);
        if (WriteStillAvif(j.out_path, blank.data(), 520, 704, kQproAvifQuality)) ++n;
    }
    return n;
}

struct SweepCtx {
    const Options* opt = nullptr;
    const std::string* out_root = nullptr;
    Result* res = nullptr;
    std::set<std::string>* video_stems = nullptr;
    const std::vector<int>* back_native_fps = nullptr;
    uint32_t cat_sid = 0;
    int cat_comp_base = -1;
    int cat_main_base = -1;
    int body_done = 0;
    int processed = 0;
    int q_limit = 0;
};

struct SweepPaths {
    const std::string* ifs = nullptr;
    const std::string* oprefix = nullptr;
};

int CompositeSweepItem(EngineSession& es, D3D9State& d3d, const SweepCtx& ctx,
                       QproDll::Category cat, const SweepPaths& paths,
                       const std::vector<LayerJob>& jobs, size_t idx, bool sel) {
    const Options& opt = *ctx.opt;
    const CompositeShare share = {
        .sid = ctx.cat_sid, .comp_base = ctx.cat_comp_base, .main_base = ctx.cat_main_base};
    if (cat != QproDll::Category::Back) {
        return RenderItemComposite(es, d3d, opt.game_dir, *paths.ifs, jobs, share, opt.fps, !sel,
                                   ctx.video_stems);
    }
    std::string const bout = jobs.empty() ? (*paths.oprefix + ".avif") : jobs[0].out_path;
    int const nf = (idx < ctx.back_native_fps->size()) ? (*ctx.back_native_fps)[idx] : opt.fps;
    return RenderBackComposite(
        es, d3d, opt.game_dir, *paths.ifs, opt.fps, bout,
        {.native_fps = nf, .share = share, .detect_only = !sel, .video_out = ctx.video_stems});
}

void SweepOneItem(EngineSession& es, D3D9State& d3d, SweepCtx& ctx, QproDll::Category cat,
                  const std::string& prefix, const QproDll::Part& part, size_t idx) {
    const Options& opt = *ctx.opt;
    Result& res = *ctx.res;
    std::error_code ec;
    const bool sel = opt.part_sel.selected(cat, idx);
    ++ctx.processed;

    const std::string& ifs = part.ifs;
    std::string const path = IfsPath(opt.game_dir, ifs);
    std::string const lbl = prefix + "_" + std::to_string(idx);

    g_done = ctx.body_done + ctx.processed;
    if ((ctx.processed % 25) == 0 || idx == 0) {
        LOG("Qpro", "[%d/%d] %s_%zu (%s)", ctx.processed, res.parts, prefix.c_str(), idx,
            ifs.c_str());
        PublishProgress("extracting layers");
    }

    if (!fs::exists(path, ec)) {
        if (sel) {
            res.images += EmitPlaceholder(*ctx.out_root, cat, prefix, idx);
            Note(res, true, lbl, ifs, "no source .ifs on disk -> transparent placeholder(s)");
        }
        return;
    }

    std::string const oprefix =
        (fs::path(*ctx.out_root) / (prefix + "_" + std::to_string(idx))).string();
    std::vector<LayerJob> const jobs = CompositeJobs(cat, oprefix);

    if (ctx.cat_sid == 0) {
        if (sel) {
            res.images += EmitPlaceholder(*ctx.out_root, cat, prefix, idx);
            Note(res, true, lbl, ifs, "qp_main2 not mounted -> transparent placeholder(s)");
        }
        return;
    }
    res.images +=
        CompositeSweepItem(es, d3d, ctx, cat, {.ifs = &ifs, .oprefix = &oprefix}, jobs, idx, sel);
    if (!sel) return;

    for (const LayerJob& j : jobs) {
        if (!fs::exists(j.out_path, ec)) {
            std::vector<uint8_t> blank((size_t)520 * 704 * 4, 0);
            if (WriteStillAvif(j.out_path, blank.data(), 520, 704, kQproAvifQuality)) res.images++;
            Note(res, true, lbl, ifs, "layer missing after composite -> transparent placeholder");
        }
    }
}

void RunCategorySweep(EngineSession& es, D3D9State& d3d, SweepCtx& ctx,
                      const QproDll::Parts& parts) {
    const QproDll::Category kCatOrder[] = {
        QproDll::Category::Hand, QproDll::Category::Face, QproDll::Category::Hair,
        QproDll::Category::Head, QproDll::Category::Back,
    };
    for (QproDll::Category const cat : kCatOrder) {
        if (!WantsCat(ctx.opt->parts, cat)) continue;
        const std::string prefix = QproDll::Prefix(cat);
        const std::vector<QproDll::Part>& list = parts.of(cat);
        for (size_t idx = 0; idx < list.size(); ++idx) {
            if (ctx.q_limit > 0 && std::cmp_greater_equal(idx, ctx.q_limit)) break;
            SweepOneItem(es, d3d, ctx, cat, prefix, list[idx], idx);
        }
    }
}

void WritePartsJson(const QproDll::Parts& parts, const std::string& out_root, Result& res) {
    std::string const json = QproDll::ToJson(parts);
    res.json_path = (fs::path(out_root) / "2dx_qpro.json").string();
    std::ofstream jf(res.json_path, std::ios::binary | std::ios::trunc);
    if (jf) jf.write(json.data(), (std::streamsize)json.size());
}

void UnmountSweepPackages(EngineSession& es) {
    AfpManager::UnloadPackages(es);
    AfpManager::UmountPackagesAndData(es.avs);
}

void WriteVideosJson(const std::string& out_root, std::set<std::string>& video_stems) {
    std::error_code dec;
    for (fs::directory_iterator it(out_root, dec), dend; !dec && it != dend; it.increment(dec)) {
        std::string ext = it->path().extension().string();
        for (char& ch : ext)
            if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (ext == ".webm" || ext == ".mp4") video_stems.insert(it->path().stem().string());
    }
    std::string vjson = "{\n";
    size_t idx = 0;
    for (const auto& stem : video_stems) {
        vjson += "  \"" + stem + "\": true";
        vjson += (++idx < video_stems.size()) ? ",\n" : "\n";
    }
    vjson += "}\n";
    const std::string vpath = (fs::path(out_root) / "qpro_videos.json").string();
    std::ofstream vf(vpath, std::ios::binary | std::ios::trunc);
    if (vf) vf.write(vjson.data(), (std::streamsize)vjson.size());
    LOG("Qpro", "qpro_videos.json: %zu part(s) have video (all parts detected + dir scan)",
        video_stems.size());
}
}

Result Run(EngineSession& es, D3D9State& d3d, const Options& opt) {
    Result res;
    const std::string out_root = ResolveOutRoot(opt.out_dir);
    BeginRunStatus(out_root);

    QproDll::Parts const parts = QproDll::Read(opt.game_dir);
    if (!parts.ok()) {
        res.error = parts.error;
        FailRunStatus(parts.error);
        App::Global().EndLoad();
        return res;
    }
    res.parts = CountSelectedParts(opt, parts);

    std::set<std::string> video_stems;
    g_total = res.parts;
    {
        std::scoped_lock const lk(g_mu);
        g_status.total = g_total;
    }

    std::error_code ec;
    fs::create_directories(out_root, ec);

    int const base = AfpD3D9::NextSlot();

    if (opt.parts.body) RunBodyPass(es, d3d, parts, opt.game_dir, out_root, res, opt.part_sel);
    AfpD3D9::SetNextSlot(base);

    const int q_limit = QproLimit();
    const std::vector<int> back_native_fps = MeasureBackNativeFps(es, d3d, opt, parts, q_limit);
    const CatMount cm = MountCategorySweep(es, opt);

    SweepCtx ctx;
    ctx.opt = &opt;
    ctx.out_root = &out_root;
    ctx.res = &res;
    ctx.video_stems = &video_stems;
    ctx.back_native_fps = &back_native_fps;
    ctx.cat_sid = cm.sid;
    ctx.cat_comp_base = cm.comp_base;
    ctx.cat_main_base = cm.main_base;
    ctx.body_done = g_done;
    ctx.q_limit = q_limit;
    RunCategorySweep(es, d3d, ctx, parts);
    UnmountSweepPackages(es);

    WritePartsJson(parts, out_root, res);
    WriteVideosJson(out_root, video_stems);

    LOG("Qpro", "DONE: %d parts, %d images written, %d skipped (no source), %d failed -> %s",
        res.parts, res.images, res.skipped, res.failed, out_root.c_str());

    FinishRunStatus(res);
    App::Global().EndLoad();
    return res;
}

Status GetStatus() {
    std::scoped_lock const lk(g_mu);
    return g_status;
}

bool IsRunning() {
    std::scoped_lock const lk(g_mu);
    return g_status.running;
}

}
