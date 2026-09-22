#pragma once

#include <cstdint>
#include <string>

namespace DdrClips {

struct ClipState {
    bool found = false;
    float x = 0.0F;
    float y = 0.0F;
    float origin_x = 0.0F;
    float origin_y = 0.0F;
    float alpha = 0.0F;
};

[[nodiscard]] bool Available();

[[nodiscard]] ClipState Read(const std::string& name);

void SetVisible(const std::string& name, bool visible);

bool SetBitmap(const std::string& name, const std::string& bitmap);

void ResetSprites();

[[nodiscard]] uint32_t DrawSprite(const std::string& bitmap, float x, float y, float alpha,
                                  int bm2d_priority, float origin_x = 0.0F, float origin_y = 0.0F);

void MaskSprite(uint32_t layer_id, int x, int y, int w, int h);

void BlendSprite(uint32_t layer_id, int afp_blend);

[[nodiscard]] bool HasVisibleSprites();

void DisplaySprites();

}
