#include "support/dll_loader.h"

#include "support/log.h"
#include "support/module_handle.h"

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace Support {

namespace {

bool IsHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

}

ExportNaming ClassifyFirstExport(std::string_view first_export_name) {
    constexpr std::size_t kOrdinalDigits = 6;
    ExportNaming out;
    if (first_export_name.size() < kOrdinalDigits + 1) {
        out.by_name = true;
        return out;
    }
    const std::string_view tail =
        first_export_name.substr(first_export_name.size() - kOrdinalDigits);
    for (const char c : tail) {
        if (!IsHexDigit(c)) {
            out.by_name = true;
            return out;
        }
    }
    out.prefix = first_export_name.substr(0, first_export_name.size() - kOrdinalDigits);
    out.by_name = false;
    return out;
}

std::string FormatMangledExport(const std::string& prefix, int ordinal) {
    return std::format("{}{:06x}", prefix, static_cast<unsigned>(ordinal));
}

}

bool DllLoader::Load(const char* dll_path) {
    path_ = dll_path;
    module_ = Support::LoadModule(dll_path);
    if (module_ == nullptr) {
        LOG("DllLoader", "Failed to load %s (error %lu)", dll_path, GetLastError());
        return false;
    }

    if (!DetectPrefix()) {
        LOG("DllLoader", "Failed to detect export prefix for %s", dll_path);
        module_.reset();
        return false;
    }

    if (by_name_) {
        LOG("DllLoader", "Loaded %s (readable exports, count: %u)", dll_path, num_exports_);
    } else {
        LOG("DllLoader", "Loaded %s (prefix: %s, exports: %u)", dll_path, prefix_.c_str(),
            num_exports_);
    }
    return true;
}

bool DllLoader::DetectPrefix() {
    const auto* base = reinterpret_cast<const BYTE*>(module_.get());
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& export_dir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (export_dir.VirtualAddress == 0 || export_dir.Size == 0) return false;

    const auto* exports =
        reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + export_dir.VirtualAddress);
    if (exports->NumberOfNames == 0) return false;

    const auto* names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
    const auto* first_name = reinterpret_cast<const char*>(base + names[0]);

    num_exports_ = exports->NumberOfNames;

    const Support::ExportNaming naming = Support::ClassifyFirstExport(first_name);
    prefix_ = naming.prefix;
    by_name_ = naming.by_name;
    return true;
}

namespace {

FARPROC ResolveProc(HMODULE module, bool by_name, const std::string& prefix, int ordinal,
                    const char* name) {
    if (by_name) {
        return name != nullptr ? GetProcAddress(module, name) : nullptr;
    }
    const std::string sym = Support::FormatMangledExport(prefix, ordinal);
    return GetProcAddress(module, sym.c_str());
}

}

void* DllLoader::GetFunc(int ordinal, const char* name) {
    if (module_ == nullptr) return nullptr;

    const auto proc = ResolveProc(module_.get(), by_name_, prefix_, ordinal, name);
    if (proc == nullptr && name != nullptr) {
        LOG("DllLoader", "WARNING: Could not resolve %s (ord 0x%x, by_name=%d) in %s", name,
            ordinal, static_cast<int>(by_name_), path_.c_str());
    }
    return reinterpret_cast<void*>(proc);
}
