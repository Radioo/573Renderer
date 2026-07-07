#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "render/vertex_math.h"

constexpr uint32_t AFP_FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

struct D3D9State {
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    HWND hwnd = nullptr;
    int width = 1280;
    int height = 720;

    IDirect3DVertexShader9* afp_vs = nullptr;

    IDirect3DPixelShader9* afp_hsl_ps = nullptr;

    IDirect3DPixelShader9* afp_add_ps = nullptr;

    IDirect3DTexture9* afp_texture = nullptr;

    IDirect3DSurface9* backbuffer = nullptr;
    IDirect3DSurface9* offscreen_rt = nullptr;
    IDirect3DSurface9* depth_stencil = nullptr;

    bool Init(HWND window);
    void Shutdown();
    void BeginFrame() const;
    void EndFrame() const;

    uint32_t clear_color = 0x00000000;

    bool SaveBackBufferToFile(const char* path) const;

    bool SaveOffscreenRGBAToPNG(const char* path) const;

    bool ReadOffscreenBGRA(std::vector<uint8_t>& out, int& out_w, int& out_h) const;

    void GetOffscreenSize(int& w, int& h) const;

    void GetBackBufferSize(int& w, int& h) const;

    void DrawCropOverlay() const;
};

void D3D9State_RequestScreenshot(const char* path);

struct AfpRenderContext {
    static constexpr size_t kDeviceOffset = 0x18000;
    static constexpr size_t kSize = 0x20000;
    uint8_t data[kSize];

    void InitZero() { memset(data, 0, sizeof(data)); }
    uint32_t& Flags() { return *(uint32_t*)&data[0]; }
    void*& FnAt(int offset) { return *(void**)&data[offset]; }

    IDirect3DDevice9*& DeviceAt() {
        return *reinterpret_cast<IDirect3DDevice9**>(&data[kDeviceOffset]);
    }
};

namespace AfpD3D9 {
void Init(IDirect3DDevice9* device, int screen_w, int screen_h,
          IDirect3DVertexShader9* vs = nullptr, IDirect3DPixelShader9* hsl_ps = nullptr,
          IDirect3DPixelShader9* add_ps = nullptr);

void SetStateCtx(void* ctx);

void SetAfpuTexSlotResolver(uint32_t (*fn)(uint32_t tex_id));

int GetTotalDrawCount();

IDirect3DTexture9* ResolveTexture(uint32_t tex_ref);

bool ReadTexturePixels(IDirect3DTexture9* tex, std::vector<uint8_t>& out, int& out_w, int& out_h);

int NextSlot();
IDirect3DTexture9* GetTexture(int slot);
bool GetTextureSize(int slot, int& w, int& h);
void SetNextSlot(int slot);

void SetQproDrawProbe(bool on);

void SetHandRenderShift(bool on);
float RenderXOffset();

void SetHsvScopeRect(float umin, float umax, float vmin, float vmax);
void SetHsvScopeRect2(float umin, float umax, float vmin, float vmax);
void ResetHsvScopeRect();

void SetAfpuSetScreenRectFnOffset(uintptr_t offset);

void SetScreenSize(int w, int h);

void __cdecl BeginRender();
void __cdecl EndRender();
void __cdecl SetLayer(unsigned int blend_mode, int zero, const unsigned char* hsv_desc);
void __cdecl DrawPrimitive(unsigned int prim_info);
void __cdecl SetBlend(unsigned int blend_mode, unsigned char flags, void* extra);
void __cdecl SetMask(unsigned int type, unsigned char level, void* data);
void __cdecl SubmitGeometry(void* a1, int a2, void* geo_data, void* tex_ptr);

void __cdecl LayerCommand(unsigned int cmd, uint64_t a2, uint64_t a3, void** a4);
void __cdecl SetMaskRegion(unsigned int mode, unsigned int layer, unsigned int x, unsigned int y,
                           unsigned int w, unsigned int h);
bool InMaskWrite();
void ResetMaskWrite();
void __cdecl SetShapeMatrix(const int* mat2d);
void __cdecl SetMatrix(void* matrix_4x4);
void __cdecl SetMatrixFull(void* data);
void __cdecl InvalidateBlend();
void __cdecl GetScreenSize(int* x, int* y, int* w, int* h);
void __cdecl GetNearFar(float* near_val, float* far_val);
char __cdecl FindTexture(void* name, void* data);

void MarkPersistentBoundary();

void ResetAllTextures();

int __cdecl TexCreate(void* ctx, unsigned int width, unsigned int height);
void __cdecl TexDestroy(unsigned int tex_id);
void __cdecl TexUpload(unsigned int tex_id, unsigned int format, unsigned int width,
                       unsigned int height, int x_off, int y_off, int src_w, int src_h,
                       void* pixel_data);

void EnqueueAtlasFilter(unsigned int mag_d3d, unsigned int min_d3d);
void ClearAtlasFilterQueue();

int LoadExternalImageSlot(const std::string& path, int& out_w, int& out_h);
}
