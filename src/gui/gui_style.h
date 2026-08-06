#pragma once

#include "imgui.h"

#include <string>

namespace Gui {

struct Fonts {
    ImFont* base = nullptr;
    ImFont* header = nullptr;
    ImFont* mono = nullptr;
};

void PushHeaderFont();
void PushMonoFont();

void LoadFonts();

void ApplyStyle();

void ApplyAccentForProfile(const std::string& slug);

}
