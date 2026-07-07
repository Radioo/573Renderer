#pragma once

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace NativeDialog {

std::string BrowseForFolder(HWND parent, const std::string& initial);

}
