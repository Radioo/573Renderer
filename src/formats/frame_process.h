#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Frame {

void CompositeOverOpaqueBg(std::span<uint8_t> bgra, float r, float g, float b);

struct CropSpec {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

[[nodiscard]] CropSpec ClampCropToImage(CropSpec c, int img_w, int img_h);

void CopyCropRegion(std::span<const uint8_t> bgra, int img_w, CropSpec c,
                    std::vector<uint8_t>& out);

}
