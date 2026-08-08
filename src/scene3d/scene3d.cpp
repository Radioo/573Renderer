#include "scene3d/scene3d.h"

#include "formats/gcz.h"
#include "formats/inz.h"
#include "formats/lzss.h"
#include "formats/xfile.h"
#include "scene3d/anim.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Scene3d {

namespace {

constexpr int kTileSize = 256;
constexpr size_t kMaxChunkVerts = 65000;

std::vector<uint8_t> ReadFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

bool Inflate(const std::filesystem::path& p, std::vector<uint8_t>& out, std::string& err) {
    const std::vector<uint8_t> raw = ReadFile(p);
    if (raw.empty()) {
        err = "could not read " + p.filename().string();
        return false;
    }
    return Lzss::Decompress(raw, out, err);
}

bool LoadTiles(const std::filesystem::path& dir, Scene& out, std::string& err) {
    out.tiles.clear();
    out.tiles.reserve(out.manifest.slices.size());
    for (const auto& slice : out.manifest.slices) {
        std::vector<uint8_t> raw;
        if (!Inflate(dir / (slice.name + ".gcz"), raw, err)) return false;
        Gcz::Tile parsed;
        if (!Gcz::Parse(raw, parsed, err)) return false;
        Tile tile;
        tile.name = slice.name;
        tile.width = parsed.width;
        tile.height = parsed.height;
        Gcz::ExpandToBgra(parsed, tile.bgra);
        out.tiles.push_back(std::move(tile));
    }
    return true;
}

int TileForTexture(const Scene& scene, const std::string& texture) {
    const Inz::Pattern* p = Inz::FindPattern(scene.manifest, texture);
    if (p == nullptr) return -1;
    const int index = p->y / kTileSize;
    if (index < 0 || (size_t)index >= scene.tiles.size()) return -1;
    return index;
}

void AppendTriangle(const XFile::Mesh& mesh, size_t tri, DrawChunk& chunk,
                    std::map<uint32_t, uint16_t>& remap) {
    for (size_t k = 0; k < 3; k++) {
        const uint32_t src = mesh.indices[(tri * 3) + k];
        auto it = remap.find(src);
        if (it == remap.end()) {
            const auto slot = (uint16_t)(chunk.vertices.size() / kVertexFloats);
            const auto& p = mesh.positions[src];
            chunk.vertices.push_back(p.x);
            chunk.vertices.push_back(p.y);
            chunk.vertices.push_back(p.z);
            if (src < mesh.uvs.size()) {
                chunk.vertices.push_back(mesh.uvs[src].u);
                chunk.vertices.push_back(mesh.uvs[src].v);
            } else {
                chunk.vertices.push_back(0.0F);
                chunk.vertices.push_back(0.0F);
            }
            it = remap.emplace(src, slot).first;
        }
        chunk.indices.push_back(it->second);
    }
}

int TileForTriangle(const Scene& scene, const XFile::Mesh& mesh, size_t tri) {
    const uint32_t mat = (tri < mesh.face_material.size()) ? mesh.face_material[tri] : 0;
    if (mat >= mesh.materials.size()) return -1;
    const std::string& tex = mesh.materials[mat].texture;
    return tex.empty() ? -1 : TileForTexture(scene, tex);
}

void BuildMeshChunks(const Scene& scene, const XFile::Mesh& mesh, int frame_index,
                     std::vector<DrawChunk>& out) {
    std::map<int, DrawChunk> by_tile;
    std::map<int, std::map<uint32_t, uint16_t>> remaps;
    const size_t tris = mesh.indices.size() / 3;
    for (size_t t = 0; t < tris; t++) {
        const int tile = TileForTriangle(scene, mesh, t);
        DrawChunk& chunk = by_tile[tile];
        if (chunk.vertices.empty()) {
            chunk.frame = frame_index;
            chunk.tile = tile;
        }
        if (chunk.vertices.size() / kVertexFloats >= kMaxChunkVerts) continue;
        AppendTriangle(mesh, t, chunk, remaps[tile]);
    }
    for (auto& [tile, chunk] : by_tile) {
        (void)tile;
        if (!chunk.indices.empty()) out.push_back(std::move(chunk));
    }
}

void BuildChunks(const Scene& scene, Model& model) {
    for (size_t fi = 0; fi < model.scene.frames.size(); fi++) {
        for (const auto& mesh : model.scene.frames[fi].meshes)
            BuildMeshChunks(scene, mesh, (int)fi, model.chunks);
    }
}

void ComputeBounds(Scene& out) {
    bool any = false;
    std::array<float, 3> lo = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> hi = {0.0F, 0.0F, 0.0F};
    std::vector<XFile::Matrix> locals;
    std::vector<XFile::Matrix> worlds;
    for (const auto& model : out.models) {
        SampleLocals(model.scene, 0.0F, locals);
        ComputeWorlds(model.scene, locals, worlds);
        for (const auto& chunk : model.chunks) {
            const XFile::Matrix& m = worlds[(size_t)chunk.frame];
            for (size_t v = 0; v + kVertexFloats <= chunk.vertices.size(); v += kVertexFloats) {
                const float x = chunk.vertices[v];
                const float y = chunk.vertices[v + 1];
                const float z = chunk.vertices[v + 2];
                const std::array<float, 3> p = {(m[0] * x) + (m[4] * y) + (m[8] * z) + m[12],
                                                (m[1] * x) + (m[5] * y) + (m[9] * z) + m[13],
                                                (m[2] * x) + (m[6] * y) + (m[10] * z) + m[14]};
                for (size_t k = 0; k < 3; k++) {
                    lo[k] = any ? std::min(lo[k], p[k]) : p[k];
                    hi[k] = any ? std::max(hi[k], p[k]) : p[k];
                }
                any = true;
            }
        }
    }
    if (any) {
        out.bounds_min = lo;
        out.bounds_max = hi;
    }
}

struct ScreenBlendSetup {
    const char* scene;
    const char* model;
    int mode;
};

constexpr std::array<ScreenBlendSetup, 5> kScreenBlendSetups = {{
    {.scene = "mode_bg", .model = "island", .mode = kBlendOpaque},
    {.scene = "mode_bg", .model = "sea", .mode = kBlendAdditive},
    {.scene = "mode_bg", .model = "cloud", .mode = kBlendAdditive},
    {.scene = "boss_st", .model = "earth", .mode = kBlendAdditive},
    {.scene = "boss_st", .model = "bg_star", .mode = kBlendAdditive},
}};

void ApplyScreenBlendSetup(Scene& out) {
    for (auto& model : out.models) {
        for (const auto& setup : kScreenBlendSetups) {
            if (out.name != setup.scene || model.name != setup.model) continue;
            model.blend_mode = setup.mode;
            break;
        }
    }
}

void FindCamera(Scene& out) {
    for (size_t mi = 0; mi < out.models.size(); mi++) {
        const auto& frames = out.models[mi].scene.frames;
        for (size_t fi = 0; fi < frames.size(); fi++) {
            if (frames[fi].name.find("camera") == std::string::npos) continue;
            out.camera_model = (int)mi;
            out.camera_frame = (int)fi;
            return;
        }
    }
}

bool LoadModels(const std::filesystem::path& dir, Scene& out, std::string& err) {
    std::vector<std::filesystem::path> xs;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".xz") xs.push_back(e.path());
    }
    std::ranges::sort(xs);
    if (xs.empty()) {
        err = "scene directory has no .xz models";
        return false;
    }

    for (const auto& p : xs) {
        std::vector<uint8_t> raw;
        if (!Inflate(p, raw, err)) return false;
        Model model;
        model.name = p.stem().string();
        if (!XFile::Parse(std::string(raw.begin(), raw.end()), model.scene, err)) return false;
        out.max_time = std::max(out.max_time, (float)model.scene.max_key_time);
        BuildChunks(out, model);
        LOG("Scene3d", "model '%s': %zu frames, %zu chunks, %d key ticks", model.name.c_str(),
            model.scene.frames.size(), model.chunks.size(), model.scene.max_key_time);
        out.models.push_back(std::move(model));
    }
    return true;
}

}

bool IsSceneDir(const std::string& dir) {
    std::error_code ec;
    const std::filesystem::path p(dir);
    if (!std::filesystem::is_directory(p, ec)) return false;
    bool has_inz = false;
    bool has_xz = false;
    for (const auto& e : std::filesystem::directory_iterator(p, ec)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() == ".inz") has_inz = true;
        if (e.path().extension() == ".xz") has_xz = true;
    }
    return has_inz && has_xz;
}

bool Load(const std::string& dir, Scene& out, std::string& err) {
    const std::filesystem::path path(dir);
    out = Scene{};
    out.name = path.filename().string();

    std::filesystem::path inz;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(path, ec)) {
        if (e.is_regular_file() && e.path().extension() == ".inz") {
            inz = e.path();
            break;
        }
    }
    if (inz.empty()) {
        err = "scene directory has no .inz manifest";
        return false;
    }

    std::vector<uint8_t> raw;
    if (!Inflate(inz, raw, err)) return false;
    if (!Inz::Parse(std::string(raw.begin(), raw.end()), out.manifest, err)) return false;
    if (!LoadTiles(path, out, err)) return false;
    if (!LoadModels(path, out, err)) return false;
    ComputeBounds(out);
    ApplyScreenBlendSetup(out);
    FindCamera(out);

    LOG("Scene3d",
        "scene '%s': %zu tiles, %zu models, %.0f ticks, camera=%d/%d, "
        "bounds=(%.0f,%.0f,%.0f)..(%.0f,%.0f,%.0f)",
        out.name.c_str(), out.tiles.size(), out.models.size(), out.max_time, out.camera_model,
        out.camera_frame, out.bounds_min[0], out.bounds_min[1], out.bounds_min[2],
        out.bounds_max[0], out.bounds_max[1], out.bounds_max[2]);
    return true;
}

}
