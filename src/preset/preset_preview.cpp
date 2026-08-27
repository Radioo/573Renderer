#include "preset/preset_preview.h"

#include "app_globals.h"
#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "render_backend.h"

#include <d3d9.h>

#include "support/log.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Preset::Preview {

namespace {

constexpr int kThumbWidth = 160;
constexpr std::size_t kCacheEntries = 8;

struct Target {
    IDirect3DTexture9* texture = nullptr;
    IDirect3DSurface9* surface = nullptr;
    IDirect3DSurface9* readback = nullptr;
    int width = 0;
    int height = 0;
};

std::mutex g_lock;
SnapshotPtr g_published;
Request g_request;
int g_next = 0;
unsigned g_version = 0;
bool g_release_pending = false;
Cache g_cache(kCacheEntries);
Target g_target;

void ReleaseTarget() {
    if (g_target.readback != nullptr) g_target.readback->Release();
    if (g_target.surface != nullptr) g_target.surface->Release();
    if (g_target.texture != nullptr) g_target.texture->Release();
    g_target = {};
}

bool EnsureTarget(IDirect3DDevice9* device, int width, int height) {
    if (g_target.surface != nullptr && g_target.width == width && g_target.height == height) {
        return true;
    }
    ReleaseTarget();
    if (FAILED(device->CreateTexture((UINT)width, (UINT)height, 1, D3DUSAGE_RENDERTARGET,
                                     D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &g_target.texture,
                                     nullptr))) {
        LOG("Preview", "could not create a %dx%d preview target", width, height);
        return false;
    }
    if (FAILED(g_target.texture->GetSurfaceLevel(0, &g_target.surface)) ||
        FAILED(device->CreateOffscreenPlainSurface((UINT)width, (UINT)height, D3DFMT_X8R8G8B8,
                                                   D3DPOOL_SYSTEMMEM, &g_target.readback,
                                                   nullptr))) {
        ReleaseTarget();
        return false;
    }
    g_target.width = width;
    g_target.height = height;
    return true;
}

int SampleFrame(const Request& request, int index) {
    if (request.length <= 1 || request.samples <= 1) return 0;
    return ((request.length - 1) * index) / (request.samples - 1);
}

void DrawSample(const Request& request, int frame) {
    Gc2dHost::SpritePlacement placement;
    placement.asset = request.asset;
    placement.name = request.animation;
    placement.animated = true;
    placement.timing.playback = GcAnim::Playback::HoldLast;
    placement.skip_parts = request.hidden_parts;
    std::vector<Gc2dHost::SpritePlacement> one;
    one.push_back(std::move(placement));
    Gc2dHost::SetSprites(std::move(one));
    Gc2dHost::SetSpriteFrame(0, frame);
    Gc2dHost::DrawSprites(INT_MIN, INT_MAX);
}

Sample Downsample(const uint8_t* pixels, int pitch, int width, int height) {
    Sample sample;
    sample.width = std::min(kThumbWidth, width);
    sample.height = std::max(1, (height * sample.width) / std::max(1, width));
    sample.bgra.assign((std::size_t)sample.width * (std::size_t)sample.height * 4, 0);
    for (int y = 0; y < sample.height; y++) {
        const int source_y = (y * height) / sample.height;
        const uint8_t* row = pixels + ((std::size_t)source_y * (std::size_t)pitch);
        for (int x = 0; x < sample.width; x++) {
            const int source_x = (x * width) / sample.width;
            const std::size_t to =
                (((std::size_t)y * (std::size_t)sample.width) + (std::size_t)x) * 4;
            const std::size_t from = (std::size_t)source_x * 4;
            sample.bgra[to] = row[from];
            sample.bgra[to + 1] = row[from + 1];
            sample.bgra[to + 2] = row[from + 2];
            sample.bgra[to + 3] = 0xFF;
        }
    }
    return sample;
}

bool Capture(IDirect3DDevice9* device, Sample& out) {
    if (FAILED(device->GetRenderTargetData(g_target.surface, g_target.readback))) return false;
    D3DLOCKED_RECT locked = {};
    if (FAILED(g_target.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return false;
    out = Downsample((const uint8_t*)locked.pBits, locked.Pitch, g_target.width, g_target.height);
    g_target.readback->UnlockRect();
    return true;
}

bool TakePendingRelease() {
    const std::scoped_lock guard(g_lock);
    if (!g_release_pending) return false;
    g_release_pending = false;
    return true;
}

bool NextSample(Request& request, int& index) {
    const std::scoped_lock guard(g_lock);
    if (g_request.key.empty() || g_next >= std::max(1, g_request.samples)) return false;
    request = g_request;
    index = g_next;
    return true;
}

void RenderTo(IDirect3DDevice9* device, const Request& request, int index) {
    IDirect3DSurface9* saved_target = nullptr;
    IDirect3DSurface9* saved_depth = nullptr;
    device->GetRenderTarget(0, &saved_target);
    device->GetDepthStencilSurface(&saved_depth);
    device->SetRenderTarget(0, g_target.surface);
    device->SetDepthStencilSurface(nullptr);
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF101014, 1.0F, 0);
    if (SUCCEEDED(device->BeginScene())) {
        DrawSample(request, SampleFrame(request, index));
        device->EndScene();
    }
    if (saved_target != nullptr) {
        device->SetRenderTarget(0, saved_target);
        saved_target->Release();
    }
    device->SetDepthStencilSurface(saved_depth);
    if (saved_depth != nullptr) saved_depth->Release();
}

void PublishSample(const std::string& key, Sample sample) {
    const std::scoped_lock guard(g_lock);
    if (g_request.key != key || g_published == nullptr) return;
    auto next = std::make_shared<Snapshot>(*g_published);
    next->samples.push_back(std::move(sample));
    next->version = ++g_version;
    g_published = next;
    g_next++;
    if (g_next >= std::max(1, g_request.samples)) g_cache.Insert(key, g_published);
}

}

SnapshotPtr Cache::Find(const std::string& key) {
    const auto it =
        std::ranges::find_if(entries_, [&key](const auto& entry) { return entry.first == key; });
    if (it == entries_.end()) return nullptr;
    auto found = std::move(*it);
    entries_.erase(it);
    SnapshotPtr snapshot = found.second;
    entries_.insert(entries_.begin(), std::move(found));
    return snapshot;
}

void Cache::Insert(const std::string& key, SnapshotPtr snapshot) {
    std::erase_if(entries_, [&key](const auto& entry) { return entry.first == key; });
    entries_.insert(entries_.begin(), std::pair{key, std::move(snapshot)});
    if (entries_.size() > capacity_) entries_.resize(capacity_);
}

void Cache::Clear() {
    entries_.clear();
}

std::string KeyFor(const std::string& asset, const std::string& animation,
                   const std::vector<std::string>& hidden_parts) {
    std::string key = asset + "/" + animation;
    for (const std::string& part : hidden_parts)
        key += "|" + part;
    return key;
}

void Post(Request request) {
    const std::scoped_lock guard(g_lock);
    if (request.key == g_request.key) return;
    g_request = std::move(request);
    const SnapshotPtr cached = g_cache.Find(g_request.key);
    if (cached != nullptr) {
        g_published = cached;
        g_next = std::max(1, g_request.samples);
        return;
    }
    auto fresh = std::make_shared<Snapshot>();
    fresh->key = g_request.key;
    fresh->total = std::max(1, g_request.samples);
    fresh->version = ++g_version;
    g_published = fresh;
    g_next = 0;
}

void Reset() {
    const std::scoped_lock guard(g_lock);
    g_request = {};
    g_next = 0;
    g_published = nullptr;
    g_cache.Clear();
    g_release_pending = true;
}

bool Pump() {
    if (TakePendingRelease()) ReleaseTarget();

    Request request;
    int index = 0;
    if (!NextSample(request, index)) return false;

    IDirect3DDevice9* device = g_d3d.device;
    if (device == nullptr) return false;
    if (!EnsureTarget(device, g_d3d.width, g_d3d.height)) return false;

    RenderTo(device, request, index);

    Sample sample;
    if (!Capture(device, sample)) return true;
    PublishSample(request.key, std::move(sample));
    return true;
}

SnapshotPtr Get() {
    const std::scoped_lock guard(g_lock);
    return g_published;
}

}
