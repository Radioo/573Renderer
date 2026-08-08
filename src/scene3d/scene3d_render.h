#pragma once

#include "scene3d/scene3d.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <vector>

namespace Scene3d {

class Renderer {
public:
    bool Init(IDirect3DDevice9* device, const Scene& scene);

    void Release();

    void Draw(const Scene& scene, float model_time, float camera_time, int width, int height,
              const XFile::Matrix* view_override);

    [[nodiscard]] int DrawCalls() const { return draw_calls_; }

private:
    void ApplyBaseState();
    void ApplyBlendMode(int mode);
    void BindTile(int tile);
    void DrawPass(const Scene& scene, float time, bool blended);
    XFile::Matrix view_{};

    IDirect3DDevice9* dev_ = nullptr;
    std::vector<IDirect3DTexture9*> textures_;
    int draw_calls_ = 0;
};

}
