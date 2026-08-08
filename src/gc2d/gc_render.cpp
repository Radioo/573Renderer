#include "gc2d/gc_render.h"

#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_package.h"
#include "support/log.h"

#include <d3d9.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Gc2d {

namespace {

constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 480;
constexpr float kHalfTexel = 0.5F;

struct Vtx {
    float x;
    float y;
    float z;
    float rhw;
    D3DCOLOR color;
    float u;
    float v;
};

struct CellSegment {
    int tile = 0;
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 0.0F;
    float v1 = 0.0F;
    float fy0 = 0.0F;
    float fy1 = 0.0F;
};

void Rotate(float& x, float& y, float cx, float cy, float s, float c) {
    const float dx = x - cx;
    const float dy = y - cy;
    x = cx + (dx * c) - (dy * s);
    y = cy + (dx * s) + (dy * c);
}

void SplitCell(const Package& pkg, const SysIdx::Cell& cell, std::vector<CellSegment>& out) {
    out.clear();
    if (cell.h == 0 || cell.w == 0) return;
    const int y0 = cell.y;
    const int y1 = cell.y + cell.h;

    for (size_t i = 0; i < pkg.tiles.size(); i++) {
        const TileImage& tile = pkg.tiles[i];
        if (tile.width <= 0 || tile.height <= 0) continue;
        const int tile_top = (int)i * SysIdx::kAtlasTileHeight;
        const int top = std::max(y0, tile_top);
        const int bot = std::min(y1, tile_top + tile.height);
        if (bot <= top) continue;

        CellSegment seg;
        seg.tile = (int)i;
        seg.u0 = (float)(cell.x - tile.origin_x) / (float)tile.width;
        seg.u1 = (float)((int)cell.x + (int)cell.w - tile.origin_x) / (float)tile.width;
        seg.v0 = (float)(top - tile_top - tile.origin_y) / (float)tile.height;
        seg.v1 = (float)(bot - tile_top - tile.origin_y) / (float)tile.height;
        seg.fy0 = (float)(top - y0) / (float)cell.h;
        seg.fy1 = (float)(bot - y0) / (float)cell.h;
        out.push_back(seg);
    }
}

void ApplyBlend(IDirect3DDevice9* dev, GcAnim::Blend blend) {
    switch (blend) {
    case GcAnim::Blend::Additive:
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        break;
    case GcAnim::Blend::Subtract:
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
        break;
    default:
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        break;
    }
}

void DrawSegment(IDirect3DDevice9* dev, IDirect3DTexture9* tex, const GcAnim::DrawNode& node,
                 const CellSegment& seg, float sx, float sy) {
    dev->SetTexture(0, tex);

    const auto a = (int)(node.alpha * 255.0F);
    const D3DCOLOR color = D3DCOLOR_ARGB(std::clamp(a, 0, 255), 255, 255, 255);

    const float x0 = (node.x * sx) - kHalfTexel;
    const float x1 = ((node.x + node.w) * sx) - kHalfTexel;
    const float y0 = ((node.y + (node.h * seg.fy0)) * sy) - kHalfTexel;
    const float y1 = ((node.y + (node.h * seg.fy1)) * sy) - kHalfTexel;

    Vtx quad[4] = {
        {.x = x0, .y = y0, .z = 0.0F, .rhw = 1.0F, .color = color, .u = seg.u0, .v = seg.v0},
        {.x = x1, .y = y0, .z = 0.0F, .rhw = 1.0F, .color = color, .u = seg.u1, .v = seg.v0},
        {.x = x1, .y = y1, .z = 0.0F, .rhw = 1.0F, .color = color, .u = seg.u1, .v = seg.v1},
        {.x = x0, .y = y1, .z = 0.0F, .rhw = 1.0F, .color = color, .u = seg.u0, .v = seg.v1},
    };
    if (node.rotation != 0.0F) {
        const float cx = node.pivot_x * sx;
        const float cy = node.pivot_y * sy;
        const float s = std::sin(node.rotation);
        const float c = std::cos(node.rotation);
        for (auto& v : quad)
            Rotate(v.x, v.y, cx, cy, s, c);
    }
    const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, idx, D3DFMT_INDEX16, quad,
                                sizeof(Vtx));
}

}

bool Renderer::Init(IDirect3DDevice9* device, const Package& pkg) {
    Release();
    dev_ = device;
    if (dev_ == nullptr || pkg.tiles.empty()) return false;

    textures_.assign(pkg.tiles.size(), nullptr);
    for (size_t i = 0; i < pkg.tiles.size(); i++) {
        const TileImage& tile = pkg.tiles[i];
        if (tile.width <= 0 || tile.height <= 0) continue;
        IDirect3DTexture9* staging = nullptr;
        if (FAILED(dev_->CreateTexture((UINT)tile.width, (UINT)tile.height, 1, 0, D3DFMT_A8R8G8B8,
                                       D3DPOOL_SYSTEMMEM, &staging, nullptr))) {
            LOG("Gc2d", "could not create a %dx%d staging texture", tile.width, tile.height);
            continue;
        }
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(staging->LockRect(0, &lr, nullptr, 0))) {
            for (int row = 0; row < tile.height; row++) {
                std::memcpy(static_cast<uint8_t*>(lr.pBits) + ((size_t)row * lr.Pitch),
                            tile.bgra.data() + ((size_t)row * (size_t)tile.width * 4),
                            (size_t)tile.width * 4);
            }
            staging->UnlockRect(0);
        }

        IDirect3DTexture9* tex = nullptr;
        if (FAILED(dev_->CreateTexture((UINT)tile.width, (UINT)tile.height, 1, 0, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, &tex, nullptr))) {
            LOG("Gc2d", "could not create a %dx%d tile texture", tile.width, tile.height);
            staging->Release();
            continue;
        }
        if (FAILED(dev_->UpdateTexture(staging, tex))) {
            LOG("Gc2d", "UpdateTexture failed for tile %zu", i);
        }
        staging->Release();
        textures_[i] = tex;
    }
    LOG("Gc2d", "uploaded %zu tile textures", textures_.size());
    return true;
}

void Renderer::Release() {
    for (auto*& t : textures_) {
        if (t != nullptr) t->Release();
        t = nullptr;
    }
    textures_.clear();
    dev_ = nullptr;
}

void Renderer::ApplyState() {
    dev_->SetVertexShader(nullptr);
    dev_->SetPixelShader(nullptr);
    dev_->SetFVF(kFvf);
    dev_->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev_->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    dev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

void Renderer::Draw(const Package& pkg, const std::vector<GcAnim::DrawNode>& nodes, int width,
                    int height) {
    if (dev_ == nullptr || textures_.empty() || nodes.empty()) return;
    ApplyState();

    const float sx = (float)width / (float)kCanvasWidth;
    const float sy = (float)height / (float)kCanvasHeight;

    std::vector<CellSegment> segments;
    for (const auto& n : nodes) {
        if (n.cell < 0 || (size_t)n.cell >= pkg.index.cells.size()) continue;
        SplitCell(pkg, pkg.index.cells[(size_t)n.cell], segments);
        ApplyBlend(dev_, n.blend);
        for (const auto& seg : segments) {
            if (seg.tile < 0 || (size_t)seg.tile >= textures_.size()) continue;
            IDirect3DTexture9* tex = textures_[(size_t)seg.tile];
            if (tex != nullptr) DrawSegment(dev_, tex, n, seg, sx, sy);
        }
    }
}
}
