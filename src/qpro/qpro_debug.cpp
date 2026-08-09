#include "qpro/qpro_internal.h"
#include "engine_session.h"

#include "afp_boot.h"
#include "boot.h"
#include "qpro/qpro_dll.h"
#include "qpro/qpro_extract.h"
#include "qpro/qpro_walk.h"
#include "render_backend.h"
#include "support/log.h"

#include <filesystem>
#include <string>

namespace QproExtract {

namespace fs = std::filesystem;
using namespace detail;

void SetHueScopeEnabled(bool on) {
    g_hue_scope_enabled = on;
}

void BackOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& back_ifs) {
    std::string const path = IfsPath(game_dir, back_ifs);
    AfpManager::UmountPackagesAndData(es.avs);
    int const slot0 = AfpD3D9::NextSlot();
    if (MountAndLoadIfs(path)) {
        TexList tl;
        ParseTexturelist(es, tl);
        const Layer& ly = LayersFor(QproDll::Category::Back).front();
        RenderLayerViaWalk(es, d3d, tl, slot0, QproDll::Category::Back, ly, false,
                           (fs::path("screenshots") / "qpback_old.avif").string());
        AfpManager::UnloadPackages(es);
    }
    g_clip_dump_raw = true;
    int const n = RenderBackRealtime(es, d3d, game_dir, back_ifs, 60,
                                     (fs::path("screenshots") / "qpback.avif").string());
    g_clip_dump_raw = false;
    LOG("QproBack", "%s -> RenderBackRealtime wrote %d frames", back_ifs.c_str(), n);

    int const cn =
        RenderBackComposite(es, d3d, game_dir, back_ifs, 60,
                            (fs::path("screenshots") / "qpback_composite.avif").string());
    LOG("QproBack", "%s -> composite real-time wrote %d file(s)", back_ifs.c_str(), cn);
}

namespace {

void CategoryOne(EngineSession& es, D3D9State& d3d, QproDll::Category cat,
                 const std::string& game_dir, const std::string& ifs, const char* out_stem,
                 const char* log_tag) {
    g_clip_dump_raw = true;
    int const n = RenderItemComposite(
        es, d3d, game_dir, ifs, CompositeJobs(cat, (fs::path("screenshots") / out_stem).string()));
    g_clip_dump_raw = false;
    LOG(log_tag, "%s -> composite wrote %d/2 layers", ifs.c_str(), n);
    AfpManager::UnloadPackages(es);
}

}

void HeadOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& head_ifs) {
    CategoryOne(es, d3d, QproDll::Category::Head, game_dir, head_ifs, "qphead", "QproHead");
}

void HandOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& hand_ifs) {
    CategoryOne(es, d3d, QproDll::Category::Hand, game_dir, hand_ifs, "qphand", "QproHand");
}

void HairOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& hair_ifs) {
    CategoryOne(es, d3d, QproDll::Category::Hair, game_dir, hair_ifs, "qphair", "QproHair");
}

void FaceOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& face_ifs) {
    CategoryOne(es, d3d, QproDll::Category::Face, game_dir, face_ifs, "qpface", "QproFace");
}

}
