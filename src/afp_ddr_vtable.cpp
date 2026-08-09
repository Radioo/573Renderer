#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include "afp_ddr_render.h"
#include "afp_ddr_render_internal.h"
#include "afp_ddr_render_shape.h"
#include "support/engine_abi.h"
#include "support/env.h"
#include "support/log.h"

namespace DdrRender {
namespace {

intptr_t Cb_Noop() {
    return 0;
}

int AFP_CB Cb_IdentityTexBind(unsigned id) {
    return (int)id;
}

constexpr size_t kRenderParamsSlots = 0x140 / 8;
constexpr size_t kAfpuConfigSlots = 0x80 / 8;

constexpr size_t kSlotInitFrame = 1;
constexpr size_t kSlotFinishFrame = 2;
constexpr size_t kSlotSetMask = 3;
constexpr size_t kSlotSetBlend = 4;
constexpr size_t kSlotSetPriority = 5;
constexpr size_t kSlotSetFilter = 6;
constexpr size_t kSlotDrawPrimitive = 7;
constexpr size_t kSlotDrawShape = 8;
constexpr size_t kSlotLoadMatrix = 9;
constexpr size_t kSlotLoadMatrix44 = 10;
constexpr size_t kSlotLoadProj44 = 11;
constexpr size_t kSlotGetScreenSize = 12;
constexpr size_t kSlotGetNearFar = 13;
constexpr size_t kSlotSetDrawRect = 14;
constexpr size_t kSlotOptionalFirst = 15;
constexpr size_t kSlotGetShapeId = 16;
constexpr size_t kSlotGetShapeRect = 17;
constexpr size_t kSlotOptionalLast = 34;
constexpr size_t kSlotReserved = 35;
constexpr size_t kSlotAlloc = 36;
constexpr size_t kSlotRealloc = 37;
constexpr size_t kSlotFree = 38;

constexpr size_t kSlotTexCreate = 0;
constexpr size_t kSlotTexDestroy = 1;
constexpr size_t kSlotTexUpload = 2;
constexpr size_t kSlotAfpuAlloc = 4;
constexpr size_t kSlotAfpuRealloc = 5;
constexpr size_t kSlotAfpuFree = 6;
constexpr size_t kSlotAfpuNear = 7;

uint8_t g_render_params[kRenderParamsSlots * Support::kEngineSlot];
uint8_t g_afpu_config[kAfpuConfigSlots * Support::kEngineSlot];

void PutSlot(uint8_t* base, size_t slot, void* value) {
    memcpy(base + Support::SlotOffset(slot), static_cast<const void*>(&value), sizeof(value));
}

template <class F> void Put(uint8_t* base, size_t slot, F fn) {
    PutSlot(base, slot, reinterpret_cast<void*>(fn));
}

void BuildStructs(bool legacy_draw_primitive) {
    memset(g_render_params, 0, sizeof(g_render_params));
    *reinterpret_cast<uint32_t*>(g_render_params) = 0x200;
    for (size_t slot = 1; slot < kRenderParamsSlots; slot++)
        Put(g_render_params, slot, Cb_Noop);
    Put(g_render_params, kSlotInitFrame, Cb_InitFrame);
    Put(g_render_params, kSlotFinishFrame, Cb_FinishFrame);
    Put(g_render_params, kSlotSetMask, Cb_SetMask);
    Put(g_render_params, kSlotSetBlend, Cb_SetBlend);
    Put(g_render_params, kSlotSetPriority, Cb_SetPriority);
    Put(g_render_params, kSlotSetFilter, Cb_SetFilter);
    if (legacy_draw_primitive) {
        Put(g_render_params, kSlotDrawPrimitive, Cb_DrawPrimitiveLegacy);
        Put(g_render_params, kSlotOptionalFirst, Cb_GetBitmapInfo);
        Put(g_render_params, kSlotGetShapeId, Cb_GetShapeId);
        Put(g_render_params, kSlotGetShapeRect, Cb_GetShapeRect);
        g_tex_bind = Cb_IdentityTexBind;
    } else {
        Put(g_render_params, kSlotDrawPrimitive, Cb_DrawPrimitive);
    }
    Put(g_render_params, kSlotDrawShape, Cb_DrawShape);
    Put(g_render_params, kSlotLoadMatrix, Cb_LoadMatrix);
    Put(g_render_params, kSlotLoadMatrix44, Cb_LoadMatrix44);
    Put(g_render_params, kSlotLoadProj44, Cb_LoadProj44);
    Put(g_render_params, kSlotGetScreenSize, Cb_GetScreenSize);
    Put(g_render_params, kSlotGetNearFar, Cb_GetNearFar);
    Put(g_render_params, kSlotSetDrawRect, Cb_SetDrawRect);
    if (Support::EnvFlag("DDR_DEFAULT_CB")) {
        for (size_t slot = kSlotOptionalFirst; slot <= kSlotOptionalLast; slot++)
            PutSlot(g_render_params, slot, nullptr);
        LOG("DDR-R", "DDR_DEFAULT_CB: nulled render_params slots %zu..%zu (afp defaults)",
            kSlotOptionalFirst, kSlotOptionalLast);
    }
    PutSlot(g_render_params, kSlotReserved, nullptr);
    Put(g_render_params, kSlotAlloc, Cb_Alloc);
    Put(g_render_params, kSlotRealloc, Cb_Realloc);
    Put(g_render_params, kSlotFree, Cb_Free);

    memset(g_afpu_config, 0, sizeof(g_afpu_config));
    Put(g_afpu_config, kSlotTexCreate, Cb_TexCreate);
    Put(g_afpu_config, kSlotTexDestroy, Cb_TexDestroy);
    Put(g_afpu_config, kSlotTexUpload, Cb_TexUpload);
    Put(g_afpu_config, kSlotAfpuAlloc, Cb_Alloc);
    Put(g_afpu_config, kSlotAfpuRealloc, Cb_Realloc);
    Put(g_afpu_config, kSlotAfpuFree, Cb_Free);
    auto* afpu_planes =
        reinterpret_cast<float*>(g_afpu_config + Support::SlotOffset(kSlotAfpuNear));
    afpu_planes[0] = 1.0F;
    afpu_planes[1] = 9999.0F;
}

void SetupOrthoProjection() {
    memset(&g_proj, 0, sizeof(g_proj));
    g_proj._11 = 2.0F / (float)g_w;
    g_proj._22 = -2.0F / (float)g_h;
    g_proj._33 = 1.0F;
    g_proj._41 = -1.0F;
    g_proj._42 = 1.0F;
    g_proj._44 = 1.0F;
    g_have_proj = true;
}

}

void Init(IDirect3DDevice9* device, int screen_w, int screen_h, bool legacy_draw_primitive) {
    g_dev = device;
    g_w = screen_w;
    g_h = screen_h;
    IdentityM(g_proj);
    IdentityM(g_world);
    SetupOrthoProjection();
    BuildStructs(legacy_draw_primitive);
    LOG("DDR-R", "render backend init %dx%d dev=%p (ortho proj, draw_primitive=%s)", g_w, g_h,
        (void*)g_dev, legacy_draw_primitive ? "legacy 9-arg" : "unified 4-arg");
}

void* RenderParams() {
    return g_render_params;
}
void* AfpuConfig() {
    return g_afpu_config;
}
void SetScreenSize(int w, int h) {
    g_w = w;
    g_h = h;
}
void SetBitmapQuery(BitmapQuery fn) {
    g_bitmap_query = fn;
}
void SetTexBindResolver(TexBindResolver fn) {
    g_tex_bind = fn;
}
int DrawCount() {
    return g_draw_count;
}
void ResetDrawCount() {
    g_draw_count = 0;
}

}
