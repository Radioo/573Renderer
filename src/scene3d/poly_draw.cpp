#include "scene3d/poly_draw.h"

#include "media/movie_source.h"
#include "scene3d/poly_grid.h"
#include "scene3d/poly_vertex.h"
#include "support/log.h"

#include <d3d9.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace Scene3d {

namespace {

static_assert(kPolyFvf == (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1));

constexpr int kQuietDecodeSteps = 8;
constexpr int kReportEverySteps = 16;

}

void PolyDraw::Init(IDirect3DDevice9* device) {
    Release();
    dev_ = device;
}

void PolyDraw::Release() {
    if (texture_ != nullptr) texture_->Release();
    texture_ = nullptr;
    texture_side_ = 0;
    uploaded_ = -1;
    movie_.Close();
    wanted_.clear();
    refused_ = false;
    grid_ = PolyGrid{};
    dev_ = nullptr;
}

void PolyDraw::SetGrid(PolyGrid grid) {
    grid_ = std::move(grid);
}

void PolyDraw::SetMovieReporter(MovieReporter reporter) {
    reporter_ = std::move(reporter);
}

void PolyDraw::OpenMovie() {
    if (grid_.movie == wanted_) return;
    wanted_ = grid_.movie;
    refused_ = false;
    uploaded_ = -1;
    movie_.Close();
    if (wanted_.empty()) return;
    if (!movie_.Open(wanted_)) {
        LOG("Scene3d", "poly tiles draw untextured: %s", movie_.LastError().c_str());
        refused_ = true;
        return;
    }
    movie_.SetOutputSize(std::max(1, grid_.movie_width), std::max(1, grid_.movie_height));
    LOG("Scene3d", "poly tile movie %s: %dx%d at %.3f fps", wanted_.c_str(), grid_.movie_width,
        grid_.movie_height, movie_.FrameRate());
}

Movie::Frame PolyDraw::Fetch(int index) {
    int steps = 0;
    bool loading = false;
    const Movie::Frame frame = movie_.FrameAt(index, [&](int decoded, int total) {
        steps++;
        if (steps <= kQuietDecodeSteps) return;
        if (!loading) {
            loading = true;
            if (reporter_.begin) reporter_.begin(wanted_);
        }
        if (steps % kReportEverySteps != 0) return;
        const float fraction = (total > 0) ? ((float)decoded / (float)total) : -1.0F;
        if (reporter_.stage) {
            reporter_.stage("Replaying the tile movie, frame " + std::to_string(decoded) + " / " +
                                std::to_string(total),
                            fraction);
        }
    });
    if (loading && reporter_.end) reporter_.end();
    return frame;
}

IDirect3DTexture9* PolyDraw::Texture() {
    OpenMovie();
    if (refused_ || !movie_.IsOpen() || dev_ == nullptr) return nullptr;
    const int side = std::max(1, grid_.texture_side);
    if (texture_ != nullptr && texture_side_ != side) {
        texture_->Release();
        texture_ = nullptr;
    }
    if (texture_ == nullptr) {
        if (FAILED(dev_->CreateTexture((UINT)side, (UINT)side, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, &texture_, nullptr))) {
            LOG("Scene3d", "poly tiles draw untextured: no %dx%d texture", side, side);
            refused_ = true;
            return nullptr;
        }
        texture_side_ = side;
        uploaded_ = -1;
    }
    const Movie::Frame frame = Fetch(movie_.IndexAt((double)grid_.seconds));
    if (movie_.Broken()) {
        LOG("Scene3d", "poly tiles draw untextured: %s cannot be scaled to %dx%d", wanted_.c_str(),
            grid_.movie_width, grid_.movie_height);
        refused_ = true;
        return nullptr;
    }
    const int held = movie_.Position();
    if (frame.bgra == nullptr || held == uploaded_) return texture_;
    D3DLOCKED_RECT locked;
    if (FAILED(texture_->LockRect(0, &locked, nullptr, 0))) return texture_;
    uploaded_ = held;
    const int rows = std::min(frame.height, side);
    const auto bytes = (std::size_t)std::min(frame.width, side) * 4U;
    for (int row = 0; row < rows; row++) {
        std::memcpy(static_cast<std::uint8_t*>(locked.pBits) + ((std::size_t)row * locked.Pitch),
                    frame.bgra + ((std::size_t)row * (std::size_t)frame.width * 4U), bytes);
    }
    texture_->UnlockRect(0);
    return texture_;
}

void PolyDraw::Draw(bool lit) {
    draw_calls_ = 0;
    if (dev_ == nullptr || !grid_.active || grid_.tiles.empty()) return;

    IDirect3DTexture9* texture = Texture();
    const std::uint32_t diffuse = DiffuseOf(grid_.alpha);

    D3DMATRIX world{};
    world._11 = 1.0F;
    world._22 = 1.0F;
    world._33 = 1.0F;
    world._44 = 1.0F;
    dev_->SetTransform(D3DTS_WORLD, &world);
    dev_->SetFVF(kPolyFvf);
    dev_->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    dev_->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev_->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev_->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
    dev_->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev_->SetTexture(0, texture);
    dev_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev_->SetTextureStageState(0, D3DTSS_COLOROP,
                               (texture != nullptr) ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
    dev_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    for (const PolyTile& tile : grid_.tiles) {
        const std::array<PolyVertex, 4> strip = StripOf(tile, diffuse);
        dev_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, strip.data(), sizeof(PolyVertex));
        draw_calls_++;
    }

    dev_->SetTexture(0, nullptr);
    dev_->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    dev_->SetRenderState(D3DRS_LIGHTING, lit ? TRUE : FALSE);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
}

}
