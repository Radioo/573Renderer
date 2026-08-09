#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdint>

#include "afp_ddr_render.h"
#include "support/engine_abi.h"

namespace DdrRender {

struct Vtx {
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};

inline constexpr DWORD kFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
inline constexpr int kMaxTex = 4096;
inline constexpr int kMaxDrawVerts = 16384;

extern IDirect3DDevice9* g_dev;
extern int g_w;
extern int g_h;
extern int g_draw_count;
extern int g_frame;
extern int g_shape_count;
extern int g_max_count;
extern int g_trunc_count;
extern int g_loadmat_count;
extern IDirect3DTexture9* g_tex[kMaxTex];
extern int g_tex_count;
extern D3DMATRIX g_proj;
extern D3DMATRIX g_world;
extern bool g_have_proj;
extern bool g_have_world;
extern bool g_in_mask_write;
extern RECT g_scissor;
extern int g_blend_mode;
extern bool g_filter_on;
extern float g_filter_dh;
extern float g_filter_ds;
extern float g_filter_dl;
extern TexBindResolver g_tex_bind;
extern BitmapQuery g_bitmap_query;

int DumpFrame();
void IdentityM(D3DMATRIX& m);
IDirect3DTexture9* TextureAt(int slot);
void UvBias(IDirect3DTexture9* tex, float& du, float& dv);
void DumpAtlases();

int AFP_CB Cb_TexCreate(void* ctx, unsigned int w, unsigned int h, int fmt, int a5, int a6, int a7);
void AFP_CB Cb_TexDestroy(int id);
void AFP_CB Cb_TexUpload(int id, int fmt, intptr_t a3, intptr_t a4, int x, int y, int w, int h,
                         void* pixels);
void* AFP_CB Cb_Alloc(void* ctx, unsigned int size);
void* AFP_CB Cb_Realloc(void* ctx, void* p, unsigned int size);
void AFP_CB Cb_Free(void* ctx, void* p);
void AFP_CB Cb_InitFrame();
void AFP_CB Cb_FinishFrame();
void AFP_CB Cb_SetMask(int type, int level, int x, int y, int w, int h, int a7);
void AFP_CB Cb_SetPriority(int p);
void AFP_CB Cb_SetBlend(int mode);
void AFP_CB Cb_SetFilter(int a1, int a2, void* a3);
void AFP_CB Cb_SetDrawRect(const float* r);
void AFP_CB Cb_LoadMatrix(float* m2x3);
void AFP_CB Cb_LoadMatrix44(float* m);
void AFP_CB Cb_LoadProj44(float* m);
void AFP_CB Cb_GetScreenSize(int* x, int* y, int* w, int* h);
void AFP_CB Cb_GetNearFar(float* nr, float* fr);
void AFP_CB Cb_DrawPrimitive(const float* vtx, int count, int* params, void* a4);
void AFP_CB Cb_DrawPrimitiveLegacy(const float* vtx, int count, int prim_type, unsigned attr,
                                   int a5, int a6, const float* c0, const float* c1, void* ctx);
int __stdcall Cb_GetBitmapInfo(unsigned* out_id, int* out_w, int* out_h, float* out_u0,
                               float* out_u1, float* out_v0, float* out_v1, const char* name);

}
