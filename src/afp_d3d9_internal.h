#pragma once
#include <windows.h>
#include <d3d9.h>
#include "gpu_context.h"

namespace AfpD3D9 {

void AfpScreamUnimpl(const char* what, const void* caller);

inline void BuildDefaultMatrix(float* dst, float w, float h) {
    dst[0] = 2.0f / w;
    dst[1] = 0.0f;
    dst[2] = 0.0f;
    dst[3] = -1.0f + 2.0f * g_gpu.afp_render_x_offset / w;
    dst[4] = 0.0f;
    dst[5] = -2.0f / h;
    dst[6] = 0.0f;
    dst[7] = 1.0f;
    dst[8] = 0.0f;
    dst[9] = 0.0f;
    dst[10] = -0.5f;
    dst[11] = 0.5f;
    dst[12] = 0.0f;
    dst[13] = 0.0f;
    dst[14] = 0.0f;
    dst[15] = 1.0f;
}

inline void TransposeMatrix4x4(float* dst, const float* src) {
    float tmp[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            tmp[r * 4 + c] = src[c * 4 + r];
    for (int i = 0; i < 16; i++)
        dst[i] = tmp[i];
}

}

#define AFP_SCREAM_UNIMPL(what)                                                                    \
    do {                                                                                           \
        static bool afp_screamed_ = false;                                                         \
        if (!afp_screamed_) {                                                                      \
            afp_screamed_ = true;                                                                  \
            AfpD3D9::AfpScreamUnimpl((what), _ReturnAddress());                                    \
        }                                                                                          \
    } while (0)
