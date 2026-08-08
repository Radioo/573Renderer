#include "support/crash_report.h"

#include "support/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdint>

namespace Support {
namespace {

bool IsFatal(DWORD code) {
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
           code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
           code == EXCEPTION_STACK_OVERFLOW;
}

const char* ModuleBaseName(std::array<char, MAX_PATH>& path, const void* addr, HMODULE& out) {
    out = nullptr;
    path[0] = '\0';
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(addr), &out);
    if (out != nullptr) GetModuleFileNameA(out, path.data(), MAX_PATH);
    const char* base = path.data();
    for (const char* p = path.data(); *p != 0; ++p) {
        if (*p == '\\' || *p == '/') base = p + 1;
    }
    return (base[0] != 0) ? base : "?";
}

LONG CALLBACK CrashHandler(PEXCEPTION_POINTERS ep) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (!IsFatal(code)) return EXCEPTION_CONTINUE_SEARCH;

    const void* addr = ep->ExceptionRecord->ExceptionAddress;
    std::array<char, MAX_PATH> path = {};
    HMODULE module = nullptr;
    const char* base = ModuleBaseName(path, addr, module);
    const auto module_addr = reinterpret_cast<uintptr_t>(module);
    const auto fault_addr = reinterpret_cast<uintptr_t>(addr);
    const auto off = (module != nullptr) ? (unsigned long long)(fault_addr - module_addr) : 0ULL;
    const auto data = (code == EXCEPTION_ACCESS_VIOLATION)
                          ? (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]
                          : 0ULL;
    LOG("CRASH", "code=0x%08lx at 0x%llx module=%s base=0x%llx off=0x%llx data=0x%llx", code,
        (unsigned long long)fault_addr, base, (unsigned long long)module_addr, off, data);
    return EXCEPTION_CONTINUE_SEARCH;
}

}

void InstallCrashReporter() {
    static void* const handle = AddVectoredExceptionHandler(1, CrashHandler);
    (void)handle;
}

}
