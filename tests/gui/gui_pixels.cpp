#include "gui_pixels.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <d3d9.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace GuiTest {

namespace {

int AbsDiff(uint8_t a, uint8_t b) {
    return a > b ? static_cast<int>(a) - static_cast<int>(b)
                 : static_cast<int>(b) - static_cast<int>(a);
}

}

bool CaptureBackBuffer(IDirect3DDevice9* device, Framebuffer& out) {
    if (device == nullptr) return false;

    IDirect3DSurface9* back = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || back == nullptr) {
        return false;
    }

    D3DSURFACE_DESC desc = {};
    back->GetDesc(&desc);

    IDirect3DSurface9* sysmem = nullptr;
    bool ok = SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                            D3DPOOL_SYSTEMMEM, &sysmem, nullptr)) &&
              sysmem != nullptr;
    if (ok) ok = SUCCEEDED(device->GetRenderTargetData(back, sysmem));

    D3DLOCKED_RECT locked = {};
    if (ok) ok = SUCCEEDED(sysmem->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    if (ok) {
        out.width = static_cast<int>(desc.Width);
        out.height = static_cast<int>(desc.Height);
        out.bgra.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4, 0);
        for (int y = 0; y < out.height; y++) {
            std::memcpy(out.bgra.data() +
                            (static_cast<size_t>(y) * static_cast<size_t>(out.width) * 4),
                        static_cast<const uint8_t*>(locked.pBits) +
                            (static_cast<size_t>(y) * static_cast<size_t>(locked.Pitch)),
                        static_cast<size_t>(out.width) * 4);
        }
        sysmem->UnlockRect();
    }

    if (sysmem != nullptr) sysmem->Release();
    back->Release();
    return ok;
}

Rgb StyleColor(int imgui_col) {
    ImVec4 const c = ImGui::GetStyleColorVec4(imgui_col);
    auto to8 = [](float v) {
        return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F));
    };
    return Rgb{.r = to8(c.x), .g = to8(c.y), .b = to8(c.z)};
}

bool SameColor(Rgb a, Rgb b, int tolerance) {
    return AbsDiff(a.r, b.r) <= tolerance && AbsDiff(a.g, b.g) <= tolerance &&
           AbsDiff(a.b, b.b) <= tolerance;
}

int CountPixels(const Framebuffer& fb, Rgb color, int tolerance) {
    int hits = 0;
    for (size_t i = 0; i + 3 < fb.bgra.size(); i += 4) {
        if (AbsDiff(fb.bgra[i + 2], color.r) > tolerance) continue;
        if (AbsDiff(fb.bgra[i + 1], color.g) > tolerance) continue;
        if (AbsDiff(fb.bgra[i], color.b) > tolerance) continue;
        hits++;
    }
    return hits;
}

}
