#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdint>

namespace DdrRender {

void Init(IDirect3DDevice9* device, int screen_w, int screen_h, bool legacy_draw_primitive);

void* RenderParams();

void* AfpuConfig();

void SetScreenSize(int w, int h);

typedef int (*TexBindResolver)(unsigned int afp_tex_id);
void SetTexBindResolver(TexBindResolver fn);

using BitmapQuery = bool (*)(const char* name, unsigned* out_id, int* out_w, int* out_h,
                             float* out_u0, float* out_u1, float* out_v0, float* out_v1);

void SetBitmapQuery(BitmapQuery fn);

int CreateTextureRgba(int w, int h, const unsigned char* pixels, size_t pixel_bytes);

void DestroyTexture(int id);

void DrawShapeTriangles(int texture_slot, uint32_t modulate, const float* positions,
                        const float* uvs, const unsigned char* colors, const uint16_t* indices,
                        int index_count, int vertex_count);

int DrawCount();
void ResetDrawCount();

}
