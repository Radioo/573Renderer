#pragma once

struct GpuContext;
struct IDirect3DDevice9;

namespace Render {

void CompileAfpShaders(IDirect3DDevice9* device, GpuContext& gpu);

}
