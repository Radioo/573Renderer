#include <catch2/catch_test_macros.hpp>

#include <catch2/catch_message.hpp>

#include "gpu_context.h"
#include "render/command_list.h"
#include "render_backend.h"
#include "render_executor.h"
#include "scene_support.h"
#include "warp_device.h"

#include <d3d9.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <span>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr int kRtWidth = 320;
constexpr int kRtHeight = 240;
constexpr int kCheckerSlot = 7;
constexpr uint32_t kCheckerTexRef = kCheckerSlot - 1;

uint64_t Fnv1a64(const std::vector<uint8_t>& data) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const uint8_t b : data) {
        h ^= b;
        h *= 0x100000001b3ULL;
    }
    return h;
}

uint32_t F32Bits(float v) {
    return std::bit_cast<uint32_t>(v);
}

struct GeoBlob {
    std::array<uint32_t, 16> words{};
};

GeoBlob MakeGeo(uint32_t prim, uint32_t flags, uint32_t tex_ref, const std::vector<float>& tail) {
    GeoBlob g;
    g.words.at(0) = prim;
    g.words.at(1) = flags;
    g.words.at(2) = tex_ref;
    g.words.at(3) = 0;
    for (std::size_t i = 0; i < tail.size(); i++) {
        g.words.at(4 + i) = F32Bits(tail.at(i));
    }
    return g;
}

std::vector<float> ColoredQuadStream(float x0, float y0, float x1, float y1,
                                     const std::array<uint32_t, 4>& colors) {
    std::vector<float> src;
    const std::array<float, 4> xs = {x0, x1, x0, x1};
    const std::array<float, 4> ys = {y0, y0, y1, y1};
    const std::array<float, 4> us = {0.0F, 1.0F, 0.0F, 1.0F};
    const std::array<float, 4> vs = {0.0F, 0.0F, 1.0F, 1.0F};
    for (std::size_t v = 0; v < 4; v++) {
        src.push_back(us.at(v));
        src.push_back(vs.at(v));
        src.push_back(std::bit_cast<float>(colors.at(v)));
        src.push_back(xs.at(v));
        src.push_back(ys.at(v));
    }
    return src;
}

void SubmitColoredQuad(float x0, float y0, float x1, float y1,
                       const std::array<uint32_t, 4>& colors, uint32_t tex_ref) {
    std::vector<float> src = ColoredQuadStream(x0, y0, x1, y1, colors);
    GeoBlob geo = MakeGeo(5, 0x08 | 0x04 | 0x01, tex_ref, {1.0F, 1.0F, 1.0F, 1.0F});
    AfpD3D9::SubmitGeometry(src.data(), 4, geo.words.data(), nullptr);
}

Render::RenderCommandList RecordPixelFrame() {
    Render::RenderCommandList tap;
    g_gpu.cmd_list = &tap;
    g_gpu.deferred_replay = true;
    g_gpu.frame_cmds.clear();

    AfpD3D9::SetLayer(0, 0, nullptr);
    AfpD3D9::SetBlend(0, 0, nullptr);
    SubmitColoredQuad(20.0F, 20.0F, 150.0F, 110.0F,
                      {0xFFFF2020, 0xFF20FF20, 0xFF2020FF, 0xFFFFFFFF}, 0);

    AfpD3D9::SetBlend(1, 0, nullptr);
    SubmitColoredQuad(100.0F, 70.0F, 220.0F, 170.0F,
                      {0xFF804020, 0xFF804020, 0xFF204080, 0xFF204080}, 0);

    AfpD3D9::SetBlend(0, 0, nullptr);
    SubmitColoredQuad(170.0F, 20.0F, 298.0F, 84.0F,
                      {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}, kCheckerTexRef);

    AfpD3D9::SetMaskRegion(1, 1, 40, 130, 120, 70);
    SubmitColoredQuad(0.0F, 0.0F, 320.0F, 240.0F, {0xC0FFE040, 0xC0FFE040, 0xC040E0FF, 0xC040E0FF},
                      0);
    AfpD3D9::SetMaskRegion(2, 1, 0, 0, 0, 0);

    std::vector<float> two =
        ColoredQuadStream(0.0F, 0.0F, 8.0F, 8.0F, {0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000});
    GeoBlob zero_geo = MakeGeo(5, 0x08 | 0x04 | 0x01, 0, {1.0F, 1.0F, 1.0F, 1.0F});
    AfpD3D9::SubmitGeometry(two.data(), 2, zero_geo.words.data(), nullptr);

    g_gpu.cmd_list = nullptr;
    Render::RenderCommandList frame = std::move(g_gpu.frame_cmds);
    g_gpu.frame_cmds.clear();
    g_gpu.deferred_replay = false;
    return frame;
}

IDirect3DTexture9* CreateFilledTexture(IDirect3DDevice9* device, int w, int h,
                                       const std::vector<uint32_t>& argb) {
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(device->CreateTexture((UINT)w, (UINT)h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex,
                                     nullptr))) {
        return nullptr;
    }
    D3DLOCKED_RECT lr = {};
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
        tex->Release();
        return nullptr;
    }
    std::span<uint8_t> const bits{static_cast<uint8_t*>(lr.pBits), (size_t)lr.Pitch * h};
    for (int y = 0; y < h; y++) {
        std::span<uint8_t> const row = bits.subspan((size_t)y * lr.Pitch, (size_t)w * 4);
        std::memcpy(row.data(), &argb[(size_t)y * w], (size_t)w * 4);
    }
    tex->UnlockRect(0);
    return tex;
}

std::vector<uint32_t> CheckerPattern(int w, int h) {
    std::vector<uint32_t> px((size_t)w * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            px[((size_t)y * w) + x] = ((x + y) % 2 == 0) ? 0xFFFF00FFU : 0xFF00FF80U;
        }
    }
    return px;
}

void SetFixedFunctionBaseState(IDirect3DDevice9* device) {
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_MAX);
    device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    for (DWORD s = 0; s < 2; s++) {
        device->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        device->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    }

    const std::array<float, 16> ortho = {2.0F / (float)kRtWidth,
                                         0.0F,
                                         0.0F,
                                         0.0F,
                                         0.0F,
                                         -2.0F / (float)kRtHeight,
                                         0.0F,
                                         0.0F,
                                         0.0F,
                                         0.0F,
                                         1.0F,
                                         0.0F,
                                         -1.0F,
                                         1.0F,
                                         0.5F,
                                         1.0F};
    D3DMATRIX proj = {};
    static_assert(sizeof(proj) == sizeof(ortho));
    std::memcpy(&proj, ortho.data(), sizeof(proj));
    device->SetTransform(D3DTS_PROJECTION, &proj);
}

bool ReadBackPixels(IDirect3DDevice9* device, IDirect3DSurface9* rt, std::vector<uint8_t>& out) {
    IDirect3DSurface9* sysmem = nullptr;
    if (FAILED(device->CreateOffscreenPlainSurface(kRtWidth, kRtHeight, D3DFMT_A8R8G8B8,
                                                   D3DPOOL_SYSTEMMEM, &sysmem, nullptr))) {
        return false;
    }
    if (FAILED(device->GetRenderTargetData(rt, sysmem))) {
        sysmem->Release();
        return false;
    }
    D3DLOCKED_RECT lr = {};
    if (FAILED(sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
        sysmem->Release();
        return false;
    }
    out.resize((size_t)kRtWidth * kRtHeight * 4);
    std::span<const uint8_t> const bits{static_cast<const uint8_t*>(lr.pBits),
                                        (size_t)lr.Pitch * kRtHeight};
    std::span<uint8_t> const dst{out};
    for (int y = 0; y < kRtHeight; y++) {
        std::span<const uint8_t> const row =
            bits.subspan((size_t)y * lr.Pitch, (size_t)kRtWidth * 4);
        std::ranges::copy(row, dst.subspan((size_t)y * kRtWidth * 4, (size_t)kRtWidth * 4).begin());
    }
    sysmem->UnlockRect();
    sysmem->Release();
    return true;
}

struct RenderedFrame {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> bgra;
};

using RecordSceneFn = Render::RenderCommandList (*)();

RenderedFrame RenderGoldenFrame(WarpD3D9::Device& warp, RecordSceneFn record) {
    RenderedFrame out;
    IDirect3DDevice9* device = warp.device;

    g_gpu = GpuContext{};
    g_gpu.device = device;
    g_gpu.screen_w = kRtWidth;
    g_gpu.screen_h = kRtHeight;

    IDirect3DTexture9* white =
        CreateFilledTexture(device, 2, 2, std::vector<uint32_t>(4, 0xFFFFFFFFU));
    IDirect3DTexture9* checker = CreateFilledTexture(device, 4, 4, CheckerPattern(4, 4));
    if (white == nullptr || checker == nullptr) {
        out.error = "texture creation failed";
        return out;
    }
    g_gpu.fallback_texture = white;
    g_gpu.textures[kCheckerSlot] = checker;
    g_gpu.texture_dims_w[kCheckerSlot] = 4;
    g_gpu.texture_dims_h[kCheckerSlot] = 4;

    const Render::RenderCommandList frame = record();

    IDirect3DTexture9* rt_tex = nullptr;
    if (FAILED(device->CreateTexture(kRtWidth, kRtHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                     D3DPOOL_DEFAULT, &rt_tex, nullptr))) {
        out.error = "render target creation failed";
        return out;
    }
    IDirect3DSurface9* rt = nullptr;
    rt_tex->GetSurfaceLevel(0, &rt);
    device->SetRenderTarget(0, rt);

    SetFixedFunctionBaseState(device);
    device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 16, 32, 64), 1.0F, 0);
    device->BeginScene();
    RenderExec::ExecuteList(device, g_gpu, frame);
    device->EndScene();

    out.ok = ReadBackPixels(device, rt, out.bgra);
    if (!out.ok) out.error = "readback failed";

    rt->Release();
    rt_tex->Release();
    g_gpu.fallback_texture = nullptr;
    g_gpu.textures[kCheckerSlot] = nullptr;
    white->Release();
    checker->Release();
    g_gpu = GpuContext{};
    return out;
}

Render::RenderCommandList RecordWideScene() {
    g_gpu.cmd_list = nullptr;
    g_gpu.deferred_replay = true;
    g_gpu.frame_cmds.clear();
    SceneSupport::DriveWideScene(kCheckerTexRef);
    Render::RenderCommandList frame = std::move(g_gpu.frame_cmds);
    g_gpu.frame_cmds.clear();
    g_gpu.deferred_replay = false;
    return frame;
}

std::string HashLine(const std::vector<uint8_t>& bgra) {
    return std::format("{}x{} fnv1a64={:016x}\n", kRtWidth, kRtHeight, Fnv1a64(bgra));
}

void DumpActual(const char* dump_name, const std::vector<uint8_t>& bgra) {
    std::ofstream out(dump_name, std::ios::binary | std::ios::trunc);
    const std::string bytes(bgra.begin(), bgra.end());
    out << bytes;
}

void CheckPixelGolden(WarpD3D9::Device& warp, RecordSceneFn record, const char* golden_name,
                      const char* dump_name) {
    const RenderedFrame a = RenderGoldenFrame(warp, record);
    REQUIRE(a.ok);
    const RenderedFrame b = RenderGoldenFrame(warp, record);
    REQUIRE(b.ok);
    REQUIRE(a.bgra == b.bgra);

    const std::string line = HashLine(a.bgra);
    const std::filesystem::path dir(R573_LOCAL_BASELINE_DIR);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string path = (dir / golden_name).string();
    const std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::ofstream outf(path, std::ios::binary);
        outf << line;
        DumpActual(dump_name, a.bgra);
        WARN("machine-local pixel baseline was missing - blessed the current hash at "
             << path << "; review the dumped .bgra if this machine rendered before");
        return;
    }
    std::stringstream have;
    have << in.rdbuf();
    INFO("on mismatch the frame is dumped as 320x240 BGRA in the build dir");
    if (have.str() != line) DumpActual(dump_name, a.bgra);
    CHECK(have.str() == line);
}

}

TEST_CASE("WARP pixel golden: executor replay matches the committed hash") {
    WarpD3D9::Device warp;
    if (!WarpD3D9::Create(warp, kRtWidth, kRtHeight)) {
        SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    }
    CheckPixelGolden(warp, &RecordPixelFrame, "pixel_golden_frame.sha", "pixel_golden_actual.bgra");
}

TEST_CASE("WARP pixel golden: the wide scene (blend modes, prim types, layer multiply)") {
    WarpD3D9::Device warp;
    if (!WarpD3D9::Create(warp, kRtWidth, kRtHeight)) {
        SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    }
    CheckPixelGolden(warp, &RecordWideScene, "pixel_golden_wide.sha", "pixel_wide_actual.bgra");
}
