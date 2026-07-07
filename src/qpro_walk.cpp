#include <cstdio>
#include "formats/bgra_crop.h"
#include <utility>
#include "media/media_format.h"
#include <functional>
#include "state/app_state.h"
#include "qpro_walk.h"

#include "qpro_internal.h"
#include "qpro_dll.h"
#include "afp_boot.h"
#include "app_globals.h"
#include "render_backend.h"
#include "media_sink.h"
#include "support/log.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <cmath>

namespace QproExtract::detail {

namespace fs = std::filesystem;

const std::vector<Layer>& LayersFor(QproDll::Category c) {
    static const std::vector<Layer> body = {
        {.suffix = "",
         .candidates = {"qp_body_f", "qp_body"},
         .clip_candidates = {"qp_body_f_neutral", "qp_body_f"}}};
    static const std::vector<Layer> head = {
        {.suffix = "_b",
         .candidates = {"qp_head_b"},
         .clip_candidates = {"qp_head_b_neutral", "qp_head_b"}},
        {.suffix = "_f",
         .candidates = {"qp_head_f", "qp_head"},
         .clip_candidates = {"qp_head_f_neutral", "qp_head_f"}}};
    static const std::vector<Layer> face = {{.suffix = "",
                                             .candidates = {"qp_face", "qp_face_neutral"},
                                             .clip_candidates = {"qp_face_neutral", "qp_face"}}};
    static const std::vector<Layer> hair = {
        {.suffix = "_b",
         .candidates = {"qp_hair_b"},
         .clip_candidates = {"qp_hair_b_neutral", "qp_hair_b"}},
        {.suffix = "_f",
         .candidates = {"qp_hair_f"},
         .clip_candidates = {"qp_hair_f_neutral", "qp_hair_f"}}};
    static const std::vector<Layer> hand = {
        {.suffix = "_l",
         .candidates = {"qp_hand_l"},
         .clip_candidates = {"qp_hand_l_neutral", "qp_hand_l"}},
        {.suffix = "_r",
         .candidates = {"qp_hand_r"},
         .clip_candidates = {"qp_hand_r_neutral", "qp_hand_r"}}};
    static const std::vector<Layer> back = {{.suffix = "",
                                             .candidates = {"qp_bg", "qp_back", "qp_default_bg"},
                                             .clip_candidates = {"qp_bg"}}};
    static const std::vector<Layer> none;
    switch (c) {
    case QproDll::Category::Body:
        return body;
    case QproDll::Category::Head:
        return head;
    case QproDll::Category::Face:
        return face;
    case QproDll::Category::Hair:
        return hair;
    case QproDll::Category::Hand:
        return hand;
    case QproDll::Category::Back:
        return back;
    default:
        return none;
    }
}

void FixedCanvasFor(QproDll::Category cat, const char* suffix, int& x, int& y, int& w, int& h) {
    x = 0;
    y = 0;
    w = 0;
    h = 0;
    if (cat == QproDll::Category::Head) {
        w = kHeadCanvasW;
        h = kHeadCanvasH;
    } else if (cat == QproDll::Category::Hair) {
        w = kHairCanvasW;
        h = kHairCanvasH;
    } else if (cat == QproDll::Category::Hand) {
        h = kHandCanvasH;
        w = ((suffix != nullptr) && std::strcmp(suffix, "_r") == 0) ? kHandCanvasRW : kHandCanvasLW;
    } else if (cat == QproDll::Category::Face) {
        w = kFaceCanvasW;
        h = kFaceCanvasH;
    } else if (cat == QproDll::Category::Back) {
        x = kBackCropX;
        y = kBackCropY;
        w = kBackCropW;
        h = kBackCropH;
    }
}

bool CategoryIsFrontBackPair(QproDll::Category cat) {
    return cat == QproDll::Category::Head || cat == QproDll::Category::Hair;
}

bool CategoryCanvasIsHardCrop(QproDll::Category cat) {
    return cat != QproDll::Category::Face;
}

const char* PairFrontImage(QproDll::Category cat) {
    if (cat == QproDll::Category::Head) return "qp_head_f";
    if (cat == QproDll::Category::Hair) return "qp_hair_f";
    return nullptr;
}

void UnpremultiplyBGRA(std::vector<uint8_t>& bgra) {
    const size_t n = bgra.size() / 4;
    uint8_t* p = bgra.data();
    for (size_t i = 0; i < n; ++i, p += 4) {
        const int a = p[3];
        if (a == 0 || a == 255) continue;
        for (int c = 0; c < 3; ++c) {
            int const v = (p[c] * 255 + a / 2) / a;
            p[c] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}

int AfpFpsFromSteps(int steps, int frames, int fallback_fps) {
    if (frames < 1 || steps < 1) return fallback_fps;
    int k = (steps + frames / 2) / frames;
    k = std::max(k, 1);
    k = std::min(k, 12);
    return 120 / k;
}

void UnionContentBBox(const std::vector<uint8_t>& bgra, int w, int h, int& x0, int& y0, int& x1,
                      int& y1) {
    const uint8_t* p = bgra.data();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (p[((size_t)((y * w) + x) * 4) + 3] != 0) {
                x0 = std::min(x, x0);
                y0 = std::min(y, y0);
                x1 = std::max(x, x1);
                y1 = std::max(y, y1);
            }
        }
    }
}

int SwitchToAnimatedClip(const std::vector<const char*>& clips) {
    for (const char* clip : clips) {
        if (AfpManager::SwitchAnimation(g_engine, clip, true)) {
            uint32_t cur = 0;
            uint32_t total = 0;
            uint32_t lc = 0;
            if (AfpManager::ReadMcPlayhead(g_afp, &cur, &total, &lc) && total >= 1)
                return (int)total;
        }
    }
    return 0;
}

namespace {

struct ClipCapture {
    std::vector<std::vector<uint8_t>> buf;
    int w = 0;
    int h = 0;
    int bx0 = 1 << 30;
    int by0 = 1 << 30;
    int bx1 = -1;
    int by1 = -1;
    int got = 0;
    int guard = 0;
};

ClipCapture CollectClipFrames(int nframes, const char* label) {
    ClipCapture cap;
    const int gmax = (nframes * 8) + 480;
    std::vector<uint8_t> px;
    cap.buf.assign((size_t)nframes, {});
    AfpManager::SeekFrame(g_afp, 0);
    while (cap.got < nframes && cap.guard < gmax) {
        ++cap.guard;
        RenderFrame(px, cap.w, cap.h, true);
        uint32_t cur = 0;
        uint32_t t = 0;
        uint32_t lc = 0;
        AfpManager::ReadMcPlayhead(g_afp, &cur, &t, &lc);
        if ((int)cur >= 0 && std::cmp_less(cur, nframes) && cap.buf[cur].empty()) {
            UnionContentBBox(px, cap.w, cap.h, cap.bx0, cap.by0, cap.bx1, cap.by1);
            cap.buf[cur] = px;
            ++cap.got;
        }
        if ((cap.guard & 15) == 0) {
            char b[128];
            snprintf(b, sizeof(b), "qpro: rendering animated %s (frame %d / %d)",
                     (label != nullptr) ? label : "part", cap.got, nframes);
            App::Global().UpdateLoadStage(b, ProgressFrac());
        }
    }
    return cap;
}

bool ResolveClipCropRect(ClipCapture& cap, int nframes, int fcx, int fcy, int fcw, int fch, int& cw,
                         int& ch) {
    if (fcw > 0 && fch > 0) {
        if (cap.bx1 < cap.bx0 || cap.by1 < cap.by0) {
            LOG("Qpro", "clip fixed-crop on empty content (total=%d)", nframes);
            return false;
        }
        float const xofs = AfpD3D9::RenderXOffset();
        if (xofs > 0.0F) {
            int const o = (int)std::lroundf(xofs);
            cap.bx0 = (cap.bx0 < o) ? cap.bx0 : o;
        } else {
            cap.bx0 = fcx;
        }
        cap.by0 = fcy;
        cw = fcw;
        ch = fch;
        return true;
    }
    if (cap.bx1 < cap.bx0 || cap.by1 < cap.by0) {
        LOG("Qpro", "clip empty bbox (total=%d, frame %dx%d)", nframes, cap.w, cap.h);
        return false;
    }
    cw = cap.bx1 - cap.bx0 + 1;
    ch = cap.by1 - cap.by0 + 1;
    if ((cw & 1) != 0) {
        if (cap.bx0 + cw < cap.w) {
            ++cw;
        } else if (cap.bx0 > 0) {
            --cap.bx0, ++cw;
        }
    }
    if ((ch & 1) != 0) {
        if (cap.by0 + ch < cap.h) {
            ++ch;
        } else if (cap.by0 > 0) {
            --cap.by0, ++ch;
        }
    }
    return true;
}

void CropClipFrames(ClipCapture& cap, ClipFrames& out, int nframes) {
    out.frames.assign((size_t)nframes, {});
    for (int f = 0; f < nframes; ++f) {
        if (cap.buf[f].empty()) continue;
        int x = cap.bx0;
        int y = cap.by0;
        int ww = out.cw;
        int hh = out.ch;
        std::vector<uint8_t> crop = Bgra::Crop(cap.buf[f], cap.w, cap.h, x, y, ww, hh);
        cap.buf[f].clear();
        cap.buf[f].shrink_to_fit();
        if (!crop.empty() && ww == out.cw && hh == out.ch) {
            UnpremultiplyBGRA(crop);
            out.frames[f] = std::move(crop);
        }
    }
}

}

bool CaptureClipFrames(ClipFrames& out, int fps, const char* label, int fcx, int fcy, int fcw,
                       int fch) {
    out = ClipFrames{};
    uint32_t c0 = 0;
    uint32_t total = 0;
    uint32_t l0 = 0;
    if (!AfpManager::ReadMcPlayhead(g_afp, &c0, &total, &l0) || total <= 1) return false;
    int const nframes = (int)total;

    ClipCapture cap = CollectClipFrames(nframes, label);

    int cw = 0;
    int ch = 0;
    if (!ResolveClipCropRect(cap, nframes, fcx, fcy, fcw, fch, cw, ch)) return false;

    out.fps = AfpFpsFromSteps(cap.guard, cap.got, fps);
    LOG("Qpro", "clip fps probe: total=%d steps=%d got=%d -> afp_fps=%d", nframes, cap.guard,
        cap.got, out.fps);

    out.cw = cw;
    out.ch = ch;
    CropClipFrames(cap, out, nframes);
    return true;
}

std::string QproFormatPath(const std::string& avif_path, MediaSink::Format fmt) {
    if (fmt == MediaSink::Format::AVIF) return avif_path;
    std::string stem = avif_path;
    if (stem.size() > 5 && stem.ends_with(".avif")) stem.resize(stem.size() - 5);
    return stem + MediaSink::FormatExtension(fmt);
}

namespace {
int QproFormatQuality(MediaSink::Format fmt) {
    switch (fmt) {
    case MediaSink::Format::WebM_VP9:
        return 32;
    case MediaSink::Format::WebP_Anim:
        return 85;
    default:
        return kQproAvifQuality;
    }
}
}

int EncodeClipFrames(const std::string& out_path, const ClipFrames& cf, const char* dump_raw_prefix,
                     MediaSink::Format format) {
    if (cf.cw <= 0 || cf.ch <= 0 || cf.frames.empty()) return 0;
    int const nframes = (int)cf.frames.size();
    MediaSink::Sink sink;
    MediaSink::Params p;
    p.output_path = out_path;
    p.format = format;
    p.src_width = cf.cw;
    p.src_height = cf.ch;
    p.fps = cf.fps;
    p.quality = QproFormatQuality(format);
    p.prefer_hardware = (format == MediaSink::Format::AVIF) && kQproAvifPreferHardware;
    if (!sink.Open(p)) {
        LOG("Qpro", "clip %s open failed (%s): %s", MediaSink::FormatToken(format),
            out_path.c_str(), sink.LastError().c_str());
        return 0;
    }
    int captured = 0;
    for (int f = 0; f < nframes; ++f) {
        if (cf.frames[f].empty()) continue;
        if (g_clip_dump_raw && (dump_raw_prefix != nullptr) &&
            (f == 0 || f == nframes / 4 || f == nframes / 2 || f == (3 * nframes) / 4)) {
            WritePngBGRA((fs::path("screenshots") /
                          (std::string(dump_raw_prefix) + "_f" + std::to_string(f) + ".png"))
                             .string(),
                         cf.frames[f].data(), cf.cw, cf.ch);
        }
        sink.SubmitFrame(cf.frames[f].data(), captured++);
    }
    if (captured > 1 && sink.Finish()) {
        LOG("Qpro", "clip %s: %d frames %dx%d -> %s", MediaSink::FormatToken(format), captured,
            cf.cw, cf.ch, out_path.c_str());
        return captured;
    }
    sink.Cancel();
    return 0;
}

int RenderClipAvif(const std::string& out_path, int fps, const char* label, int fcx, int fcy,
                   int fcw, int fch) {
    ClipFrames cf;
    if (!CaptureClipFrames(cf, fps, label, fcx, fcy, fcw, fch)) return 0;
    return EncodeClipFrames(out_path, cf, "clipraw");
}

void NormalizeClipToCanvas(ClipFrames& cf, int cw, int ch, bool hard_crop) {
    if (cw <= 0 || ch <= 0) return;
    if (!hard_crop && cf.cw >= cw && cf.ch >= ch) return;
    if (cf.cw == cw && cf.ch == ch) return;
    int const srcw = cf.cw;
    int const srch = cf.ch;
    for (auto& fr : cf.frames) {
        if (fr.empty()) continue;
        std::vector<uint8_t> canvas((size_t)cw * ch * 4, 0);
        int const rows = srch < ch ? srch : ch;
        int const rowbytes = (srcw < cw ? srcw : cw) * 4;
        for (int y = 0; y < rows; ++y)
            std::memcpy(&canvas[(size_t)y * cw * 4], &fr[(size_t)y * srcw * 4], (size_t)rowbytes);
        fr.swap(canvas);
    }
    cf.cw = cw;
    cf.ch = ch;
}

bool CaptureLayerStatic(const TexList& tl, int slot0, QproDll::Category cat, const Layer& ly,
                        ClipFrames& out) {
    out = ClipFrames{};
    const char* base = nullptr;
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (const char* cand : ly.candidates) {
        if (ReadPiece(tl, slot0, cand, px, w, h)) {
            base = cand;
            break;
        }
    }
    if (base == nullptr) return false;
    out.cw = w;
    out.ch = h;
    out.fps = 1;
    out.frames.assign(1, px);
    int fcx = 0;
    int fcy = 0;
    int fcw = 0;
    int fch = 0;
    FixedCanvasFor(cat, ly.suffix, fcx, fcy, fcw, fch);
    NormalizeClipToCanvas(out, fcw, fch, CategoryCanvasIsHardCrop(cat));
    return true;
}

namespace {

struct HandShiftGuard {
    bool on;
    explicit HandShiftGuard(bool o) : on(o) {
        if (o) AfpD3D9::SetHandRenderShift(true);
    }
    HandShiftGuard(const HandShiftGuard&) = delete;
    HandShiftGuard& operator=(const HandShiftGuard&) = delete;
    HandShiftGuard(HandShiftGuard&&) = delete;
    HandShiftGuard& operator=(HandShiftGuard&&) = delete;
    ~HandShiftGuard() {
        if (on) AfpD3D9::SetHandRenderShift(false);
    }
};

}

bool CaptureLayerViaWalk(const TexList& tl, int slot0, QproDll::Category cat, const Layer& ly,
                         bool composed, ClipFrames& out, bool& is_animated) {
    out = ClipFrames{};
    is_animated = false;

    HandShiftGuard const hand_shift(cat == QproDll::Category::Hand);

    int fcx = 0;
    int fcy = 0;
    int fcw = 0;
    int fch = 0;
    FixedCanvasFor(cat, ly.suffix, fcx, fcy, fcw, fch);

    bool const hard_crop = CategoryCanvasIsHardCrop(cat);
    bool const capture_on_canvas = hard_crop && fcw > 0 && fch > 0;

    const char* base = nullptr;
    for (const char* cand : ly.candidates) {
        if (FindImage(tl, cand) != nullptr) {
            base = cand;
            break;
        }
    }

    if (SwitchToAnimatedClip(ly.clip_candidates) > 1) {
        AfpD3D9::ResetHsvScopeRect();
        if (g_hue_scope_enabled && cat == QproDll::Category::Hand && (base != nullptr) &&
            CountWithPrefix(tl, base) > 1) {
            ScopeHueToImage(FindImage(tl, base), slot0);
            if (std::strcmp(base, "qp_hand_l") == 0)
                ScopeHueToImage2(FindImage(tl, "qp_hand_l2"), slot0);
        }
        const char* label = QproDll::Prefix(cat);
        bool const ok = capture_on_canvas ? CaptureClipFrames(out, 60, label, fcx, fcy, fcw, fch)
                                          : CaptureClipFrames(out, 60, label, 0, 0, 0, 0);
        AfpD3D9::ResetHsvScopeRect();
        if (ok) {
            if (!capture_on_canvas) NormalizeClipToCanvas(out, fcw, fch, hard_crop);
            is_animated = true;
            return true;
        }
    }

    (void)composed;
    if (CaptureLayerStatic(tl, slot0, cat, ly, out)) return true;

    int const cw = (fcw > 0 ? fcw : 2);
    int const ch = (fch > 0 ? fch : 2);
    out = ClipFrames{};
    out.cw = cw;
    out.ch = ch;
    out.fps = 1;
    out.frames.assign(1, std::vector<uint8_t>((size_t)cw * ch * 4, 0));
    is_animated = false;
    LOG("Qpro", "%s%s: no clip + no atlas -> transparent %dx%d placeholder (never skip)",
        QproDll::Prefix(cat), ly.suffix, cw, ch);
    return true;
}

bool EmitClipFrames(const ClipFrames& cf, bool anim, const std::string& path,
                    const char* dump_label) {
    if (cf.frames.empty()) return false;
    if (g_clip_dump_raw) {
        int last = -1;
        for (int f = (int)cf.frames.size() - 1; f >= 0; --f) {
            if (!cf.frames[f].empty()) {
                last = f;
                break;
            }
        }
        if (last >= 0) {
            std::string png = path;
            if (png.size() > 5 && png.ends_with(".avif")) png.replace(png.size() - 5, 5, ".png");
            WritePngBGRA(png, cf.frames[last].data(), cf.cw, cf.ch);
        }
    }
    if (anim) return EncodeClipFrames(path, cf, dump_label) > 1;
    if (cf.frames[0].empty()) return false;
    return WriteStillAvif(path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality);
}

int RenderLayerViaWalk(const TexList& tl, int slot0, QproDll::Category cat, const Layer& ly,
                       bool composed, const std::string& out_path) {
    ClipFrames cf;
    bool anim = false;
    if (!CaptureLayerViaWalk(tl, slot0, cat, ly, composed, cf, anim)) return 0;
    std::string const dump = std::string("clipraw") + ly.suffix;
    return EmitClipFrames(cf, anim, out_path, dump.c_str()) ? 1 : 0;
}

bool IsComposedPlaceholder(const TexList& tl, int slot0, QproDll::Category cat) {
    const char* front = PairFrontImage(cat);
    if (front == nullptr) return false;
    std::vector<uint8_t> fpx;
    int fw = 0;
    int fh = 0;
    if (!ReadPiece(tl, slot0, front, fpx, fw, fh) || fpx.empty()) return false;
    for (size_t p = 3; p < fpx.size(); p += 4)
        if (fpx[p] > 8) return false;
    return true;
}

int RenderPairViaWalk(const TexList& tl, int slot0, QproDll::Category cat, bool composed,
                      const std::function<std::string(const char*)>& out_for) {
    const std::vector<Layer>& layers = LayersFor(cat);
    const Layer* lb = nullptr;
    const Layer* lf = nullptr;
    for (const Layer& ly : layers) {
        if (std::strcmp(ly.suffix, "_b") == 0) {
            lb = &ly;
        } else if (std::strcmp(ly.suffix, "_f") == 0) {
            lf = &ly;
        }
    }
    if ((lb == nullptr) || (lf == nullptr)) return 0;

    ClipFrames front;
    bool f_anim = false;
    bool const have_front = CaptureLayerViaWalk(tl, slot0, cat, *lf, composed, front, f_anim);

    ClipFrames back;
    bool b_anim = false;
    bool const have_back = CaptureLayerViaWalk(tl, slot0, cat, *lb, composed, back, b_anim);

    int written = 0;
    if (have_back && EmitClipFrames(back, b_anim, out_for("_b"), "clipraw_b")) ++written;
    if (have_front && EmitClipFrames(front, f_anim, out_for("_f"), "clipraw_f")) ++written;
    return written;
}

}
