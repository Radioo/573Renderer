#pragma once
#include <d3d9.h>
#include <cstdint>
#include <span>

#include "render/command_list.h"

struct GpuContext;

namespace RenderExec {

void Execute(IDirect3DDevice9* device, const Render::SetLayerCmd& cmd);

void Execute(IDirect3DDevice9* device, const Render::SetBlendCmd& cmd);

void Execute(IDirect3DDevice9* device, GpuContext& gpu, const Render::MaskCmd& cmd);

void Execute(IDirect3DDevice9* device, GpuContext& gpu, const Render::DrawCmd& cmd,
             std::span<const AfpVertex> verts);

void Execute(IDirect3DDevice9* device, GpuContext& gpu, const Render::LayerCmdCmd& cmd, uint64_t a2,
             uint64_t a3, void** a4);

void ExecuteList(IDirect3DDevice9* device, GpuContext& gpu, const Render::RenderCommandList& list);

}
