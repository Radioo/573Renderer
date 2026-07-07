#include "gpu_context.h"
#include "render_backend.h"
#include "afp_d3d9_internal.h"
#include "render/command_list.h"
#include "render_executor.h"
#include "support/log.h"
#include <cstdint>
#include <cstdio>
#include <intrin.h>
#include <windows.h>

namespace AfpD3D9 {

void __cdecl SetShapeMatrix(const int* mat2d) {
    (void)mat2d;
}

void __cdecl SetMatrix(void* matrix_4x4) {
    if (g_gpu.device == nullptr) return;

    static int mat_count = 0;
    if (mat_count < 10) {
        LOG("AfpD3D9", "SetMatrix(%p) call #%d", matrix_4x4, mat_count);
        if (matrix_4x4 != nullptr) {
            auto* m = (float*)matrix_4x4;
            auto* d = (DWORD*)matrix_4x4;
            printf("  hex: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n", d[0], d[1], d[2],
                   d[3], d[4], d[5], d[6], d[7]);
            printf("       %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n", d[8], d[9], d[10],
                   d[11], d[12], d[13], d[14], d[15]);
            printf("  flt: %.4f %.4f %.4f %.4f\n", m[0], m[1], m[2], m[3]);
            printf("       %.4f %.4f %.4f %.4f\n", m[4], m[5], m[6], m[7]);
            printf("       %.4f %.4f %.4f %.4f\n", m[8], m[9], m[10], m[11]);
            printf("       %.4f %.4f %.4f %.4f\n", m[12], m[13], m[14], m[15]);
        }
        mat_count++;
    }

    if (matrix_4x4 != nullptr) {
        TransposeMatrix4x4(g_gpu.current_matrix, (const float*)matrix_4x4);
        if (g_gpu.afp_render_x_offset != 0.0F && g_gpu.screen_w > 0)
            g_gpu.current_matrix[3] += 2.0F * g_gpu.afp_render_x_offset / (float)g_gpu.screen_w;
    } else {
        BuildDefaultMatrix(g_gpu.current_matrix, (float)g_gpu.screen_w, (float)g_gpu.screen_h);
    }
    g_gpu.current_matrix_ready = true;
    g_gpu.device->SetVertexShaderConstantF(0, g_gpu.current_matrix, 4);
}

void __cdecl SetMatrixFull(void* data) {
    SetMatrix(data);
}

void __cdecl InvalidateBlend() {}

void __cdecl GetScreenSize(int* x, int* y, int* w, int* h) {
    if (x != nullptr) *x = 0;
    if (y != nullptr) *y = 0;
    if (w != nullptr) *w = g_gpu.screen_w;
    if (h != nullptr) *h = g_gpu.screen_h;
}

void __cdecl GetNearFar(float* near_val, float* far_val) {
    if (near_val != nullptr) *near_val = 0.01F;
    if (far_val != nullptr) *far_val = 1000.0F;
}

char __cdecl FindTexture(void* name, void* data) {
    static int find_count = 0;
    if (find_count < 20) {
        LOG("AfpD3D9", "FindTexture('%s', data=%p)", (const char*)name, data);
        find_count++;
    }
    return 0;
}

void __cdecl LayerCommand(unsigned int cmd, uint64_t a2, uint64_t a3, void** a4) {
    if ((cmd & 1U) == 0) {
        g_gpu.layer_cmd_calls[6]++;
        return;
    }

    unsigned int const sub = (cmd >> 1) & 0x3FFU;
    Render::LayerCmdCmd lc{.cmd = cmd, .sub = sub};
    if (g_gpu.cmd_list != nullptr) g_gpu.cmd_list->emplace_back(lc);
    if (sub <= 5) {
        g_gpu.layer_cmd_calls[sub]++;
    } else {
        g_gpu.layer_cmd_calls[7]++;
    }
    static int lc_log = 0;
    if (lc_log < 30) {
        LOG("AfpD3D9", "LayerCommand cmd=0x%08x sub=%u a2=0x%llx a3=0x%llx a4=%p", cmd, sub,
            (unsigned long long)a2, (unsigned long long)a3, (void*)a4);
        lc_log++;
    }

    switch (sub) {
    case 0:
    case 1:
        AFP_SCREAM_UNIMPL("LayerCommand DrawLines/DrawLineStrip (sub 0/1)");
        break;

    case 2: {
        AFP_SCREAM_UNIMPL("LayerCommand DrawTextured (sub 2)");
        static int c2_log = 0;
        if (c2_log < 5) {
            LOG("AfpD3D9", "LayerCommand case 2 (DrawTextured) -- not implemented");
            c2_log++;
        }
    } break;

    case 3:
    case 4:
    case 5:
        break;

    default:
        AFP_SCREAM_UNIMPL("LayerCommand unknown sub-op");
        break;
    }

    if (g_gpu.deferred_replay && (sub == 3 || sub == 4 || sub == 5))
        AFP_SCREAM_UNIMPL("LayerCommand sub 3/4/5 under --deferred-replay (draws out of order)");
    RenderExec::Execute(g_gpu.device, g_gpu, lc, a2, a3, a4);
}

void __cdecl SetMaskRegion(unsigned int mode, unsigned int layer, unsigned int x, unsigned int y,
                           unsigned int w, unsigned int h) {
    if ((g_gpu.device == nullptr) && !g_gpu.deferred_replay) return;
    const Render::MaskCmd cmd{.op = mode, .layer = layer, .x = (int)x, .y = (int)y, .w = w, .h = h};
    if (g_gpu.cmd_list != nullptr) g_gpu.cmd_list->emplace_back(cmd);
    if (g_gpu.deferred_replay) g_gpu.frame_cmds.emplace_back(cmd);

    static int cmd_count = 0;
    if (cmd_count < 32) {
        LOG("AfpD3D9", "SetMaskRegion #%d: op=%u layer=%u rect=(%d,%d,%u,%u)", cmd_count, mode,
            layer, (int)x, (int)y, w, h);
    }
    cmd_count++;

    if (mode == 0) {
        g_gpu.in_mask_write = true;
    } else if (mode == 1 || mode == 2) {
        g_gpu.in_mask_write = false;
    }

    if (!g_gpu.deferred_replay) RenderExec::Execute(g_gpu.device, g_gpu, cmd);
}

}
