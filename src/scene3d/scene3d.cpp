#include "scene3d/scene3d.h"

#include "formats/gcz.h"
#include "formats/inz.h"
#include "formats/lzss.h"
#include "formats/xfile.h"
#include "scene3d/anim.h"
#include "scene3d/atlas.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <cmath>
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

constexpr Inz::AtlasGrid kAtlasGrid = {.tile_width = 256, .tile_height = 256, .tiles_per_row = 1};
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
    out.tiles.assign(out.manifest.slices.size(), Tile{});
    for (auto& tile : out.tiles) {
        tile.width = kAtlasGrid.tile_width;
        tile.height = kAtlasGrid.tile_height;
        tile.bgra.assign((size_t)kAtlasGrid.tile_width * kAtlasGrid.tile_height * 4, 0);
    }
    for (size_t i = 0; i < out.manifest.slices.size(); i++) {
        const auto& slice = out.manifest.slices[i];
        std::vector<uint8_t> raw;
        if (!Inflate(dir / (slice.name + ".gcz"), raw, err)) return false;
        Gcz::Tile parsed;
        if (!Gcz::Parse(raw, parsed, err)) return false;
        out.tiles[i].name = slice.name;
        ScatterSlice(parsed, kAtlasGrid,
                     kAtlasGrid.tile_width * (int)(i % kAtlasGrid.tiles_per_row),
                     kAtlasGrid.tile_height * (int)(i / kAtlasGrid.tiles_per_row), out.tiles);
    }
    return true;
}

Inz::Region RegionForMaterial(const Scene& scene, const XFile::Mesh& mesh, uint32_t material) {
    if (material >= mesh.materials.size()) return {};
    const std::string& tex = mesh.materials[material].texture;
    if (tex.empty()) return {};
    Inz::Region region = Inz::ResolveRegion(scene.manifest, tex, kAtlasGrid);
    if (region.tile < 0 || (size_t)region.tile >= scene.tiles.size()) return {};
    return region;
}

XFile::Vec3 CornerNormal(const XFile::Mesh& mesh, size_t corner, uint32_t position_index) {
    if (corner < mesh.normal_indices.size()) {
        const uint32_t n = mesh.normal_indices[corner];
        if (n < mesh.normals.size()) return mesh.normals[n];
    }
    if (position_index < mesh.normals.size()) return mesh.normals[position_index];
    return XFile::Vec3{.x = 0.0F, .y = 0.0F, .z = 1.0F};
}

void AppendTriangle(const XFile::Mesh& mesh, size_t tri, const Inz::Region& region,
                    DrawChunk& chunk, std::map<uint32_t, uint16_t>& remap) {
    for (size_t k = 0; k < 3; k++) {
        const size_t corner = (tri * 3) + k;
        const uint32_t src = mesh.indices[corner];
        auto it = remap.find(src);
        if (it == remap.end()) {
            const auto slot = (uint16_t)(chunk.vertices.size() / kVertexFloats);
            const auto& p = mesh.positions[src];
            const XFile::Vec3 n = CornerNormal(mesh, corner, src);
            chunk.vertices.push_back(p.x);
            chunk.vertices.push_back(p.y);
            chunk.vertices.push_back(p.z);
            chunk.vertices.push_back(n.x);
            chunk.vertices.push_back(n.y);
            chunk.vertices.push_back(n.z);
            if (src < mesh.uvs.size()) {
                chunk.vertices.push_back(region.u_bias + (mesh.uvs[src].u * region.u_scale));
                chunk.vertices.push_back(region.v_bias + (mesh.uvs[src].v * region.v_scale));
            } else {
                chunk.vertices.push_back(region.u_bias);
                chunk.vertices.push_back(region.v_bias);
            }
            it = remap.emplace(src, slot).first;
        }
        chunk.indices.push_back(it->second);
    }
}

void BuildMeshChunks(const Scene& scene, const XFile::Mesh& mesh, int frame_index,
                     std::vector<DrawChunk>& out) {
    std::map<uint32_t, DrawChunk> by_material;
    std::map<uint32_t, std::map<uint32_t, uint16_t>> remaps;
    std::map<uint32_t, Inz::Region> regions;
    const size_t tris = mesh.indices.size() / 3;
    for (size_t t = 0; t < tris; t++) {
        const uint32_t mat = (t < mesh.face_material.size()) ? mesh.face_material[t] : 0;
        auto region = regions.find(mat);
        if (region == regions.end())
            region = regions.emplace(mat, RegionForMaterial(scene, mesh, mat)).first;
        DrawChunk& chunk = by_material[mat];
        if (chunk.vertices.empty()) {
            chunk.frame = frame_index;
            chunk.tile = region->second.tile;
            if (mat < mesh.materials.size()) {
                chunk.diffuse = mesh.materials[mat].diffuse;
                chunk.emissive = mesh.materials[mat].emissive;
            }
        }
        if (chunk.vertices.size() / kVertexFloats >= kMaxChunkVerts) continue;
        AppendTriangle(mesh, t, region->second, chunk, remaps[mat]);
    }
    for (auto& [mat, chunk] : by_material) {
        (void)mat;
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

XFile::Matrix ModelTransform(const Model& model) {
    XFile::Matrix m = XFile::Identity();
    m[0] = model.scale[0];
    m[5] = model.scale[1];
    m[10] = model.scale[2];
    for (size_t axis = 0; axis < 3; axis++) {
        const float a = model.rotation[axis];
        if (a == 0.0F) continue;
        const float c = std::cos(a);
        const float s = std::sin(a);
        XFile::Matrix r = XFile::Identity();
        if (axis == 0) {
            r[5] = c;
            r[6] = s;
            r[9] = -s;
            r[10] = c;
        } else if (axis == 1) {
            r[0] = c;
            r[2] = -s;
            r[8] = s;
            r[10] = c;
        } else {
            r[0] = c;
            r[1] = s;
            r[4] = -s;
            r[5] = c;
        }
        m = Multiply(m, r);
    }
    m[12] += model.position[0];
    m[13] += model.position[1];
    m[14] += model.position[2];
    return m;
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
