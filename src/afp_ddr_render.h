#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdint>

namespace DdrRender {

void Init(IDirect3DDevice9* device, int screen_w, int screen_h);

void* RenderParams();

void* AfpuConfig();

void SetScreenSize(int w, int h);

typedef int (*TexBindResolver)(unsigned int afp_tex_id);
void SetTexBindResolver(TexBindResolver fn);

int DrawCount();
void ResetDrawCount();

}
