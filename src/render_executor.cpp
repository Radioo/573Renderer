#include "render_executor.h"

#include "gpu_context.h"
#include "render/blend_map.h"
#include "render/command_list.h"
#include "render/vertex_math.h"
#include "render_backend.h"
#include "support/log.h"

#include <cstdint>
#include <d3d9.h>
#include <algorithm>
#include <span>
#include <variant>
#include <windows.h>

namespace RenderExec {

void Execute(IDirect3DDevice9* device, const Render::SetLayerCmd& cmd) {
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLENDALPHA, Blend::kAlphaCoverage.src);
    device->SetRenderState(D3DRS_DESTBLENDALPHA, Blend::kAlphaCoverage.dst);
    device->SetRenderState(D3DRS_BLENDOPALPHA, Blend::kAlphaCoverage.op);

    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    if (cmd.blend_mode == 3) {
        device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_BLENDCURRENTALPHA);
        device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
        device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
    }
    const Blend::D3d9Blend bs = Blend::MapAfpMode(cmd.blend_mode);
    device->SetRenderState(D3DRS_BLENDOP, bs.op);
    device->SetRenderState(D3DRS_SRCBLEND, bs.src);
    device->SetRenderState(D3DRS_DESTBLEND, bs.dst);

    device->SetTexture(1, nullptr);
}

void Execute(IDirect3DDevice9* device, const Render::SetBlendCmd& cmd) {
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

    switch (cmd.mode) {
    case 0:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    case 1:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    case 2:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
        break;
    case 3:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
        break;
    case 4:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    default:
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    }
}

void Execute(IDirect3DDevice9* device, GpuContext& gpu, const Render::MaskCmd& cmd) {
    if (cmd.op == 0 || cmd.op == 2) {
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    } else if (cmd.op == 1) {
        RECT scissor;
        scissor.left = (cmd.x < 0) ? 0 : cmd.x;
        scissor.top = (cmd.y < 0) ? 0 : cmd.y;
        scissor.right = cmd.x + (int)cmd.w;
        scissor.bottom = cmd.y + (int)cmd.h;
        scissor.right = std::min<LONG>(scissor.right, gpu.screen_w);
        scissor.bottom = std::min<LONG>(scissor.bottom, gpu.screen_h);
        scissor.left = std::max<LONG>(scissor.left, 0);
        scissor.top = std::max<LONG>(scissor.top, 0);

        device->SetScissorRect(&scissor);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    }
}

namespace {

D3DPRIMITIVETYPE MapDrawPrimType(int prim_type) {
    switch (prim_type) {
    case 1:
    case 3:
        return D3DPT_LINELIST;
    case 2:
        return D3DPT_LINESTRIP;
    case 4:
        return D3DPT_TRIANGLELIST;
    case 6:
        return D3DPT_TRIANGLEFAN;
    case 5:
    default:
        return D3DPT_TRIANGLESTRIP;
    }
}

void BindDrawTexture(IDirect3DDevice9* device, GpuContext& gpu, int tex_slot) {
    if (tex_slot > 0 && tex_slot < kTexSlotCount && (gpu.textures[tex_slot] != nullptr)) {
        device->SetTexture(0, gpu.textures[tex_slot]);
    } else {
        device->SetTexture(0, gpu.fallback_texture);
    }
}

void ApplyDrawFilterState(IDirect3DDevice9* device, GpuContext& gpu, const Render::DrawCmd& cmd) {
    if (cmd.hsl) {
        const float c0[4] = {0.0F, 0.0F, 0.0F, 0.0F};
        device->SetPixelShader(gpu.afp_hsl_ps);
        device->SetPixelShaderConstantF(0, c0, 1);
        device->SetPixelShaderConstantF(1, cmd.hsv_c1.data(), 1);
        device->SetPixelShaderConstantF(2, gpu.hsv_scope_rect, 1);
        device->SetPixelShaderConstantF(3, cmd.hsv_c3.data(), 1);
        device->SetPixelShaderConstantF(4, gpu.hsv_scope2_rect, 1);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }
    if (cmd.add) {
        device->SetPixelShader(gpu.afp_add_ps);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    }
}

void RestoreDrawFilterState(IDirect3DDevice9* device, const Render::DrawCmd& cmd) {
    if (cmd.hsl) {
        device->SetPixelShader(nullptr);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    }
    if (cmd.add) {
        device->SetPixelShader(nullptr);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    }
}

}

void Execute(IDirect3DDevice9* device, GpuContext& gpu, const Render::DrawCmd& cmd,
             std::span<const AfpVertex> verts) {
    BindDrawTexture(device, gpu, cmd.tex_slot);

    if (gpu.afp_vs != nullptr) device->SetVertexShaderConstantF(4, cmd.shape_color.data(), 1);

    const int vert_count = (int)verts.size();
    const int prim_count = Render::AfpPrimCount(cmd.prim_type, vert_count);
    const D3DPRIMITIVETYPE d3d_prim = MapDrawPrimType(cmd.prim_type);
    if (prim_count <= 0) {
        gpu.submit_zero_prim++;
        if (gpu.submit_zero_prim <= 10) {
            LOG("AfpD3D9", "SubmitGeometry zero_prim: vc=%d prim_type=%d", vert_count,
                cmd.prim_type);
        }
        gpu.draw_count++;
        return;
    }

    if ((gpu.afp_vs != nullptr) && cmd.matrix_ready)
        device->SetVertexShaderConstantF(0, cmd.matrix.data(), 4);

    ApplyDrawFilterState(device, gpu, cmd);

    device->SetFVF(AFP_FVF);
    HRESULT const hr =
        device->DrawPrimitiveUP(d3d_prim, prim_count, verts.data(), sizeof(AfpVertex));
    if (FAILED(hr)) {
        static int dpfail = 0;
        if (dpfail++ < 40) {
            LOG("AfpD3D9", "DrawPrimitiveUP FAILED hr=0x%08lx (prim=%d count=%u vc=%d slot=%d)",
                (unsigned long)hr, (int)d3d_prim, prim_count, vert_count, cmd.tex_slot);
        }
    }

    RestoreDrawFilterState(device, cmd);

    gpu.draw_count++;
}

void Execute(IDirect3DDevice9* device, GpuContext& gpu, const Render::LayerCmdCmd& cmd, uint64_t a2,
             uint64_t a3, void** a4) {
    switch (cmd.sub) {
    case 3:
        if ((a4 != nullptr) && (*a4 != nullptr)) {
            void* inner = *a4;
            void** inner_vtbl = *reinterpret_cast<void***>(inner);
            using Case3Fn = void(__fastcall*)(void*, unsigned int*, uint64_t, uint64_t, void**);
            auto fn = reinterpret_cast<Case3Fn>(inner_vtbl[176 / 8]);
            unsigned int cmd_local = cmd.cmd;
            fn(inner, &cmd_local, a2, a3, a4);
        }
        break;

    case 4:
        if (a4 != nullptr) {
            void** vtbl = *reinterpret_cast<void***>(a4);

            using VtblFn168 = void(__fastcall*)(void**, void*, IDirect3DDevice9*, unsigned int*,
                                                uint64_t, uint64_t);
            using VtblFn176_184 = void(__fastcall*)(void**, void*, IDirect3DDevice9*);

            auto fn168 = reinterpret_cast<VtblFn168>(vtbl[168 / 8]);
            auto fn176 = reinterpret_cast<VtblFn176_184>(vtbl[176 / 8]);
            auto fn184 = reinterpret_cast<VtblFn176_184>(vtbl[184 / 8]);

            unsigned int cmd_local = cmd.cmd;
            static int c4_log = 0;
            if (c4_log < 5) {
                LOG("AfpD3D9", "LayerCommand case 4: a4=%p vtbl=%p device=%p ctx=%p", (void*)a4,
                    (void*)vtbl, (void*)device, gpu.state_ctx);
                LOG("AfpD3D9", "  fn168=%p fn176=%p fn184=%p", (void*)fn168, (void*)fn176,
                    (void*)fn184);
                c4_log++;
            }

            if (fn168 != nullptr) fn168(a4, gpu.state_ctx, device, &cmd_local, a2, a3);
            if (fn176 != nullptr) fn176(a4, gpu.state_ctx, device);
            if (fn184 != nullptr) fn184(a4, gpu.state_ctx, device);
        }
        break;

    case 5:
        if ((a4 != nullptr) && (a4[0] != nullptr)) {
            using Case5Fn = void(__fastcall*)(void*, unsigned int*, uint64_t, uint64_t);
            auto fn = reinterpret_cast<Case5Fn>(a4[0]);
            unsigned int cmd_local = cmd.cmd;
            fn(a4[1], &cmd_local, a2, a3);
        }
        break;

    default:
        break;
    }
}

void ExecuteList(IDirect3DDevice9* device, GpuContext& gpu, const Render::RenderCommandList& list) {
    for (const Render::RenderCommand& rc : list) {
        if (const auto* sl = std::get_if<Render::SetLayerCmd>(&rc)) {
            Execute(device, *sl);
        } else if (const auto* sb = std::get_if<Render::SetBlendCmd>(&rc)) {
            Execute(device, *sb);
        } else if (const auto* mk = std::get_if<Render::MaskCmd>(&rc)) {
            Execute(device, gpu, *mk);
        } else if (const auto* dr = std::get_if<Render::DrawCmd>(&rc)) {
            Execute(device, gpu, *dr, dr->verts);
        }
    }
}

}
