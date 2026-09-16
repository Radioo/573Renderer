#include "support/stack_trace.h"

#include "support/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dbghelp.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace Support {

namespace {

constexpr unsigned kMaxNameChars = 512;

std::mutex& SymbolLock() {
    static std::mutex lock;
    return lock;
}

bool EnsureSymbols() {
    static const bool ready = [] {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }();
    return ready;
}

std::string ModuleOf(unsigned long long address) {
    const unsigned long long base = SymGetModuleBase64(GetCurrentProcess(), address);
    std::array<char, MAX_PATH + 64> out{};
    if (base == 0) {
        snprintf(out.data(), out.size(), "?+0x%llx", address);
        return out.data();
    }
    IMAGEHLP_MODULE64 info{};
    info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    const char* name = "?";
    if (SymGetModuleInfo64(GetCurrentProcess(), base, &info) != FALSE)
        name = static_cast<const char*>(info.ModuleName);
    snprintf(out.data(), out.size(), "%s+0x%llx", name, address - base);
    return out.data();
}

std::string DescribeFrame(unsigned long long address) {
    std::string where = ModuleOf(address);
    if (!EnsureSymbols()) return where;

    std::array<char, sizeof(SYMBOL_INFO) + kMaxNameChars> buffer{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = kMaxNameChars - 1;
    unsigned long long displacement = 0;
    if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol) != FALSE) {
        where += " ";
        where += static_cast<const char*>(symbol->Name);
    }

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD line_displacement = 0;
    if (SymGetLineFromAddr64(GetCurrentProcess(), address, &line_displacement, &line) != FALSE) {
        const char* file = line.FileName != nullptr ? line.FileName : "?";
        const char* leaf = file;
        for (const char* p = file; *p != 0; ++p) {
            if (*p == '\\' || *p == '/') leaf = p + 1;
        }
        std::array<char, 256> tail{};
        snprintf(tail.data(), tail.size(), " (%s:%lu)", leaf, (unsigned long)line.LineNumber);
        where += tail.data();
    }
    return where;
}

}

unsigned CaptureStackAddresses(_EXCEPTION_POINTERS* from, unsigned long long* out,
                               unsigned max_frames) {
    const std::scoped_lock held(SymbolLock());
    if (out == nullptr || max_frames == 0) return 0;

    CONTEXT context{};
    if (from != nullptr && from->ContextRecord != nullptr) {
        context = *from->ContextRecord;
    } else {
        RtlCaptureContext(&context);
    }

    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    EnsureSymbols();
    unsigned found = 0;
    while (found < max_frames) {
        if (StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &frame,
                        &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                        nullptr) == FALSE) {
            break;
        }
        if (frame.AddrPC.Offset == 0) break;
        out[found++] = frame.AddrPC.Offset;
    }
    return found;
}

unsigned ScanStackForReturns(_EXCEPTION_POINTERS* from, unsigned long long* out,
                             unsigned max_frames) {
    const std::scoped_lock held(SymbolLock());
    if (out == nullptr || max_frames == 0) return 0;
    if (from == nullptr || from->ContextRecord == nullptr) return 0;

    MEMORY_BASIC_INFORMATION region{};
    const auto stack_pointer = from->ContextRecord->Rsp;
    if (VirtualQuery(std::bit_cast<LPCVOID>(stack_pointer), &region, sizeof(region)) == 0) return 0;
    const auto region_start =
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(region.BaseAddress));
    const unsigned long long region_end = region_start + region.RegionSize;

    const unsigned long long own =
        SymGetModuleBase64(GetCurrentProcess(), std::bit_cast<unsigned long long>(&ModuleOf));
    unsigned found = 0;
    for (unsigned long long at = stack_pointer; at + 8 <= region_end && found < max_frames;
         at += 8) {
        unsigned long long candidate = 0;
        std::memcpy(&candidate, std::bit_cast<const void*>(at), sizeof(candidate));
        if (candidate < 0x10000ULL) continue;
        if (SymGetModuleBase64(GetCurrentProcess(), candidate) != own) continue;
        if (found > 0 && out[found - 1] == candidate) continue;
        out[found++] = candidate;
    }
    return found;
}

std::vector<std::string> DescribeAddresses(const unsigned long long* addresses, unsigned count) {
    const std::scoped_lock held(SymbolLock());
    std::vector<std::string> out;
    if (addresses == nullptr) return out;
    out.reserve(count);
    for (unsigned i = 0; i < count; i++)
        out.push_back(DescribeFrame(addresses[i]));
    return out;
}

std::vector<std::string> CaptureStackTrace(_EXCEPTION_POINTERS* from, unsigned max_frames) {
    std::array<unsigned long long, kMaxStackFrames> addresses{};
    const unsigned wanted = max_frames < kMaxStackFrames ? max_frames : kMaxStackFrames;
    const unsigned found = CaptureStackAddresses(from, addresses.data(), wanted);
    if (found >= 3) return DescribeAddresses(addresses.data(), found);

    std::array<unsigned long long, kMaxStackFrames> scanned{};
    const unsigned guessed = ScanStackForReturns(from, scanned.data(), wanted);
    std::vector<std::string> out = DescribeAddresses(addresses.data(), found);
    for (const std::string& frame : DescribeAddresses(scanned.data(), guessed))
        out.push_back("via " + frame);
    return out;
}

void LogStackTrace(const char* tag, const std::vector<std::string>& frames) {
    if (frames.empty()) {
        LOG(tag, "stack: unavailable");
        return;
    }
    for (std::size_t i = 0; i < frames.size(); i++)
        LOG(tag, "stack[%zu] %s", i, frames[i].c_str());
}

}
