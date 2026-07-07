#include "gpu_context.h"
#include "render_backend.h"
#include "render_executor.h"
#include "support/log.h"
#include "dll_offsets.h"
#include "game_profile.h"
#include <cstdint>
#include <cstdio>
#include <d3d9.h>
#include "afp_d3d9_internal.h"

namespace AfpD3D9 {
void Init(IDirect3DDevice9* device, int screen_w, int screen_h, IDirect3DVertexShader9* vs,
          IDirect3DPixelShader9* hsl_ps, IDirect3DPixelShader9* add_ps) {
    g_gpu.device = device;
    g_gpu.screen_w = screen_w;
    g_gpu.screen_h = screen_h;
    g_gpu.draw_count = 0;
    g_gpu.afp_vs = vs;
    g_gpu.afp_hsl_ps = hsl_ps;
    g_gpu.afp_add_ps = add_ps;

    g_gpu.afpu_base = GetModuleHandleA("afp-utils.dll");

    if ((device != nullptr) && (g_gpu.fallback_texture == nullptr)) {
        IDirect3DTexture9* tex = nullptr;
        HRESULT const hr = device->CreateTexture(2, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT, &tex, nullptr);
        if (SUCCEEDED(hr)) {
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(tex->LockRect(0, &lr, nullptr, 0))) {
                auto* pixels = (DWORD*)lr.pBits;
                pixels[0] = 0xFFFFFFFF;
                pixels[1] = 0xFFFFFFFF;
                pixels[lr.Pitch / 4] = 0xFFFFFFFF;
                pixels[(lr.Pitch / 4) + 1] = 0xFFFFFFFF;
                tex->UnlockRect(0);
            }
            g_gpu.fallback_texture = tex;
        }
    }

    LOG("AfpD3D9", "Initialized (device=%p, %dx%d, fallback_tex=%p)", device, screen_w, screen_h,
        g_gpu.fallback_texture);

    if (g_gpu.afpu_base != nullptr) {
        auto* data_struct =
            DllOffsets::At<uint64_t>(g_gpu.afpu_base, DllOffsets::AfpUtils::kDataStruct);
        LOG("AfpD3D9", "afpu data struct at %p:", data_struct);
        for (int i = 0; i < 35; i++) {
            uint64_t const val = data_struct[i];
            printf("  [%2d] +0x%03X: 0x%016llX\n", i, i * 8, val);
        }
    }
}

void SetScreenSize(int w, int h) {
    g_gpu.screen_w = w;
    g_gpu.screen_h = h;
    g_gpu.state_setup = false;
}

void SetAfpuSetScreenRectFnOffset(uintptr_t offset) {
    g_gpu.afpu_set_screen_rect_off = offset;
    LOG("AfpD3D9", "SetAfpuSetScreenRectFnOffset(0x%llx)", (unsigned long long)offset);
}

int GetTotalDrawCount() {
    return g_gpu.draw_count;
}

void SetAfpuTexSlotResolver(uint32_t (*fn)(uint32_t)) {
    g_gpu.afpu_get_tex_slot = fn;
    LOG("AfpD3D9", "SetAfpuTexSlotResolver(%p)", (void*)fn);
}

void SetStateCtx(void* ctx) {
    g_gpu.state_ctx = ctx;
    if ((ctx != nullptr) && (g_gpu.device != nullptr)) {
        *reinterpret_cast<IDirect3DDevice9**>((uint8_t*)ctx + 0x18000) = g_gpu.device;
    }
}

namespace {
namespace {

void InstallDefaultBlendAndStencil() {
    g_gpu.device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_gpu.device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_gpu.device->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_gpu.device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
    g_gpu.device->SetRenderState(D3DRS_CLIPPING, TRUE);
    g_gpu.device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    g_gpu.device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_gpu.device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_gpu.device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    g_gpu.device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_MAX);
    g_gpu.device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);
    g_gpu.device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    g_gpu.device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
    g_gpu.device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
    g_gpu.device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    g_gpu.device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    g_gpu.device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

void InstallDefaultStagesAndSamplers() {
    g_gpu.device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_gpu.device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_gpu.device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_gpu.device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_gpu.device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_gpu.device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_gpu.device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    g_gpu.device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    g_gpu.device->SetTexture(1, nullptr);

    g_gpu.device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_gpu.device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    g_gpu.device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    g_gpu.device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);
    g_gpu.device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_gpu.device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    g_gpu.device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_gpu.device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    g_gpu.device->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    g_gpu.device->SetSamplerState(1, D3DSAMP_MAXANISOTROPY, 1);
    g_gpu.device->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_gpu.device->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    g_gpu.active_mag_filter_stage0 = D3DTEXF_LINEAR;
    g_gpu.active_min_filter_stage0 = D3DTEXF_ANISOTROPIC;
}

}

void InstallDefaultRenderStates() {
    if (g_gpu.device == nullptr) return;

    g_gpu.device->SetFVF(AFP_FVF);
    InstallDefaultBlendAndStencil();
    InstallDefaultStagesAndSamplers();

    auto w = (float)g_gpu.screen_w;
    auto h = (float)g_gpu.screen_h;

    if (g_gpu.afp_vs != nullptr) {
        g_gpu.device->SetVertexShader(g_gpu.afp_vs);

        BuildDefaultMatrix(g_gpu.current_matrix, w, h);
        g_gpu.current_matrix_ready = true;
        g_gpu.device->SetVertexShaderConstantF(0, g_gpu.current_matrix, 4);

        float color_const[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        g_gpu.device->SetVertexShaderConstantF(4, color_const, 1);
    } else {
        g_gpu.device->SetVertexShader(nullptr);
    }
    g_gpu.device->SetPixelShader(nullptr);
}
}

void __cdecl BeginRender() {
    if (g_gpu.device == nullptr) return;
    g_gpu.hsv_desc_ptr = nullptr;
    if (g_gpu.deferred_replay) g_gpu.frame_cmds.clear();
    g_gpu.device->SetPixelShader(nullptr);

    if ((g_gpu.afpu_base != nullptr) && (g_gpu.afpu_set_screen_rect_off != 0U)) {
        auto set_screen_rect =
            DllOffsets::At<void(int*)>(g_gpu.afpu_base, g_gpu.afpu_set_screen_rect_off);
        int rect[4] = {0, 0, g_gpu.screen_w, g_gpu.screen_h};
        set_screen_rect(rect);
    }

    InstallDefaultRenderStates();
    g_gpu.state_setup = true;

    ResetMaskWrite();
    g_gpu.device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    if (g_gpu.fallback_texture != nullptr) {
        g_gpu.device->SetTexture(0, g_gpu.fallback_texture);
    }
}

void __cdecl EndRender() {
    if (g_gpu.deferred_replay && (g_gpu.device != nullptr)) {
        RenderExec::ExecuteList(g_gpu.device, g_gpu, g_gpu.frame_cmds);
        g_gpu.frame_cmds.clear();
    }
    static int frame = 0;
    static int prev_draws = 0;
    static int prev_shapes_b = 0;
    static int prev_shapes_a = 0;
    static int prev_drawn = 0;
    if (frame < 10) {
        const auto& off = GameProfile::ActiveOffsets();
        if ((g_gpu.afpu_base != nullptr) && (off.afpu_shapes_a != 0U) &&
            (off.afpu_shapes_b != 0U) && (off.afpu_drawn != 0U)) {
            int const shapes_b = *DllOffsets::At<int>(g_gpu.afpu_base, off.afpu_shapes_b);
            int const shapes_a = *DllOffsets::At<int>(g_gpu.afpu_base, off.afpu_shapes_a);
            int const drawn = *DllOffsets::At<int>(g_gpu.afpu_base, off.afpu_drawn);
            int const draws_this = g_gpu.draw_count - prev_draws;
            int const b_this = shapes_b - prev_shapes_b;
            int const a_this = shapes_a - prev_shapes_a;
            int const drawn_this = drawn - prev_drawn;
            LOG("AfpD3D9",
                "EndRender frame %d: draws=%d (+%d)  "
                "afpu[A+B=%d+%d->drawn=%d  this:%d+%d->%d]",
                frame, g_gpu.draw_count, draws_this, shapes_a, shapes_b, drawn, a_this, b_this,
                drawn_this);
            if (frame == 0 || frame == 9) {
                LOG("AfpD3D9",
                    "  our reject buckets: "
                    "submit_dev_or_geo=%d submit_vc=%d submit_zero_prim=%d",
                    g_gpu.submit_rejected_device, g_gpu.submit_rejected_vc, g_gpu.submit_zero_prim);
                LOG("AfpD3D9",
                    "  LayerCommand per-case: "
                    "[0]=%d [1]=%d [2]=%d [3]=%d [4]=%d [5]=%d bit0_off=%d unk=%d",
                    g_gpu.layer_cmd_calls[0], g_gpu.layer_cmd_calls[1], g_gpu.layer_cmd_calls[2],
                    g_gpu.layer_cmd_calls[3], g_gpu.layer_cmd_calls[4], g_gpu.layer_cmd_calls[5],
                    g_gpu.layer_cmd_calls[6], g_gpu.layer_cmd_calls[7]);
            }
            prev_draws = g_gpu.draw_count;
            prev_shapes_b = shapes_b;
            prev_shapes_a = shapes_a;
            prev_drawn = drawn;
        } else {
            LOG("AfpD3D9", "EndRender frame %d: %d draws", frame, g_gpu.draw_count);
        }
    }
    frame++;

    InstallDefaultRenderStates();
}

}
