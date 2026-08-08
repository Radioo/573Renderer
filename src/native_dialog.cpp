#include "native_dialog.h"

#include <cstddef>
#include <cstdint>
#include <system_error>

#include <filesystem>
#include <shellapi.h>
#include <shlobj_core.h>
#include <shobjidl_core.h>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace NativeDialog {

namespace {
std::wstring Utf8ToWide(const std::string& in) {
    if (in.empty()) return {};
    int const n = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), (int)in.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, in.c_str(), (int)in.size(), w.data(), n);
    return w;
}
}

namespace {
std::string WideToUtf8(PCWSTR in) {
    if (in == nullptr) return {};
    int const len = WideCharToMultiByte(CP_UTF8, 0, in, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s((size_t)len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, in, -1, s.data(), len, nullptr, nullptr);
    return s;
}
}

std::string BrowseForFolder(HWND parent, const std::string& initial) {
    HRESULT const hr_init =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool we_inited = SUCCEEDED(hr_init) || hr_init == RPC_E_CHANGED_MODE;

    std::string result;
    IFileOpenDialog* dlg = nullptr;
    HRESULT const hr =
        CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || (dlg == nullptr)) {
        if (we_inited) CoUninitialize();
        return {};
    }

    DWORD flags = 0;
    dlg->GetOptions(&flags);
    dlg->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(L"Select game directory");

    if (!initial.empty()) {
        std::wstring const winit = Utf8ToWide(initial);
        IShellItem* item = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(winit.c_str(), nullptr, IID_PPV_ARGS(&item))) &&
            (item != nullptr)) {
            dlg->SetFolder(item);
            item->Release();
        }
    }

    if (SUCCEEDED(dlg->Show(parent))) {
        IShellItem* picked = nullptr;
        if (SUCCEEDED(dlg->GetResult(&picked)) && (picked != nullptr)) {
            PWSTR path = nullptr;
            if (SUCCEEDED(picked->GetDisplayName(SIGDN_FILESYSPATH, &path)) && (path != nullptr)) {
                result = WideToUtf8(path);
                CoTaskMemFree(path);
            }
            picked->Release();
        }
    }

    dlg->Release();
    if (we_inited) CoUninitialize();
    return result;
}

bool RevealInFileManager(const std::string& path) {
    if (path.empty()) return false;

    std::error_code ec;
    std::filesystem::path const target = std::filesystem::absolute(path, ec);
    if (ec) return false;

    HRESULT const hr_init =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool we_inited = SUCCEEDED(hr_init) || hr_init == RPC_E_CHANGED_MODE;

    bool ok = false;
    if (std::filesystem::exists(target, ec)) {
        auto* pidl = ILCreateFromPathW(target.wstring().c_str());
        if (pidl != nullptr) {
            ok = SUCCEEDED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
            ILFree(pidl);
        }
    }

    if (!ok) {
        std::filesystem::path dir = target;
        if (dir.has_filename()) dir = dir.parent_path();
        if (!dir.empty() && std::filesystem::exists(dir, ec)) {
            HINSTANCE rc = ShellExecuteW(nullptr, L"open", dir.wstring().c_str(), nullptr, nullptr,
                                         SW_SHOWNORMAL);
            ok = reinterpret_cast<intptr_t>(rc) > 32;
        }
    }

    if (we_inited) CoUninitialize();
    return ok;
}

}
