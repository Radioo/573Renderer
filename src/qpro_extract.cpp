#include <cstdio>
#include "avs_funcs.h"
#include <utility>
#include <intsafe.h>
#include <ocidl.h>
#include "media/media_format.h"
#include <d3d9.h>
#include "formats/bgra_crop.h"
#include "qpro_scan.h"
#include <system_error>
#include <ios>
#include "qpro_extract.h"

#include "qpro_dll.h"
#include "boot.h"
#include "afp_boot.h"
#include "mc_control.h"
#include "render_seh.h"
#include "app_globals.h"
#include "state/app_state.h"
#include "avs_xml.h"
#include "media_sink.h"
#include "render_backend.h"
#include "support/env.h"
#include "support/log.h"
#include "qpro_internal.h"
#include "qpro_walk.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <windows.h>
#include <wincodec.h>

namespace QproExtract {
namespace {
std::mutex g_mu;
Status g_status;
int g_total = 0;
int g_done = 0;

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
}
}

namespace QproExtract {
namespace detail {

namespace fs = std::filesystem;

float ProgressFrac() {
    return g_total > 0 ? (float)g_done / (float)g_total : -1.0F;
}

bool ParseTexturelist(TexList& out, const char* root) {
    std::string const path = std::string(root) + "/tex/texturelist.xml";
    auto tree = AvsXml::LoadFromFile(g_avs, path.c_str());
    if (!tree) return false;
    int atlas = 0;
    T_PROPERTY_NODE* tex = AvsXml::FindFirst(g_avs, tree, "texturelist/texture");
    while (tex != nullptr) {
        T_PROPERTY_NODE* img = g_avs.property_search(nullptr, tex, "image");
        while (img != nullptr) {
            char name[160] = {};
            uint16_t r[4] = {};
            if (AvsXml::ReadStrAttr(g_avs, img, "name", name, sizeof(name)) &&
                AvsXml::ReadChild4U16(g_avs, img, "imgrect", r)) {
                AtlasImage ai;
                ai.name = name;
                ai.atlas = atlas;
                ai.raw[0] = r[0];
                ai.raw[1] = r[1];
                ai.raw[2] = r[2];
                ai.raw[3] = r[3];
                ai.x = r[0] / 2;
                ai.y = r[2] / 2;
                ai.w = (r[1] - r[0]) / 2;
                ai.h = (r[3] - r[2]) / 2;
                out.images.push_back(std::move(ai));
            }
            img = AvsXml::NextMatch(g_avs, img);
        }
        atlas++;
        tex = AvsXml::NextMatch(g_avs, tex);
    }
    out.atlas_count = atlas;
    return true;
}

const AtlasImage* FindImage(const TexList& tl, const char* name) {
    for (const AtlasImage& a : tl.images)
        if (a.name == name) return &a;
    return nullptr;
}

bool ScopeHueToImage(const AtlasImage* im, int slot0) {
    if (im == nullptr) return false;
    int aw = 0;
    int ah = 0;
    if (!AfpD3D9::GetTextureSize(slot0 + im->atlas, aw, ah) || aw <= 0 || ah <= 0) return false;
    AfpD3D9::SetHsvScopeRect((float)im->x / (float)aw, (float)(im->x + im->w) / (float)aw,
                             (float)im->y / (float)ah, (float)(im->y + im->h) / (float)ah);
    return true;
}

bool ScopeHueToImage2(const AtlasImage* im, int slot0) {
    if (im == nullptr) return false;
    int aw = 0;
    int ah = 0;
    if (!AfpD3D9::GetTextureSize(slot0 + im->atlas, aw, ah) || aw <= 0 || ah <= 0) return false;
    AfpD3D9::SetHsvScopeRect2((float)im->x / (float)aw, (float)(im->x + im->w) / (float)aw,
                              (float)im->y / (float)ah, (float)(im->y + im->h) / (float)ah);
    return true;
}

int CountWithPrefix(const TexList& tl, const char* prefix) {
    size_t const n = std::strlen(prefix);
    int count = 0;
    for (const AtlasImage& a : tl.images)
        if (a.name.size() >= n && a.name.compare(0, n, prefix) == 0) ++count;
    return count;
}

namespace {
std::wstring Widen(const std::string& s) {
    int const n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w;
    if (n > 0) {
        w.resize(n - 1);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    }
    return w;
}
}

namespace {

struct WicPngTarget {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IWICStream* stream = nullptr;
    IPropertyBag2* props = nullptr;
    bool com_inited = false;

    WicPngTarget() = default;
    WicPngTarget(const WicPngTarget&) = delete;
    WicPngTarget& operator=(const WicPngTarget&) = delete;
    WicPngTarget(WicPngTarget&&) = delete;
    WicPngTarget& operator=(WicPngTarget&&) = delete;
    ~WicPngTarget() {
        if (props != nullptr) props->Release();
        if (frame != nullptr) frame->Release();
        if (enc != nullptr) enc->Release();
        if (stream != nullptr) stream->Release();
        if (factory != nullptr) factory->Release();
        if (com_inited) CoUninitialize();
    }
};

bool OpenPngFrame(WicPngTarget& t, const std::wstring& wpath) {
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&t.factory)))) {
        return false;
    }
    if (FAILED(t.factory->CreateStream(&t.stream))) return false;
    if (FAILED(t.stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE))) return false;
    if (FAILED(t.factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &t.enc))) return false;
    if (FAILED(t.enc->Initialize(t.stream, WICBitmapEncoderNoCache))) return false;
    if (FAILED(t.enc->CreateNewFrame(&t.frame, &t.props))) return false;
    return !FAILED(t.frame->Initialize(t.props));
}

bool EncodePngPixels(WicPngTarget& t, const uint8_t* bgra, int w, int h) {
    if (FAILED(t.frame->SetSize((UINT)w, (UINT)h))) return false;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(t.frame->SetPixelFormat(&fmt))) return false;
    UINT const stride = (UINT)w * 4;
    UINT const bufsize = stride * (UINT)h;
    if (FAILED(t.frame->WritePixels((UINT)h, stride, bufsize, (BYTE*)bgra))) return false;
    if (FAILED(t.frame->Commit())) return false;
    return !FAILED(t.enc->Commit());
}

}

bool WritePngBGRA(const std::string& path, const uint8_t* bgra, int w, int h) {
    std::wstring const wpath = Widen(path);
    WicPngTarget t;
    t.com_inited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    if (!OpenPngFrame(t, wpath)) return false;
    return EncodePngPixels(t, bgra, w, h);
}

bool g_hue_scope_enabled = true;

bool g_clip_dump_raw = false;

bool WriteStillAvif(const std::string& path, const uint8_t* bgra, int w, int h, int quality) {
    if (w <= 0 || h <= 0) return false;
    MediaSink::Sink sink;
    MediaSink::Params p;
    p.output_path = path;
    p.format = MediaSink::Format::AVIF;
    p.src_width = w;
    p.src_height = h;
    p.fps = 1;
    p.quality = quality;
    p.prefer_hardware = kQproAvifPreferHardware;
    if (!sink.Open(p)) {
        LOG("Qpro", "still AVIF open failed (%s): %s", path.c_str(), sink.LastError().c_str());
        return false;
    }
    if (!sink.SubmitFrame(bgra, 0) || !sink.Finish()) {
        sink.Cancel();
        return false;
    }
    return true;
}

std::string IfsPath(const std::string& game_dir, const std::string& ifs) {
    return (fs::path(game_dir) / "data" / "graphic" / ifs).string();
}

std::string Stem(const std::string& ifs) {
    std::string s = fs::path(ifs).filename().string();
    if (s.size() > 4 && s.ends_with(".ifs")) s.resize(s.size() - 4);
    return s;
}

namespace {
const char* kHideForBody[] = {
    "qpro_bg",           "qp_cat_1",          "qp_cat_2",          "qp_cat_3",
    "qp_head_b_neutral", "qp_hair_b",         "qp_hand_r_neutral", "qp_face_neutral",
    "qp_hair_f",         "qp_head_f_neutral", "qp_hand_l_neutral",
};
}

namespace {
void HideNonBody(uint32_t sid) {
    for (const char* c : kHideForBody)
        McControl::SetClipVisible(g_afp, sid, c, false);
}
}

namespace {
const char* kBodyClips[] = {
    "qp_body_f",      "qp_body_b",      "qp_arm_r_upper", "qp_arm_r_lower", "qp_arm_l_upper",
    "qp_arm_l_lower", "qp_leg_r_upper", "qp_leg_r_lower", "qp_leg_l_upper", "qp_leg_l_lower",
};
}

constexpr int kBodyCanvasW = 520, kBodyCanvasH = 704;

bool ReadPiece(const TexList& tl, int slot0, const char* name, std::vector<uint8_t>& out, int& w,
               int& h) {
    const AtlasImage* im = FindImage(tl, name);
    if (im == nullptr) return false;
    IDirect3DTexture9* tex = AfpD3D9::GetTexture(slot0 + im->atlas);
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

namespace {
int SwapBodyPieces(uint32_t comp_pkg, uint32_t sid) {
    int n = 0;
    for (const char* c : kBodyClips) {
        if (AfpManager::SwapClipBitmapFromCompanion(g_engine, comp_pkg, sid, c) > 0) ++n;
    }
    return n;
}
}

bool RenderFrame(std::vector<uint8_t>& out, int& w, int& h, bool advance) {
    if (g_d3d.device == nullptr) return false;
    if (advance && (g_afp.afp_do_update != nullptr))
        RenderSeh::SafeCallUpdate(g_afp.afp_do_update, 1.0F / 120.0F);
    g_d3d.BeginFrame();
    if (g_afp.afp_do_sort_render != nullptr)
        RenderSeh::SafeCallSortRender(g_afp.afp_do_sort_render);
    bool const ok = g_d3d.ReadOffscreenBGRA(out, w, h);
    g_d3d.EndFrame();
    return ok;
}

namespace {
std::vector<LayerJob> CompositeJobs(QproDll::Category cat, const std::string& prefix) {
    auto o = [&](const char* suf) { return prefix + suf + ".avif"; };
    using C = QproDll::Category;
    switch (cat) {
    case C::Head:
        return {{.layer = "qp_head_f_neutral",
                 .item_clip = "qp_head_f_neutral",
                 .atlas = "qp_head_f",
                 .hue_eff = nullptr,
                 .out_path = o("_f")},
                {.layer = "qp_head_b_neutral",
                 .item_clip = "qp_head_b_neutral",
                 .atlas = "qp_head_b",
                 .hue_eff = nullptr,
                 .out_path = o("_b")}};
    case C::Hair:
        return {{.layer = "qp_hair_f",
                 .item_clip = "qp_hair_f_neutral",
                 .atlas = "qp_hair_f",
                 .hue_eff = nullptr,
                 .out_path = o("_f")},
                {.layer = "qp_hair_b",
                 .item_clip = "qp_hair_b_neutral",
                 .atlas = "qp_hair_b",
                 .hue_eff = nullptr,
                 .out_path = o("_b")}};
    case C::Hand:
        return {{.layer = "qp_hand_l_neutral",
                 .item_clip = "qp_hand_l_neutral",
                 .atlas = "qp_hand_l",
                 .hue_eff = "qp_hand_l",
                 .out_path = o("_l")},
                {.layer = "qp_hand_r_neutral",
                 .item_clip = "qp_hand_r_neutral",
                 .atlas = "qp_hand_r",
                 .hue_eff = "qp_hand_r",
                 .out_path = o("_r")}};
    case C::Face:
        return {{.layer = "qp_face_neutral",
                 .item_clip = "qp_face_neutral",
                 .atlas = "qp_face",
                 .hue_eff = nullptr,
                 .out_path = o("")}};
    case C::Back:
        return {{.layer = "qpro_bg",
                 .item_clip = "qp_bg",
                 .atlas = "qp_bg",
                 .hue_eff = nullptr,
                 .out_path = o("")}};
    default:
        return {};
    }
}
}

namespace {
int QproLimit() {
    return Support::EnvInt("QPRO_LIMIT").value_or(0);
}
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
}

}

using namespace detail;

void SetHueScopeEnabled(bool on) {
    g_hue_scope_enabled = on;
}

void BodyOne(const std::string& game_dir, const std::string& body_ifs) {
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    std::string const body = IfsPath(game_dir, body_ifs);

    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    if (!MountAndLoadIfs(main2)) {
        LOG("QproBody", "mount qp_main2 FAILED");
        return;
    }

    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, body, Stem(body_ifs));
    LOG("QproBody", "LoadCompanion(%s) pkg=0x%x", body_ifs.c_str(), pkg);

    if (!AfpManager::SwitchAnimation(g_engine, "qp_motion", true))
        LOG("QproBody", "SwitchAnimation(qp_motion) FAILED");
    uint32_t const sid = AfpManager::StreamId();

    int const swapped = SwapBodyPieces(pkg, sid);
    LOG("QproBody", "re-pointed %d/%d body pieces", swapped,
        (int)(sizeof(kBodyClips) / sizeof(kBodyClips[0])));
    HideNonBody(sid);

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 2; ++i) {
        RenderFrame(px, w, h, true);
        HideNonBody(sid);
    }
    RenderFrame(px, w, h, false);

    LOG("QproBody", "rendered %dx%d (sid=0x%x)", w, h, sid);
    if (w > 0 && h > 0)
        WritePngBGRA((fs::path("screenshots") / "qpbodyone.png").string(), px.data(), w, h);

    if (pkg != 0U) AfpManager::UnloadCompanion(g_engine, pkg);
}

void BackOne(const std::string& game_dir, const std::string& back_ifs) {
    std::string const path = IfsPath(game_dir, back_ifs);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    int const slot0 = AfpD3D9::NextSlot();
    if (MountAndLoadIfs(path)) {
        TexList tl;
        ParseTexturelist(tl);
        const Layer& ly = LayersFor(QproDll::Category::Back).front();
        RenderLayerViaWalk(tl, slot0, QproDll::Category::Back, ly, false,
                           (fs::path("screenshots") / "qpback_old.avif").string());
        AfpManager::UnloadPackages(g_engine);
    }
    g_clip_dump_raw = true;
    int const n = RenderBackRealtime(game_dir, back_ifs, 60,
                                     (fs::path("screenshots") / "qpback.avif").string());
    g_clip_dump_raw = false;
    LOG("QproBack", "%s -> RenderBackRealtime wrote %d frames", back_ifs.c_str(), n);

    int const cn = RenderBackComposite(
        game_dir, back_ifs, 60, (fs::path("screenshots") / "qpback_composite.avif").string());
    LOG("QproBack", "%s -> composite real-time wrote %d file(s)", back_ifs.c_str(), cn);
}

void HeadOne(const std::string& game_dir, const std::string& head_ifs) {
    g_clip_dump_raw = true;
    int const n = RenderItemComposite(
        game_dir, head_ifs,
        CompositeJobs(QproDll::Category::Head, (fs::path("screenshots") / "qphead").string()));
    g_clip_dump_raw = false;
    LOG("QproHead", "%s -> composite wrote %d/2 layers", head_ifs.c_str(), n);
    AfpManager::UnloadPackages(g_engine);
}

void HandOne(const std::string& game_dir, const std::string& hand_ifs) {
    g_clip_dump_raw = true;
    int const n = RenderItemComposite(
        game_dir, hand_ifs,
        CompositeJobs(QproDll::Category::Hand, (fs::path("screenshots") / "qphand").string()));
    g_clip_dump_raw = false;
    LOG("QproHand", "%s -> composite wrote %d/2 layers", hand_ifs.c_str(), n);
    AfpManager::UnloadPackages(g_engine);
}

void HairOne(const std::string& game_dir, const std::string& hair_ifs) {
    g_clip_dump_raw = true;
    int const n = RenderItemComposite(
        game_dir, hair_ifs,
        CompositeJobs(QproDll::Category::Hair, (fs::path("screenshots") / "qphair").string()));
    g_clip_dump_raw = false;
    LOG("QproHair", "%s -> composite wrote %d/2 layers", hair_ifs.c_str(), n);
    AfpManager::UnloadPackages(g_engine);
}

void FaceOne(const std::string& game_dir, const std::string& face_ifs) {
    g_clip_dump_raw = true;
    int const n = RenderItemComposite(
        game_dir, face_ifs,
        CompositeJobs(QproDll::Category::Face, (fs::path("screenshots") / "qpface").string()));
    LOG("QproFace", "%s -> composite wrote %d layer(s)", face_ifs.c_str(), n);
    g_clip_dump_raw = false;
    AfpManager::UnloadPackages(g_engine);
}

namespace {
bool BodyCanvasOk(size_t count, Result& res) {
    int ow = 0;
    int oh = 0;
    g_d3d.GetOffscreenSize(ow, oh);
    if (ow == kBodyCanvasW && oh == kBodyCanvasH) return true;
    LOG("Qpro", "BODY needs render size %dx%d (got %dx%d); skipping %zu bodies", kBodyCanvasW,
        kBodyCanvasH, ow, oh, count);
    res.failed += (int)count;
    std::scoped_lock const lk(g_mu);
    g_status.issues.push_back({.text = "all body parts -- render must be 520x704 "
                                       "(Setup > \"qpro avatar\" preset, then re-Load)",
                               .failure = true});
    return false;
}

bool MountBodyMain(const std::string& game_dir, size_t count, Result& res) {
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    std::string const main2 = IfsPath(game_dir, "qp_main2.ifs");
    if (!MountAndLoadIfs(main2)) {
        LOG("Qpro", "BODY: qp_main2 mount FAILED; skipping %zu bodies", count);
        res.failed += (int)count;
        std::scoped_lock const lk(g_mu);
        g_status.issues.push_back(
            {.text = "all body parts -- qp_main2.ifs failed to mount", .failure = true});
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

void ExtractOneBody(const BodyPassCtx& ctx, const QproDll::Part& part, size_t idx, Result& res) {
    std::error_code ec;
    std::string const path = IfsPath(*ctx.game_dir, part.ifs);
    std::string const lbl = *ctx.prefix + "_" + std::to_string(idx);
    if (!fs::exists(path, ec)) {
        Note(res, false, lbl, part.ifs, "no source .ifs on disk");
        return;
    }

    AfpD3D9::SetNextSlot(ctx.comp_base);
    uint32_t const pkg = AfpManager::LoadCompanion(g_engine, path, Stem(part.ifs));
    if (pkg == 0U) {
        Note(res, true, lbl, part.ifs, "LoadCompanion failed");
        return;
    }
    AfpManager::SwitchAnimation(g_engine, "qp_motion", true);
    uint32_t const sid = AfpManager::StreamId();
    int const nsw = SwapBodyPieces(pkg, sid);
    if (idx == 0 && nsw == 0) {
        LOG("Qpro", "BODY WARNING: 0 pieces re-pointed - bodies will be blank");
    }
    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int s = 0; s < 2; ++s) {
        RenderFrame(px, w, h, true);
        HideNonBody(sid);
    }
    AfpManager::SeekFrame(g_afp, 0);
    HideNonBody(sid);
    RenderFrame(px, w, h, false);

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

    AfpManager::UnloadCompanion(g_engine, pkg);
    int const cur = AfpD3D9::NextSlot();
    for (int s = ctx.comp_base; s < cur; ++s)
        AfpD3D9::TexDestroy((unsigned)s);
    AfpD3D9::SetNextSlot(ctx.comp_base);
}

void RunBodyPass(const QproDll::Parts& parts, const std::string& game_dir,
                 const std::string& out_dir, Result& res, const PartSelection& part_sel) {
    const std::vector<QproDll::Part>& list = parts.of(QproDll::Category::Body);
    const std::string prefix = QproDll::Prefix(QproDll::Category::Body);

    if (!BodyCanvasOk(list.size(), res)) return;
    if (!MountBodyMain(game_dir, list.size(), res)) return;
    AfpManager::SwitchAnimation(g_engine, "qp_motion", true);
    uint32_t const sid = AfpManager::StreamId();

    std::vector<uint8_t> px;
    int w = 0;
    int h = 0;
    for (int i = 0; i < 2; ++i)
        RenderFrame(px, w, h, true);
    HideNonBody(sid);

    BodyPassCtx ctx;
    ctx.game_dir = &game_dir;
    ctx.out_dir = &out_dir;
    ctx.prefix = &prefix;
    ctx.comp_base = AfpD3D9::NextSlot();
    for (size_t idx = 0; idx < list.size(); ++idx) {
        if (QproLimit() > 0 && (int)idx >= QproLimit()) break;
        if (!part_sel.selected(QproDll::Category::Body, idx)) continue;
        ++g_done;
        if ((idx % 20) == 0) {
            LOG("Qpro", "[body %zu/%zu] %s", idx, list.size(), list[idx].ifs.c_str());
            PublishProgress("assembling bodies");
        }
        ExtractOneBody(ctx, list[idx], idx, res);
    }

    AfpManager::UnloadAllCompanions(g_engine);
    AfpManager::UnloadPackages(g_engine);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
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

std::vector<int> MeasureBackNativeFps(const Options& opt, const QproDll::Parts& parts,
                                      int q_limit) {
    std::vector<int> back_native_fps;
    if (!opt.parts.back) return back_native_fps;
    std::error_code ec;
    const std::vector<QproDll::Part>& blist = parts.of(QproDll::Category::Back);
    back_native_fps.assign(blist.size(), opt.fps);
    for (size_t bi = 0; bi < blist.size(); ++bi) {
        if (q_limit > 0 && std::cmp_greater_equal(bi, q_limit)) break;
        if (!opt.part_sel.selected(QproDll::Category::Back, bi)) continue;
        if (fs::exists(IfsPath(opt.game_dir, blist[bi].ifs), ec))
            back_native_fps[bi] = ProbeBackNativeFps(opt.game_dir, blist[bi].ifs, opt.fps);
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

CatMount MountCategorySweep(const Options& opt) {
    CatMount m;
    const bool need_cat_mount =
        opt.parts.hand || opt.parts.face || opt.parts.hair || opt.parts.head || opt.parts.back;
    if (!need_cat_mount) return m;
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    const std::string cat_main2 = IfsPath(opt.game_dir, "qp_main2.ifs");
    m.main_base = AfpD3D9::NextSlot();
    if (MountAndLoadIfs(cat_main2)) {
        TexList mtl;
        ParseTexturelist(mtl, "/afp/packages");
        (void)mtl;
        m.comp_base = AfpD3D9::NextSlot();
        AfpManager::SwitchAnimation(g_engine, "qp_motion", true);
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

void SweepOneItem(SweepCtx& ctx, QproDll::Category cat, const std::string& prefix,
                  const QproDll::Part& part, size_t idx) {
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
    if (cat == QproDll::Category::Back) {
        std::string const bout = jobs.empty() ? (oprefix + ".avif") : jobs[0].out_path;
        int const nf = (idx < ctx.back_native_fps->size()) ? (*ctx.back_native_fps)[idx] : opt.fps;
        res.images += RenderBackComposite(
            opt.game_dir, ifs, opt.fps, bout, nf,
            {.sid = ctx.cat_sid, .comp_base = ctx.cat_comp_base, .main_base = ctx.cat_main_base},
            !sel, ctx.video_stems);
    } else {
        res.images += RenderItemComposite(
            opt.game_dir, ifs, jobs,
            {.sid = ctx.cat_sid, .comp_base = ctx.cat_comp_base, .main_base = ctx.cat_main_base},
            opt.fps, !sel, ctx.video_stems);
    }
    if (!sel) return;

    for (const LayerJob& j : jobs) {
        if (!fs::exists(j.out_path, ec)) {
            std::vector<uint8_t> blank((size_t)520 * 704 * 4, 0);
            if (WriteStillAvif(j.out_path, blank.data(), 520, 704, kQproAvifQuality)) res.images++;
            Note(res, true, lbl, ifs, "layer missing after composite -> transparent placeholder");
        }
    }
}

void RunCategorySweep(SweepCtx& ctx, const QproDll::Parts& parts) {
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
            SweepOneItem(ctx, cat, prefix, list[idx], idx);
        }
    }
}

void WritePartsJson(const QproDll::Parts& parts, const std::string& out_root, Result& res) {
    std::string const json = QproDll::ToJson(parts);
    res.json_path = (fs::path(out_root) / "2dx_qpro.json").string();
    std::ofstream jf(res.json_path, std::ios::binary | std::ios::trunc);
    if (jf) jf.write(json.data(), (std::streamsize)json.size());
}

void UnmountSweepPackages() {
    AfpManager::UnloadPackages(g_engine);
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
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

Result Run(const Options& opt) {
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

    if (opt.parts.body) RunBodyPass(parts, opt.game_dir, out_root, res, opt.part_sel);
    AfpD3D9::SetNextSlot(base);

    const int q_limit = QproLimit();
    const std::vector<int> back_native_fps = MeasureBackNativeFps(opt, parts, q_limit);
    const CatMount cm = MountCategorySweep(opt);

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
    RunCategorySweep(ctx, parts);
    UnmountSweepPackages();

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
