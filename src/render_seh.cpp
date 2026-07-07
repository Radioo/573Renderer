#include <cstdint>
#include <excpt.h>
#include "afp_funcs.h"
#include "render_seh.h"
#include "support/log.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace RenderSeh {

namespace {

const char* const kRegNames[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                   " r8", " r9", "r10", "r11", "r12", "r13", "r14", "r15"};

struct ModuleSpan {
    const char* name;
    uintptr_t base;
};

ModuleSpan ResolveModule(uintptr_t addr) {
    const struct {
        const char* n;
        HMODULE h;
    } mods[] = {
        {.n = "573Renderer.exe", .h = GetModuleHandleA(nullptr)},
        {.n = "afp-core", .h = GetModuleHandleA("afp-core.dll")},
        {.n = "afp-utils", .h = GetModuleHandleA("afp-utils.dll")},
    };
    for (const auto& m : mods) {
        const auto b = reinterpret_cast<uintptr_t>(m.h);
        if (b != 0 && addr >= b && addr < b + 0x800000) return {.name = m.n, .base = b};
    }
    return {.name = "?", .base = 0};
}

void ScreamRenderFault(const char* what, const FaultReport& r) {
    static int hits = 0;
    ++hits;
    if (hits > 3 && (hits % 600) != 0) return;
    const ModuleSpan mod = ResolveModule(r.pc);
    const uintptr_t off = mod.base != 0 ? r.pc - mod.base : r.pc;
    LOG("RenderSeh", "############################################################");
    LOG("RenderSeh", "## CAUGHT CRASH in %s (code=0x%08lx, hit #%d)", what, (unsigned long)r.code,
        hits);
    LOG("RenderSeh", "## an afp callback FAULTED - render state is silently corrupted.");
    LOG("RenderSeh", "## REVERSE the faulting path and fix it ASAP.");
    LOG("RenderSeh", "## fault pc=%s+0x%llx op=%lu target=0x%llx", mod.name,
        (unsigned long long)off, (unsigned long)r.op, (unsigned long long)r.target);
    LOG("RenderSeh", "############################################################");
}

int CaptureFault(EXCEPTION_POINTERS* ep, FaultReport* out) {
    if (ep != nullptr && ep->ExceptionRecord != nullptr) {
        out->pc = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
        if (ep->ExceptionRecord->NumberParameters >= 2) {
            out->op = static_cast<DWORD>(ep->ExceptionRecord->ExceptionInformation[0]);
            out->target = static_cast<uintptr_t>(ep->ExceptionRecord->ExceptionInformation[1]);
        }
        if (ep->ContextRecord != nullptr) {
            const CONTEXT* c = ep->ContextRecord;
            const uint64_t regs[16] = {c->Rax, c->Rcx, c->Rdx, c->Rbx, c->Rsp, c->Rbp,
                                       c->Rsi, c->Rdi, c->R8,  c->R9,  c->R10, c->R11,
                                       c->R12, c->R13, c->R14, c->R15};
            for (int i = 0; i < 16; i++)
                out->regs[i] = regs[i];
            out->pc = c->Rip;
        }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

}

FaultReport SafeCallUpdate(afp_do_update_t fn, float dt) {
    FaultReport report{};
    __try {
        fn(dt, 3, 0);
    } __except (CaptureFault(GetExceptionInformation(), &report)) {
        report.faulted = true;
        report.code = static_cast<DWORD>(GetExceptionCode());
        ScreamRenderFault("afp_do_update", report);
    }
    return report;
}

FaultReport SafeCallSortRender(afp_do_sort_render_t fn) {
    FaultReport report{};
    __try {
        fn(1, 0);
    } __except (CaptureFault(GetExceptionInformation(), &report)) {
        report.faulted = true;
        report.code = static_cast<DWORD>(GetExceptionCode());
        ScreamRenderFault("afp_do_sort_render", report);
    }
    return report;
}

void LogFault(const char* what, int frame, const FaultReport& report) {
    const ModuleSpan pc_mod = ResolveModule(report.pc);
    const uintptr_t pc_off = pc_mod.base != 0 ? report.pc - pc_mod.base : report.pc;
    LOG("AFP", "%s threw 0x%08lx at frame %d, pc=%s+0x%llx target=0x%llx op=%lu", what,
        (unsigned long)report.code, frame, pc_mod.name, (unsigned long long)pc_off,
        (unsigned long long)report.target, (unsigned long)report.op);

    const auto afpu_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("afp-utils.dll"));
    const auto afp_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("afp-core.dll"));
    LOG("AFP", "  afp-utils base=%p, afp-core base=%p", reinterpret_cast<void*>(afpu_base),
        reinterpret_cast<void*>(afp_base));
    for (int i = 0; i < 16; i++) {
        const uint64_t v = report.regs[i];
        char tag[64] = {};
        if (afpu_base != 0 && v >= afpu_base && v < afpu_base + 0x200000) {
            snprintf(tag, sizeof(tag), " (afpu+0x%llx)", (unsigned long long)(v - afpu_base));
        } else if (afp_base != 0 && v >= afp_base && v < afp_base + 0x200000) {
            snprintf(tag, sizeof(tag), " (afp+0x%llx)", (unsigned long long)(v - afp_base));
        }
        LOG("AFP", "  %s = 0x%016llx%s", kRegNames[i], (unsigned long long)v, tag);
    }
}

int SafeEnumChildNames(afp_mc_enumerate_children_t fn, uint32_t parent_mc, int flags,
                       char (*names)[128], int max_names, DWORD* code) {
    static thread_local unsigned char buf[4096];
    void* base = nullptr;
    __try {
        int const rc = fn(parent_mc, buf, sizeof(buf), flags, &base);
        if ((rc != 0 && std::cmp_not_equal(rc, 0xFFFFFFFB)) || (base == nullptr)) return 0;
        const auto* b = static_cast<const unsigned char*>(base);
        unsigned const written = *reinterpret_cast<const unsigned short*>(b + 0);
        unsigned const total = *reinterpret_cast<const unsigned short*>(b + 2);
        int n = (int)(written < total ? written : total);
        n = std::min(n, max_names);
        int out = 0;
        for (int i = 0; i < n; ++i) {
            const char* nm = *reinterpret_cast<const char* const*>(b + 8 + (8 * (size_t)i));
            int j = 0;
            if (nm != nullptr) {
                for (; j < 127 && (nm[j] != 0); ++j)
                    names[out][j] = nm[j];
            }
            names[out][j] = 0;
            out++;
        }
        return out;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (code != nullptr) *code = (DWORD)GetExceptionCode();
        return -1;
    }
}

int SafeGetIdByPath(afp_mc_get_id_by_path_t fn, uint32_t stream_id, const char* path, DWORD* code) {
    __try {
        return fn(stream_id, path);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (code != nullptr) *code = (DWORD)GetExceptionCode();
        return -1;
    }
}

}
