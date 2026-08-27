#pragma once

#include "imgui.h"

struct HWND__;

namespace Gui::Dpi {

void MakeThreadPerMonitorAware();

unsigned DpiForWindow(HWND__* hwnd);

void SetScaleFromDpi(unsigned dpi);

float Scale();

inline float S(float dips) {
    return dips * Scale();
}

inline ImVec2 S(float x_dips, float y_dips) {
    return ImVec2{x_dips * Scale(), y_dips * Scale()};
}

}
