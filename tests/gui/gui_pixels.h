#pragma once

#include <cstdint>
#include <d3d9.h>
#include <vector>

namespace GuiTest {

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgra;
};

inline constexpr Rgb kShellClearColor{16, 17, 20};

bool CaptureBackBuffer(IDirect3DDevice9* device, Framebuffer& out);

int CountPixels(const Framebuffer& fb, Rgb color, int tolerance);

Rgb StyleColor(int imgui_col);

bool SameColor(Rgb a, Rgb b, int tolerance);

}
