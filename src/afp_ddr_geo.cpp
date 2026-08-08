#include "afp_ddr_geo.h"

#include "afp_ddr_shape.h"
#include "afp_ddr_txp2.h"
#include "formats/txp2.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace DdrAfp {
namespace {

constexpr uint8_t kPrimDraw = 0x01;
constexpr uint8_t kPrimTextured = 0x02;
constexpr uint8_t kPrimColored = 0x08;
constexpr uint8_t kPrimUvPending = 0x40;

constexpr uint32_t kShapePackShift = 1;
constexpr uint32_t kShapePackMask = 0x7F;
constexpr uint32_t kShapeIndexShift = 8;

struct ResolvedPrim {
    uint8_t flags = 0;
    int texture_slot = -1;
    uint32_t rgba = 0xFFFFFFFFU;
    std::vector<uint16_t> indices;
};

struct ResolvedShape {
    std::string name;
    std::vector<float> positions;
    std::vector<float> uvs;
    std::vector<uint8_t> colors;
    std::vector<ResolvedPrim> prims;
    int vertex_count = 0;
};

std::vector<ResolvedShape> g_shapes;
uint32_t g_pack_slot = 0;

struct CellUv {
    int texture_slot = -1;
    float u0 = 0.0F;
    float u1 = 1.0F;
    float v0 = 0.0F;
    float v1 = 1.0F;
};

bool LookupCell(const Txp2Loaded& loaded, const std::vector<int>& texture_slots,
                const std::string& name, CellUv& out) {
    const auto& pkg = loaded.package;
    for (const auto& entry : pkg.cell_names) {
        if (entry.name != name) continue;
        if (entry.cell_index >= pkg.cells.size()) return false;
        const auto& cell = pkg.cells[entry.cell_index];
        if (cell.texture_index >= texture_slots.size()) return false;
        if (cell.texture_index >= loaded.textures.size()) return false;
        const auto& tex = loaded.textures[cell.texture_index];
        if (tex.width <= 0 || tex.height <= 0) return false;
        const auto w = (float)tex.width;
        const auto h = (float)tex.height;
        out.texture_slot = texture_slots[cell.texture_index];
        out.u0 = (float)cell.x0 * 0.5F / w;
        out.u1 = (float)cell.x1 * 0.5F / w;
        out.v0 = (float)cell.y0 * 0.5F / h;
        out.v1 = (float)cell.y1 * 0.5F / h;
        return true;
    }
    return false;
}

void RemapPrimUvs(const Txp2::GeoPrim& prim, const CellUv& cell, ResolvedShape& shape,
                  std::vector<uint8_t>& seen) {
    for (const uint16_t vi : prim.indices) {
        if (vi >= seen.size() || seen[vi] != 0) continue;
        seen[vi] = 1;
        const size_t at = (size_t)vi * 2;
        if (at + 1 >= shape.uvs.size()) continue;
        shape.uvs[at] = ((cell.u1 - cell.u0) * shape.uvs[at]) + cell.u0;
        shape.uvs[at + 1] = ((cell.v1 - cell.v0) * shape.uvs[at + 1]) + cell.v0;
    }
}

uint32_t PackRgba(const std::array<uint8_t, 4>& rgba) {
    return ((uint32_t)rgba[3] << 24) | ((uint32_t)rgba[0] << 16) | ((uint32_t)rgba[1] << 8) |
           (uint32_t)rgba[2];
}

ResolvedShape ResolveShape(const Txp2::GeoShape& src, const Txp2Loaded& loaded,
                           const std::vector<int>& texture_slots, int& unresolved) {
    ResolvedShape out;
    out.name = src.name;
    out.positions = src.positions;
    out.uvs = src.uvs;
    out.colors = src.colors;
    out.vertex_count = (int)src.vertex_count;

    std::vector<uint8_t> seen(src.uv_count, 0);
    out.prims.reserve(src.prims.size());
    for (const auto& prim : src.prims) {
        ResolvedPrim rp;
        rp.flags = prim.flags;
        rp.rgba = PackRgba(prim.rgba);
        rp.indices = prim.indices;
        if ((prim.flags & kPrimTextured) != 0 && prim.bitmap_ref < src.bitmap_names.size()) {
            CellUv cell;
            if (LookupCell(loaded, texture_slots, src.bitmap_names[prim.bitmap_ref], cell)) {
                rp.texture_slot = cell.texture_slot;
                if ((prim.flags & kPrimUvPending) != 0) {
                    RemapPrimUvs(prim, cell, out, seen);
                    rp.flags &= (uint8_t)~kPrimUvPending;
                }
            } else {
                unresolved++;
            }
        }
        out.prims.push_back(std::move(rp));
    }
    return out;
}

bool DecodeShapeId(uint32_t id, size_t& index) {
    if ((id & 1U) != 0) return false;
    if (((id >> kShapePackShift) & kShapePackMask) != g_pack_slot) return false;
    const uint32_t instance = id >> kShapeIndexShift;
    if (instance == 0 || instance > g_shapes.size()) return false;
    index = instance - 1;
    return true;
}

bool ShapeFind(const char* name, uint32_t* out_id) {
    if (name == nullptr || out_id == nullptr) return false;
    for (size_t i = 0; i < g_shapes.size(); i++) {
        if (g_shapes[i].name != name) continue;
        *out_id = ((g_pack_slot & kShapePackMask) << kShapePackShift) |
                  ((uint32_t)(i + 1) << kShapeIndexShift);
        return true;
    }
    return false;
}

bool ShapeRect(uint32_t id, float* out4) {
    size_t index = 0;
    if (out4 == nullptr || !DecodeShapeId(id, index)) return false;
    const auto& shape = g_shapes[index];
    out4[0] = 0.0F;
    out4[1] = 0.0F;
    out4[2] = 0.0F;
    out4[3] = 0.0F;
    if (shape.positions.size() < 2) return true;
    out4[0] = shape.positions[0];
    out4[1] = shape.positions[0];
    out4[2] = shape.positions[1];
    out4[3] = shape.positions[1];
    for (size_t i = 2; i + 1 < shape.positions.size(); i += 2) {
        out4[0] = std::min(out4[0], shape.positions[i]);
        out4[1] = std::max(out4[1], shape.positions[i]);
        out4[2] = std::min(out4[2], shape.positions[i + 1]);
        out4[3] = std::max(out4[3], shape.positions[i + 1]);
    }
    return true;
}

uint8_t ScaleChannel(uint32_t channel, float factor) {
    const float scaled = ((float)channel / 255.0F) * factor * 255.0F;
    const auto rounded = (int)std::lroundf(scaled);
    return (uint8_t)std::clamp(rounded, 0, 255);
}

uint32_t ModulateRgba(uint32_t rgba, const float* modulate) {
    if (modulate == nullptr) return rgba;
    const uint32_t a = ScaleChannel((rgba >> 24) & 0xFFU, modulate[3]);
    const uint32_t r = ScaleChannel((rgba >> 16) & 0xFFU, modulate[0]);
    const uint32_t g = ScaleChannel((rgba >> 8) & 0xFFU, modulate[1]);
    const uint32_t b = ScaleChannel(rgba & 0xFFU, modulate[2]);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t FlatRgba(const float* modulate) {
    if (modulate == nullptr) return 0xFFFFFFFFU;
    return ModulateRgba(0xFFFFFFFFU, modulate);
}

bool ShapeDraw(uint32_t id, const float* modulate, GeoSink sink) {
    size_t index = 0;
    if (sink == nullptr || !DecodeShapeId(id, index)) return false;
    const auto& shape = g_shapes[index];
    for (const auto& prim : shape.prims) {
        if ((prim.flags & kPrimUvPending) != 0) continue;
        if ((prim.flags & kPrimDraw) == 0) continue;
        if (prim.indices.empty() || shape.positions.empty()) continue;
        GeoTriBatch batch;
        batch.texture_slot = ((prim.flags & kPrimTextured) != 0) ? prim.texture_slot : -1;
        batch.modulate = ((prim.flags & kPrimColored) != 0) ? ModulateRgba(prim.rgba, modulate)
                                                            : FlatRgba(modulate);
        batch.positions = shape.positions.data();
        batch.uvs = shape.uvs.empty() ? nullptr : shape.uvs.data();
        batch.colors = shape.colors.empty() ? nullptr : shape.colors.data();
        batch.indices = prim.indices.data();
        batch.index_count = (int)prim.indices.size();
        batch.vertex_count = shape.vertex_count;
        sink(batch);
    }
    return true;
}

const ShapeProvider g_provider = {
    .find = ShapeFind,
    .rect = ShapeRect,
    .draw = ShapeDraw,
};

}

void BuildGeoRegistry(const Txp2Loaded& loaded, const std::vector<int>& texture_slots) {
    g_shapes.clear();
    g_shapes.reserve(loaded.package.shapes.size());
    int unresolved = 0;
    int prim_total = 0;
    for (const auto& src : loaded.package.shapes) {
        g_shapes.push_back(ResolveShape(src, loaded, texture_slots, unresolved));
        prim_total += (int)g_shapes.back().prims.size();
    }
    LOG("DDR", "geometry: %zu shapes, %d primitives, %d unresolved bitmap refs", g_shapes.size(),
        prim_total, unresolved);
}

size_t GeoShapeCount() {
    return g_shapes.size();
}

const ShapeProvider& GeoProvider() {
    return g_provider;
}

}
