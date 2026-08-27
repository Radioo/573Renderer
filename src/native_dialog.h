#pragma once

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace NativeDialog {

struct FileRequest {
    std::string title;
    std::string filter_label;
    std::string filter_pattern;
    std::string extension;
    std::string initial_dir;
    std::string initial_name;
};

std::string BrowseForFolder(HWND parent, const std::string& initial);

std::string OpenFile(HWND parent, const FileRequest& request);

std::string SaveFile(HWND parent, const FileRequest& request);

bool RevealInFileManager(const std::string& path);

struct Overrides {
    std::string (*browse_for_folder)(const std::string& initial) = nullptr;
    bool (*reveal_in_file_manager)(const std::string& path) = nullptr;
    std::string (*open_file)(const FileRequest& request) = nullptr;
    std::string (*save_file)(const FileRequest& request) = nullptr;
};

void SetOverrides(Overrides o);

}
