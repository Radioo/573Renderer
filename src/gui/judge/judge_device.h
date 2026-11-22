#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include "judge/judge_backdrop.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace JudgeDevice {

struct StageTarget {
    IDirect3DTexture9* texture = nullptr;
    IDirect3DSurface9* surface = nullptr;
};

struct Window {
    HWND hwnd = nullptr;
    IDirect3D9Ex* d3d = nullptr;
    IDirect3DDevice9Ex* device = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    IDirect3DSurface9* depth = nullptr;
    std::array<StageTarget, 2> stages{};
    int stage_w = 0;
    int stage_h = 0;
    bool resize_pending = false;
    bool closed = false;
};

[[nodiscard]] bool Create(Window& w, HINSTANCE instance, int stage_w, int stage_h, bool vsync);
void Destroy(Window& w);

bool Pump(Window& w);
void ApplyResize(Window& w);

void BeginStage(Window& w, std::size_t index, const Judge::BackdropFill& backdrop);
void BeginWindow(Window& w);
void Present(Window& w);

[[nodiscard]] bool ReadSurface(Window& w, IDirect3DSurface9* surface,
                               std::vector<std::uint8_t>& bgra, int& width, int& height);
[[nodiscard]] IDirect3DSurface9* BackBuffer(Window& w);

}
