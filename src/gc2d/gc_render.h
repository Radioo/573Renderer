#pragma once

#include "formats/gcanim.h"
#include "gc2d/gc_package.h"
#include "gc2d/gc_sprite.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <vector>

namespace Gc2d {

class Renderer {
public:
    bool Init(IDirect3DDevice9* device, const Package& pkg);

    void Release();

    void Draw(const Package& pkg, const std::vector<GcAnim::DrawNode>& nodes, int width, int height,
              const Canvas& canvas);

private:
    void ApplyState();

    IDirect3DDevice9* dev_ = nullptr;
    std::vector<IDirect3DTexture9*> textures_;
};

}
