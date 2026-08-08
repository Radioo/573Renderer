#include "afp_ddr_render_shape.h"

#include "afp_ddr_render.h"
#include "afp_ddr_shape.h"
#include "support/engine_abi.h"
#include "support/log.h"

namespace DdrRender {
namespace {

const DdrAfp::ShapeProvider* g_provider = nullptr;
int g_id_queries = 0;
int g_draws = 0;

void EmitBatch(const DdrAfp::GeoTriBatch& batch) {
    DrawShapeTriangles(batch.texture_slot, batch.modulate, batch.positions, batch.uvs, batch.colors,
                       batch.indices, batch.index_count, batch.vertex_count);
}

}

void SetShapeProvider(const DdrAfp::ShapeProvider* provider) {
    g_provider = provider;
    g_id_queries = 0;
    g_draws = 0;
}

int AFP_CB Cb_GetShapeId(unsigned int* out_id, const char* name) {
    if (out_id == nullptr || name == nullptr) return 0;
    if (g_provider == nullptr || g_provider->find == nullptr) {
        if (g_id_queries++ < 8) LOG("DDR-R", "get_shape_id('%s'): no provider", name);
        return 0;
    }
    unsigned int id = 0;
    const bool hit = g_provider->find(name, &id);
    if (g_id_queries++ < 16) {
        LOG("DDR-R", "get_shape_id('%s') -> %s id=%#x", name, hit ? "hit" : "MISS", id);
    }
    if (!hit) return 0;
    *out_id = id;
    return 1;
}

int AFP_CB Cb_GetShapeRect(unsigned int id, float* out4) {
    if (out4 == nullptr || g_provider == nullptr || g_provider->rect == nullptr) return 0;
    return g_provider->rect(id, out4) ? 1 : 0;
}

void AFP_CB Cb_DrawShape(unsigned int id, const float* c0, const float* c1, void* ctx) {
    (void)c1;
    (void)ctx;
    if (g_provider == nullptr || g_provider->draw == nullptr) return;
    if (g_draws++ < 8) LOG("DDR-R", "draw_shape id=%#x", id);
    g_provider->draw(id, c0, EmitBatch);
}

}
