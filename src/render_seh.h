#pragma once

#include "afp_funcs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

namespace RenderSeh {

struct FaultReport {
    bool faulted;
    DWORD code;
    uintptr_t pc;
    uintptr_t target;
    DWORD op;
    uint64_t regs[16];
};

FaultReport SafeCallUpdate(afp_do_update_t fn, float dt);

FaultReport SafeCallSortRender(afp_do_sort_render_t fn);

void LogFault(const char* what, int frame, const FaultReport& report);

int SafeEnumChildNames(afp_mc_enumerate_children_t fn, uint32_t parent_mc, int flags,
                       char (*names)[128], int max_names, DWORD* code);

int SafeGetIdByPath(afp_mc_get_id_by_path_t fn, uint32_t stream_id, const char* path, DWORD* code);

}
