#pragma once

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace NativeDialog {

std::string BrowseForFolder(HWND parent, const std::string& initial);

bool RevealInFileManager(const std::string& path);

struct Overrides {
    std::string (*browse_for_folder)(const std::string& initial) = nullptr;
    bool (*reveal_in_file_manager)(const std::string& path) = nullptr;
};

void SetOverrides(Overrides o);

}
