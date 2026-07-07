#pragma once

#include "support/module_handle.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Support {

struct ExportNaming {
    std::string prefix;
    bool by_name = false;
};

[[nodiscard]] ExportNaming ClassifyFirstExport(std::string_view first_export_name);

[[nodiscard]] std::string FormatMangledExport(const std::string& prefix, int ordinal);

}

class DllLoader {
public:
    DllLoader() = default;

    bool Load(const char* dll_path);

    void* GetFunc(int ordinal, const char* name = nullptr);

    template <typename T> T GetFunc(int ordinal, const char* name = nullptr) {
        return reinterpret_cast<T>(GetFunc(ordinal, name));
    }

    [[nodiscard]] bool IsLoaded() const { return module_ != nullptr; }
    [[nodiscard]] const std::string& Prefix() const { return prefix_; }
    [[nodiscard]] uint32_t NumExports() const { return num_exports_; }
    [[nodiscard]] HMODULE Module() const { return module_.get(); }
    [[nodiscard]] bool IsByName() const { return by_name_; }

private:
    bool DetectPrefix();

    Support::ModuleHandle module_;
    std::string prefix_;
    std::string path_;
    uint32_t num_exports_ = 0;
    bool by_name_ = false;
};

#define DLL_LOAD(loader, field, ord) field = (loader).GetFunc<decltype(field)>(ord, #field)
