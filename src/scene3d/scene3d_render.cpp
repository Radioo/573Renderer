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

constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

void ToD3D(const XFile::Matrix& src, D3DMATRIX& dst) {
    std::memcpy(&dst, src.data(), sizeof(float) * 16);
}

D3DMATRIX Perspective(const Projection& p, float aspect) {
    const float h = 1.0F / std::tan(p.fov_y * 0.5F);
    const float w = h / aspect;
    D3DMATRIX m{};
    m._11 = w;
    m._22 = h;
    m._33 = p.far_z / (p.far_z - p.near_z);
    m._34 = 1.0F;
    m._43 = -p.near_z * p.far_z / (p.far_z - p.near_z);
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

DWORD Renderer::ColorOp() const {
    return (style_ == RenderStyle::LitMaterial) ? D3DTOP_MODULATE : D3DTOP_SELECTARG1;
}

void Renderer::ApplyBaseState() {
    dev_->SetVertexShader(nullptr);
    dev_->SetPixelShader(nullptr);
    dev_->SetFVF(kFvf);
    dev_->SetRenderState(D3DRS_ZENABLE, D3DZB_USEW);
    dev_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    dev_->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    dev_->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    dev_->SetRenderState(D3DRS_CULLMODE,
                         (style_ == RenderStyle::LitMaterial) ? D3DCULL_CCW : D3DCULL_NONE);
    dev_->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    dev_->SetRenderState(D3DRS_DITHERENABLE, FALSE);
    dev_->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev_->SetRenderState(D3DRS_CLIPPING, TRUE);
    dev_->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev_->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    dev_->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    dev_->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    const BOOL lit = (style_ == RenderStyle::LitMaterial) ? TRUE : FALSE;
    dev_->SetRenderState(D3DRS_LIGHTING, lit);
    dev_->SetRenderState(D3DRS_AMBIENT, 0);
    dev_->SetRenderState(D3DRS_COLORVERTEX, TRUE);
    dev_->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    dev_->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    dev_->SetRenderState(D3DRS_ALPHAREF, 0);
    dev_->SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
    dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
    dev_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev_->SetTextureStageState(0, D3DTSS_COLOROP, ColorOp());
    dev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
    dev_->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    dev_->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    dev_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    dev_->SetSamplerState(0, D3DSAMP_MINFILTER,
                          (style_ == RenderStyle::LitMaterial) ? D3DTEXF_POINT : D3DTEXF_LINEAR);
    dev_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
}

void Renderer::ApplyLights(const std::vector<Light>& lights) {
    for (DWORD i = 0; i < active_lights_; i++)
        dev_->LightEnable(i, FALSE);
    active_lights_ = (DWORD)lights.size();
    for (DWORD i = 0; i < active_lights_; i++) {
        if (!lights[i].enabled) {
            dev_->LightEnable(i, FALSE);
            continue;
        }
        D3DLIGHT9 light{};
        light.Type = D3DLIGHT_DIRECTIONAL;
        light.Diffuse = {.r = lights[i].diffuse[0],
                         .g = lights[i].diffuse[1],
                         .b = lights[i].diffuse[2],
                         .a = 1.0F};
        light.Specular = {.r = lights[i].specular[0],
                          .g = lights[i].specular[1],
                          .b = lights[i].specular[2],
                          .a = 1.0F};
        light.Direction = {
            .x = lights[i].direction[0], .y = lights[i].direction[1], .z = lights[i].direction[2]};
        dev_->SetLight(i, &light);
        dev_->LightEnable(i, TRUE);
    }
}

void Renderer::ApplyBlendMode(int mode) {
    if (mode == kBlendOpaque || mode == 1) {
        dev_->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
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
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}

void Renderer::ApplyMaterial(const DrawChunk& chunk, float alpha) {
    D3DMATERIAL9 material{};
    material.Diffuse = {.r = chunk.diffuse[0],
                        .g = chunk.diffuse[1],
                        .b = chunk.diffuse[2],
                        .a = chunk.diffuse[3] * alpha};
    material.Ambient = material.Diffuse;
    material.Emissive = {
        .r = chunk.emissive[0], .g = chunk.emissive[1], .b = chunk.emissive[2], .a = 1.0F};
    dev_->SetMaterial(&material);
}

void Renderer::BindTile(int tile, int blend_mode) {
    IDirect3DTexture9* tex = nullptr;
    if (tile >= 0 && (size_t)tile < textures_.size()) tex = textures_[(size_t)tile];
    dev_->SetTexture(0, tex);
    if (tex != nullptr) {
        dev_->SetTextureStageState(0, D3DTSS_COLOROP, ColorOp());
        dev_->SetTextureStageState(
            0, D3DTSS_ALPHAOP,
            (blend_mode == kBlendOpaque || blend_mode == 1) ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE);
        return;
    }
    dev_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
}

void Renderer::Draw(const Scene& scene, float camera_time, int width, int height,
                    const XFile::Matrix* view_override) {
    draw_calls_ = 0;
    if (dev_ == nullptr || height <= 0) return;
    ApplyBaseState();
    ApplyLights(lights_);

    const float aspect =
        (projection_.aspect > 0.0F) ? projection_.aspect : ((float)width / (float)height);
    const D3DMATRIX proj = Perspective(projection_, aspect);
    dev_->SetTransform(D3DTS_PROJECTION, &proj);

    view_ = (view_override != nullptr) ? *view_override : ViewMatrix(scene, camera_time);
    D3DMATRIX view;
    ToD3D(view_, view);
    dev_->SetTransform(D3DTS_VIEW, &view);

    DrawPass(scene, false);
    DrawPass(scene, true);
}

void Renderer::DrawPass(const Scene& scene, bool blended) {
    std::vector<XFile::Matrix> locals;
    std::vector<XFile::Matrix> worlds;
    for (const auto& model : scene.models) {
        if (!model.visible) continue;
        const bool is_blended = model.blend_mode != kBlendOpaque && model.blend_mode != 1;
        if (is_blended != blended) continue;
        ApplyBlendMode(model.blend_mode);
        SampleLocals(model.scene, model.time, locals);
        ComputeWorlds(model.scene, locals, worlds);
        const XFile::Matrix placement = ModelTransform(model);
        for (const auto& chunk : model.chunks) {
            if (chunk.indices.empty()) continue;
            D3DMATRIX world;
            ToD3D(Multiply(worlds[(size_t)chunk.frame], placement), world);
            dev_->SetTransform(D3DTS_WORLD, &world);

            ApplyMaterial(chunk, model.alpha);
            BindTile(chunk.tile, model.blend_mode);

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
