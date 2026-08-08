#include "scene3d/scene3d_render.h"

#include "scene3d/anim.h"
#include "formats/xfile.h"
#include "scene3d/scene3d.h"
#include "support/log.h"

#include <d3d9.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Scene3d {

namespace {

constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_TEX1;
constexpr float kFovY = 1.0471976F;
constexpr float kNearZ = 0.1F;
constexpr float kFarZ = 500.0F;

void ToD3D(const XFile::Matrix& src, D3DMATRIX& dst) {
    std::memcpy(&dst, src.data(), sizeof(float) * 16);
}

D3DMATRIX Perspective(float aspect) {
    const float h = 1.0F / std::tan(kFovY * 0.5F);
    const float w = h / aspect;
    D3DMATRIX m{};
    m._11 = w;
    m._22 = h;
    m._33 = kFarZ / (kFarZ - kNearZ);
    m._34 = 1.0F;
    m._43 = -kNearZ * kFarZ / (kFarZ - kNearZ);
    return m;
}

XFile::Matrix DefaultView(const Scene& scene) {
    const float cx = (scene.bounds_min[0] + scene.bounds_max[0]) * 0.5F;
    const float cy = (scene.bounds_min[1] + scene.bounds_max[1]) * 0.5F;
    const float cz = (scene.bounds_min[2] + scene.bounds_max[2]) * 0.5F;
    float radius = 0.0F;
    for (size_t k = 0; k < 3; k++)
        radius = std::max(radius, (scene.bounds_max[k] - scene.bounds_min[k]) * 0.5F);
    if (radius <= 0.0F) radius = 500.0F;

    XFile::Matrix v = XFile::Identity();
    v[12] = -cx;
    v[13] = -cy - (radius * 0.15F);
    v[14] = -cz;
    return v;
}

XFile::Matrix ViewMatrix(const Scene& scene, float time) {
    if (scene.camera_model < 0 || scene.camera_frame < 0) return DefaultView(scene);
    const auto& model = scene.models[(size_t)scene.camera_model];
    std::vector<XFile::Matrix> locals;
    std::vector<XFile::Matrix> worlds;
    SampleLocals(model.scene, time, locals);
    ComputeWorlds(model.scene, locals, worlds);
    XFile::Matrix view = XFile::Identity();
    if (!InvertAffine(worlds[(size_t)scene.camera_frame], view)) return DefaultView(scene);
    return view;
}

}

bool Renderer::Init(IDirect3DDevice9* device, const Scene& scene) {
    Release();
    dev_ = device;
    if (dev_ == nullptr) return false;

    textures_.assign(scene.tiles.size(), nullptr);
    for (size_t i = 0; i < scene.tiles.size(); i++) {
        const auto& tile = scene.tiles[i];
        IDirect3DTexture9* tex = nullptr;
        if (FAILED(dev_->CreateTexture((UINT)tile.width, (UINT)tile.height, 1, D3DUSAGE_DYNAMIC,
                                       D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr))) {
            continue;
        }
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(tex->LockRect(0, &lr, nullptr, 0))) {
            for (int row = 0; row < tile.height; row++) {
                std::memcpy(static_cast<uint8_t*>(lr.pBits) + ((size_t)row * lr.Pitch),
                            tile.bgra.data() + ((size_t)row * (size_t)tile.width * 4),
                            (size_t)tile.width * 4);
            }
            tex->UnlockRect(0);
        }
        textures_[i] = tex;
    }
    LOG("Scene3d", "renderer: uploaded %zu tile textures", textures_.size());
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

void Renderer::ApplyBaseState() {
    dev_->SetVertexShader(nullptr);
    dev_->SetPixelShader(nullptr);
    dev_->SetFVF(kFvf);
    dev_->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev_->SetRenderState(D3DRS_ZENABLE, TRUE);
    dev_->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev_->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    dev_->SetRenderState(D3DRS_ALPHAREF, 1);
    dev_->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    dev_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
}

void Renderer::ApplyBlendMode(int mode) {
    if (mode == kBlendOpaque || mode == 1) {
        dev_->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        return;
    }
    dev_->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev_->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev_->SetRenderState(D3DRS_DESTBLEND,
                         (mode == kBlendAlpha) ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE);
    dev_->SetRenderState(D3DRS_BLENDOP,
                         (mode == kBlendSubtract) ? D3DBLENDOP_REVSUBTRACT : D3DBLENDOP_ADD);
}

void Renderer::BindTile(int tile) {
    IDirect3DTexture9* tex = nullptr;
    if (tile >= 0 && (size_t)tile < textures_.size()) tex = textures_[(size_t)tile];
    dev_->SetTexture(0, tex);
    const DWORD op = (tex == nullptr) ? D3DTOP_SELECTARG2 : D3DTOP_SELECTARG1;
    const DWORD arg = (tex == nullptr) ? D3DTA_DIFFUSE : D3DTA_TEXTURE;
    dev_->SetTextureStageState(0, D3DTSS_COLOROP, op);
    dev_->SetTextureStageState(0, (tex == nullptr) ? D3DTSS_COLORARG2 : D3DTSS_COLORARG1, arg);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, op);
    dev_->SetTextureStageState(0, (tex == nullptr) ? D3DTSS_ALPHAARG2 : D3DTSS_ALPHAARG1, arg);
}

void Renderer::Draw(const Scene& scene, float model_time, float camera_time, int width, int height,
                    const XFile::Matrix* view_override) {
    draw_calls_ = 0;
    if (dev_ == nullptr || height <= 0) return;
    ApplyBaseState();

    const D3DMATRIX proj = Perspective((float)width / (float)height);
    dev_->SetTransform(D3DTS_PROJECTION, &proj);

    view_ = (view_override != nullptr) ? *view_override : ViewMatrix(scene, camera_time);
    D3DMATRIX view;
    ToD3D(view_, view);
    dev_->SetTransform(D3DTS_VIEW, &view);

    DrawPass(scene, model_time, false);
    DrawPass(scene, model_time, true);
}

void Renderer::DrawPass(const Scene& scene, float time, bool blended) {
    std::vector<XFile::Matrix> locals;
    std::vector<XFile::Matrix> worlds;
    for (const auto& model : scene.models) {
        if (!model.visible) continue;
        const bool is_blended = model.blend_mode != kBlendOpaque && model.blend_mode != 1;
        if (is_blended != blended) continue;
        ApplyBlendMode(model.blend_mode);
        SampleLocals(model.scene, time, locals);
        ComputeWorlds(model.scene, locals, worlds);
        for (const auto& chunk : model.chunks) {
            if (chunk.indices.empty()) continue;
            D3DMATRIX world;
            ToD3D(worlds[(size_t)chunk.frame], world);
            dev_->SetTransform(D3DTS_WORLD, &world);

            BindTile(chunk.tile);

            const auto verts = (UINT)(chunk.vertices.size() / kVertexFloats);
            const auto tris = (UINT)(chunk.indices.size() / 3);
            dev_->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, verts, tris, chunk.indices.data(),
                                         D3DFMT_INDEX16, chunk.vertices.data(),
                                         sizeof(float) * kVertexFloats);
            draw_calls_++;
        }
    }
}

}
