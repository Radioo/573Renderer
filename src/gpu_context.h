#pragma once
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstddef>
#include <vector>

#include "render/command_list.h"
#include "dll_offsets.h"
#include "render_backend.h"

struct AtlasFilterEntry {
    unsigned int mag;
    unsigned int min;
};

inline constexpr int kTexSlotCount = 1024;

struct GpuContext {
    D3D9State d3d;

    void (*on_texture_created)() = nullptr;
    void (*query_crop_overlay)(bool* pick_mode, int* x, int* y, int* w, int* h) = nullptr;

    IDirect3DDevice9* device = nullptr;
    int screen_w = 1280;
    int screen_h = 720;
    IDirect3DTexture9* current_texture = nullptr;
    IDirect3DTexture9* fallback_texture = nullptr;
    IDirect3DTexture9* textures[kTexSlotCount] = {};
    int texture_dims_w[kTexSlotCount] = {};
    int texture_dims_h[kTexSlotCount] = {};
    unsigned int texture_mag_filter[kTexSlotCount] = {};
    unsigned int texture_min_filter[kTexSlotCount] = {};
    std::vector<AtlasFilterEntry> atlas_filter_queue;
    size_t atlas_filter_queue_head = 0;
    unsigned int active_mag_filter_stage0 = 0;
    unsigned int active_min_filter_stage0 = 0;
    int next_tex_slot = 1;
    int persistent_tex_high_water = 1;
    int texture_width = 0;
    int texture_height = 0;
    bool in_scene = false;
    int draw_count = 0;
    bool state_setup = false;
    bool in_mask_write = false;
    int submit_rejected_device = 0;
    int submit_rejected_vc = 0;
    int submit_zero_prim = 0;
    int layer_cmd_calls[8] = {};
    IDirect3DVertexShader9* afp_vs = nullptr;
    IDirect3DPixelShader9* afp_hsl_ps = nullptr;
    IDirect3DPixelShader9* afp_add_ps = nullptr;
    const unsigned char* hsv_desc_ptr = nullptr;
    bool qpro_draw_probe = false;
    Render::RenderCommandList* cmd_list = nullptr;
    bool deferred_replay = false;
    Render::RenderCommandList frame_cmds;
    float hsv_scope_rect[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    unsigned char hsv_captured[16] = {};
    float hsv_scope2_rect[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    HMODULE afpu_base = nullptr;
    uintptr_t afpu_set_screen_rect_off = DllOffsets::AfpUtils::kSetScreenRectFn;
    void* state_ctx = nullptr;
    uint32_t (*afpu_get_tex_slot)(uint32_t tex_id) = nullptr;
    alignas(16) float current_matrix[16] = {};
    bool current_matrix_ready = false;
    float afp_render_x_offset = 0.0f;
};

inline GpuContext g_gpu;
