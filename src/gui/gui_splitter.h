#pragma once

namespace Gui {

bool VSplitter(const char* id, float width, float height, float* left_w, float* right_w,
               float left_min, float right_min);

bool HSplitter(const char* id, float width, float height, float* top_h, float* bottom_h,
               float top_min, float bottom_min, float bottom_default);

}
