#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memory>

namespace Support {

struct ModuleCloser {
    using pointer = HMODULE;
    void operator()(HMODULE module) const noexcept { FreeLibrary(module); }
};

using ModuleHandle = std::unique_ptr<HMODULE, ModuleCloser>;

[[nodiscard]] inline ModuleHandle LoadModule(const char* dll_path) {
    return ModuleHandle(LoadLibraryA(dll_path));
}

}
