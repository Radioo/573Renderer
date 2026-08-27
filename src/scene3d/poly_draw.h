#pragma once

#include "media/movie_source.h"
#include "scene3d/poly_grid.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <string>

namespace Scene3d {

class PolyDraw {
public:
    void Init(IDirect3DDevice9* device);

    void Release();

    void SetGrid(PolyGrid grid);

    void SetMovieReporter(MovieReporter reporter);

    void Draw(bool lit);

    [[nodiscard]] int DrawCalls() const { return draw_calls_; }

private:
    IDirect3DTexture9* Texture();
    void OpenMovie();
    Movie::Frame Fetch(int index);

    IDirect3DDevice9* dev_ = nullptr;
    IDirect3DTexture9* texture_ = nullptr;
    int texture_side_ = 0;
    int uploaded_ = -1;
    Movie::Source movie_;
    MovieReporter reporter_;
    std::string wanted_;
    bool refused_ = false;
    PolyGrid grid_;
    int draw_calls_ = 0;
};

}
