#include "afp_ddr_clips.h"

#include "afp_ddr.h"
#include "afp_ddr_funcs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DdrClips {

namespace {

constexpr uint32_t kParamVisible = 4103;
constexpr uint32_t kParamPosition = 4104;
constexpr uint32_t kParamColour = 4106;
constexpr uint32_t kParamRegistration = 4123;
constexpr uint32_t kParamScale = 4099;
constexpr uint32_t kParamInvalidate = 4126;
constexpr int kTraversalSameNameSibling = 6;
constexpr const char* kRootPath = "/";
constexpr std::size_t kMostSprites = 1024;

struct PooledSprite {
    uint32_t layer = 0;
    uint32_t mc = 0;
    int priority = 0;
};
std::vector<PooledSprite> g_sprite_pool;
std::size_t g_sprites_used = 0;
constexpr int kMostSameNameSiblings = 64;
constexpr int kBm2dPriorityPivot = 100;
constexpr int kAttributeTransform = 0x1000;
constexpr float kAlphaScale = 100.0F;
constexpr std::size_t kParamSlots = 32;
constexpr std::size_t kAlphaSlot = 0;

uint32_t ReferClip(const std::string& name) {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    const uint32_t layer = DdrAfp::LayerId();
    if (layer == 0U || afp.afp_layer_mc_refer == nullptr) return 0;
    const int direct = afp.afp_layer_mc_refer(layer, name.c_str());
    if (direct > 0) return static_cast<uint32_t>(direct);
    if (afp.afp_mc_search == nullptr) return 0;
    const int root = afp.afp_layer_mc_refer(layer, kRootPath);
    if (root <= 0) return 0;
    const int found = afp.afp_mc_search(static_cast<uint32_t>(root), name.c_str());
    return (found <= 0) ? 0 : static_cast<uint32_t>(found);
}

void ApplyTransform(const AfpDdrFuncs& afp, uint32_t layer) {
    if (afp.afp_layer_set_attribute != nullptr) {
        afp.afp_layer_set_attribute(layer, kAttributeTransform, kAttributeTransform);
    }
}

int AfpPriority(int bm2d_priority) {
    if (bm2d_priority > kBm2dPriorityPivot) return bm2d_priority;
    return std::abs(bm2d_priority - kBm2dPriorityPivot);
}

}

bool Available() {
    return DdrAfp::Funcs().HasClipControlApi() && DdrAfp::LayerId() != 0U;
}

ClipState Read(const std::string& name) {
    ClipState out;
    const uint32_t mc = ReferClip(name);
    if (mc == 0U) return out;

    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    std::array<float, kParamSlots> position{};
    std::array<float, kParamSlots> registration{};
    std::array<float, kParamSlots> colour{};
    afp.afp_mc_get_param(mc, kParamPosition, position.data(), nullptr);
    afp.afp_mc_get_param(mc, kParamRegistration, registration.data(), nullptr);
    afp.afp_mc_get_param(mc, kParamColour, colour.data(), nullptr);

    out.found = true;
    out.x = position[0];
    out.y = position[1];
    out.origin_x = std::trunc(registration[0]);
    out.origin_y = std::trunc(registration[1]);
    out.alpha = colour[kAlphaSlot] * kAlphaScale;
    return out;
}

void SetVisible(const std::string& name, bool visible) {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    int mc = static_cast<int>(ReferClip(name));
    for (int i = 0; mc > 0 && i < kMostSameNameSiblings; i++) {
        afp.afp_mc_set_param(static_cast<uint32_t>(mc), kParamVisible, visible ? 1 : 0);
        afp.afp_mc_set_param(static_cast<uint32_t>(mc), kParamInvalidate, 1);
        const int next = afp.afp_mc_traversal(mc, kTraversalSameNameSibling);
        mc = (next == mc) ? 0 : next;
    }
}

bool SetBitmap(const std::string& name, const std::string& bitmap) {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    if (afp.afp_mc_load_bitmap == nullptr) return false;
    int mc = static_cast<int>(ReferClip(name));
    bool any = false;
    for (int i = 0; mc > 0 && i < kMostSameNameSiblings; i++) {
        any = afp.afp_mc_load_bitmap(static_cast<uint32_t>(mc), bitmap.c_str()) >= 0 || any;
        afp.afp_mc_set_param(static_cast<uint32_t>(mc), kParamInvalidate, 1);
        const int next = afp.afp_mc_traversal(mc, kTraversalSameNameSibling);
        mc = (next == mc) ? 0 : next;
    }
    return any;
}

void ResetSprites() {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    if (afp.afp_layer_set_attribute != nullptr) {
        for (const PooledSprite& sprite : g_sprite_pool)
            afp.afp_layer_set_attribute(sprite.layer, 1, 0);
    }
    g_sprites_used = 0;
}

uint32_t DrawSprite(const std::string& bitmap, float x, float y, float alpha, int bm2d_priority,
                    float origin_x, float origin_y) {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    if (!afp.HasSpriteOverlayApi()) return 0;

    uint32_t layer = 0;
    if (g_sprites_used < g_sprite_pool.size()) {
        layer = g_sprite_pool[g_sprites_used].layer;
        if (afp.afp_layer_change_sprite != nullptr) {
            afp.afp_layer_change_sprite(layer, bitmap.c_str());
        }
    } else {
        if (g_sprite_pool.size() >= kMostSprites) return 0;
        layer = afp.afp_sprite_layer_create(bitmap.c_str(), 0);
        const int mc = (layer == 0U) ? -1 : afp.afp_layer_mc_refer(layer, kRootPath);
        if (layer == 0U) return 0;
        g_sprite_pool.push_back(
            {.layer = layer, .mc = (mc > 0) ? static_cast<uint32_t>(mc) : 0U, .priority = 0});
    }
    const int priority = AfpPriority(bm2d_priority);
    g_sprite_pool[g_sprites_used].priority = priority;
    g_sprites_used++;

    afp.afp_layer_set_priority(layer, priority);
    if (afp.afp_layer_set_attribute != nullptr) afp.afp_layer_set_attribute(layer, 1, 1);
    const std::array<float, 2> xy = {x, y};
    afp.afp_layer_set_position(layer, xy.data());
    if (afp.afp_layer_set_color != nullptr) {
        afp.afp_layer_set_color(layer, 1.0F, 1.0F, 1.0F, alpha / kAlphaScale);
    }
    const uint32_t mc = g_sprite_pool[g_sprites_used - 1].mc;
    if (mc != 0U) {
        const std::array<float, 2> origin = {origin_x, origin_y};
        const std::array<float, 2> scale = {1.0F, 1.0F};
        afp.afp_mc_set_param(mc, kParamRegistration, origin.data());
        ApplyTransform(afp, layer);
        afp.afp_mc_set_param(mc, kParamScale, scale.data());
        ApplyTransform(afp, layer);
    }
    return layer;
}

bool HasVisibleSprites() {
    return g_sprites_used != 0;
}

void DisplaySprites() {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    if (g_sprites_used == 0) return;
    std::vector<PooledSprite> order(
        g_sprite_pool.begin(), g_sprite_pool.begin() + static_cast<std::ptrdiff_t>(g_sprites_used));
    std::ranges::stable_sort(order, [](const PooledSprite& a, const PooledSprite& b) {
        return a.priority < b.priority;
    });
    for (const PooledSprite& sprite : order)
        afp.DisplayLayer(sprite.layer);
}

void MaskSprite(uint32_t layer_id, int x, int y, int w, int h) {
    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    if (layer_id == 0U || afp.afp_layer_set_mask == nullptr) return;
    afp.afp_layer_set_mask(layer_id, x, y, w, h);
}

}
