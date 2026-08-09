#include "qpro/qpro_internal.h"
#include "engine_session.h"

#include "avs_funcs.h"
#include "avs_xml.h"
#include "formats/bgra_crop.h"
#include "mc_control.h"
#include "media/media_format.h"
#include "media_sink.h"
#include "qpro/qpro_dll.h"
#include "qpro/qpro_extract.h"
#include "render_backend.h"
#include "render_seh.h"
#include "qpro/qpro_status.h"
#include "qpro/qpro_walk.h"
#include "support/com_ptr.h"
#include "support/env.h"
#include "support/log.h"

#include <cstdint>
#include <span>
#include <cstring>
#include <d3d9.h>
#include <filesystem>
#include <ocidl.h>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include <wincodec.h>

namespace QproExtract::detail {

namespace fs = std::filesystem;

float ProgressFrac() {
    const int total = TotalCount();
    return total > 0 ? (float)DoneCount() / (float)total : -1.0F;
}

bool ParseTexturelist(EngineSession& es, TexList& out, const char* root) {
    std::string const path = std::string(root) + "/tex/texturelist.xml";
    auto tree = AvsXml::LoadFromFile(es.avs, path.c_str());
    if (!tree) return false;
    int atlas = 0;
    T_PROPERTY_NODE* tex = AvsXml::FindFirst(es.avs, tree, "texturelist/texture");
    while (tex != nullptr) {
        T_PROPERTY_NODE* img = es.avs.property_search(nullptr, tex, "image");
        while (img != nullptr) {
            char name[160] = {};
            uint16_t r[4] = {};
            if (AvsXml::ReadStrAttr(es.avs, img, "name", name, sizeof(name)) &&
                AvsXml::ReadChild4U16(es.avs, img, "imgrect", r)) {
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
            img = AvsXml::NextMatch(es.avs, img);
        }
        atlas++;
        tex = AvsXml::NextMatch(es.avs, tex);
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

struct WicPngTarget {
    ComInit com;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> enc;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
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

std::string AvifPathToPng(const std::string& avif_path) {
    std::string png = avif_path;
    if (png.size() > 5 && png.ends_with(".avif")) png.replace(png.size() - 5, 5, ".png");
    return png;
}

int WriteStillFrameWithDump(const std::string& out_path, const ClipFrames& cf) {
    if (cf.frames.empty() || cf.frames[0].empty()) return 0;
    int n = 0;
    if (WriteStillAvif(out_path, cf.frames[0].data(), cf.cw, cf.ch, kQproAvifQuality)) n = 1;
    if (g_clip_dump_raw) {
        WritePngBGRA(AvifPathToPng(out_path), cf.frames[0].data(), cf.cw, cf.ch);
    }
    return n;
}

std::string IfsPath(const std::string& game_dir, const std::string& ifs) {
    return (fs::path(game_dir) / "data" / "graphic" / ifs).string();
}

std::string Stem(const std::string& ifs) {
    std::string s = fs::path(ifs).filename().string();
    if (s.size() > 4 && s.ends_with(".ifs")) s.resize(s.size() - 4);
    return s;
}

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

bool RenderFrame(EngineSession& es, D3D9State& d3d, std::vector<uint8_t>& out, int& w, int& h,
                 bool advance) {
    if (d3d.device == nullptr) return false;
    if (advance && (es.afp.afp_do_update != nullptr))
        RenderSeh::SafeCallUpdate(es.afp.afp_do_update, 1.0F / 120.0F);
    d3d.BeginFrame();
    if (es.afp.afp_do_sort_render != nullptr)
        RenderSeh::SafeCallSortRender(es.afp.afp_do_sort_render);
    bool const ok = d3d.ReadOffscreenBGRA(out, w, h);
    d3d.EndFrame();
    return ok;
}

void WarmUpFrames(EngineSession& es, D3D9State& d3d, std::vector<uint8_t>& out, int& w, int& h,
                  int frames) {
    for (int i = 0; i < frames; ++i)
        RenderFrame(es, d3d, out, w, h, true);
}

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

int QproLimit() {
    return Support::EnvInt("QPRO_LIMIT").value_or(0);
}

}

namespace QproExtract {

ClipAttach AttachClipStream(EngineSession& es, uint32_t pkg, uint32_t sid, const char* layer,
                            const char* clip_name, const char* log_tag) {
    ClipAttach r;
    if (es.afp.afp_mc_get_id_by_path != nullptr)
        r.head_mc = es.afp.afp_mc_get_id_by_path(sid, layer);

    auto get_afp_info = es.afpu.afpu_afp_get_info_in_package;
    if ((get_afp_info == nullptr) || (es.afp.afp_mc_attach_stream == nullptr) ||
        (es.afp.afp_mc_get_relative_id == nullptr) || (es.afp.afp_mc_get_id_by_path == nullptr)) {
        if (log_tag != nullptr) LOG(log_tag, "mount '%s': missing afp fn(s)", layer);
        return r;
    }

    uint64_t info[8] = {};
    int gi = (clip_name != nullptr) ? get_afp_info(info, pkg, clip_name) : -1;
    if (gi < 0 && (clip_name != nullptr) && std::strcmp(clip_name, layer) != 0)
        gi = get_afp_info(info, pkg, layer);
    r.have_stream = gi >= 0;
    r.data_id = (uint32_t)info[3];
    if (log_tag != nullptr) {
        LOG(log_tag, "layer '%s' mc=0x%x get_info=%d data_id=0x%x", layer, r.head_mc, gi,
            r.data_id);
    }
    if (!r.have_stream) return r;

    int mc = r.head_mc;
    for (int c = 0; mc >= 0 && c < 64; mc = es.afp.afp_mc_get_relative_id(mc, 6), ++c) {
        es.afp.afp_mc_attach_stream(mc, r.data_id);
        if (es.afp.afp_mc_get != nullptr) es.afp.afp_mc_get(mc, 0x101E, 1);
        ++r.attached;
    }
    if (log_tag != nullptr) {
        LOG(log_tag, "mounted item clip '%s' onto %d layer mc(s)", layer, r.attached);
    }
    return r;
}

void HideAllLayersExcept(EngineSession& es, uint32_t sid, std::span<const char* const> keep) {
    for (const char* a : kAllAvatarLayers) {
        bool kept = false;
        for (const char* k : keep) {
            if (std::strcmp(a, k) == 0) {
                kept = true;
                break;
            }
        }
        if (!kept) McControl::SetClipVisible(es.afp, sid, a, false);
    }
}

}
