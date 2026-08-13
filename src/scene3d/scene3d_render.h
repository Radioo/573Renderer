#pragma once

#include "scene3d/scene3d.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace Scene3d {

enum class RenderStyle : uint8_t {
    TextureOnly,
    LitMaterial,
};

struct Light {
    std::array<float, 3> direction = {0.0F, 0.0F, -1.0F};
    std::array<float, 3> diffuse = {1.0F, 1.0F, 1.0F};
    std::array<float, 3> specular = {1.0F, 1.0F, 1.0F};
};

struct Projection {
    float fov_y = 1.0471976F;
    float near_z = 0.1F;
    float far_z = 500.0F;
};

class Renderer {
public:
    bool Init(IDirect3DDevice9* device, const Scene& scene);

    void Release();

    void Draw(const Scene& scene, float camera_time, int width, int height,
              const XFile::Matrix* view_override);

    void SetLights(std::vector<Light> lights) { lights_ = std::move(lights); }

    void SetProjection(const Projection& projection) { projection_ = projection; }

    void SetStyle(RenderStyle style) { style_ = style; }

    [[nodiscard]] int DrawCalls() const { return draw_calls_; }

private:
    [[nodiscard]] DWORD ColorOp() const;
    void ApplyBaseState();
    void ApplyLights(const std::vector<Light>& lights);
    void ApplyBlendMode(int mode);
    void ApplyMaterial(const DrawChunk& chunk, float alpha);
    void BindTile(int tile, int blend_mode);
    void DrawPass(const Scene& scene, bool blended);
    XFile::Matrix view_{};

    IDirect3DDevice9* dev_ = nullptr;
    std::vector<IDirect3DTexture9*> textures_;
    std::vector<Light> lights_;
    Projection projection_;
    RenderStyle style_ = RenderStyle::TextureOnly;
    DWORD active_lights_ = 0;
    int draw_calls_ = 0;
};

}
