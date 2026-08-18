#pragma once

#include <cstdint>
#include <string>

namespace Panels::PresetLibrary {

enum class Action : std::uint8_t {
    None,
    New,
    Duplicate,
    Import,
    Export,
    Save,
    SaveAs,
    Revert,
    Reset,
    Properties,
    Quit,
};

bool Active();

void Render();

void RenderModals();

void RequestAction(Action action);

void SetUserRoot(const std::string& root);

}
