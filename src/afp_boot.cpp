#include "afp_boot.h"
#include "afp_funcs.h"
#include "afpu_funcs.h"
#include "avs_funcs.h"
#include "engine_session.h"
#include "support/dll_loader.h"
#include "support/log.h"
#include "dll_offsets.h"
#include "game_profile.h"
#include "render_backend.h"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <excpt.h>
#include <windows.h>
#include "app_globals.h"

void AfpManager::SetActiveProfile(const GameProfile::Profile* p) {
    g_engine.active_profile = p;
}

namespace {
void* __cdecl AfpMalloc(void* ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
}
namespace {
void* __cdecl AfpRealloc(void* ctx, void* ptr, size_t size) {
    (void)ctx;
    return realloc(ptr, size);
}
}
namespace {
void __cdecl AfpFree(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}
}

namespace {
void __cdecl StubGetScreenSize(int* x, int* y, int* w, int* h) {
    AfpD3D9::GetScreenSize(x, y, w, h);
    if ((w != nullptr) && (h != nullptr)) {
        LOG_ONCE("AFP Stub", "get_screen_size -> %dx%d (via AfpD3D9)", *w, *h);
    }
}
}

namespace {
void __cdecl StubGetNearFar(float* near_val, float* far_val) {
    AfpD3D9::GetNearFar(near_val, far_val);
    if ((near_val != nullptr) && (far_val != nullptr)) {
        LOG_ONCE("AFP Stub", "get_near_far -> %.4f, %.2f (via AfpD3D9)", *near_val, *far_val);
    }
}
}

namespace {

void FillRenderContext(AfpRenderContext& render_ctx) {
    render_ctx.InitZero();
    render_ctx.Flags() = 0x200;

    render_ctx.FnAt(0x008) = (void*)AfpD3D9::BeginRender;
    render_ctx.FnAt(0x010) = (void*)AfpD3D9::EndRender;
    render_ctx.FnAt(0x018) = (void*)AfpD3D9::SetMaskRegion;
    render_ctx.FnAt(0x020) = (void*)AfpD3D9::SetLayer;
    render_ctx.FnAt(0x028) = (void*)AfpD3D9::DrawPrimitive;
    render_ctx.FnAt(0x030) = (void*)AfpD3D9::SetBlend;
    render_ctx.FnAt(0x038) = (void*)AfpD3D9::SubmitGeometry;
    render_ctx.FnAt(0x040) = (void*)AfpD3D9::LayerCommand;
    render_ctx.FnAt(0x058) = (void*)AfpD3D9::SetMatrix;
    render_ctx.FnAt(0x060) = (void*)AfpD3D9::GetScreenSize;
    render_ctx.FnAt(0x068) = (void*)AfpD3D9::GetNearFar;

    render_ctx.FnAt(0x118) = nullptr;
    render_ctx.FnAt(0x120) = (void*)AfpMalloc;
    render_ctx.FnAt(0x128) = (void*)AfpRealloc;
    render_ctx.FnAt(0x130) = (void*)AfpFree;
}

void RunAfpCoreBoot(EngineSession& es) {
    AfpFuncs const& afp = es.afp;
    AfpRenderContext& render_ctx = es.render_ctx;
    LOG("AFP", "Calling afp_boot(render_ctx, flags=0x%x)...", render_ctx.Flags());
    int const ret = afp.afp_boot(&render_ctx);
    LOG("AFP", "afp_boot returned: %d", ret);

    LOG("AFP", "Setting verbose=1...");
    if (afp.afp_set_verbose != nullptr) {
        const uint32_t verbose_flags =
            ((es.active_profile != nullptr) && es.active_profile->afp_set_verbose_wide_args)
                ? 0x10000U
                : 0U;
        afp.afp_set_verbose(1, verbose_flags);
    }

    if ((es.active_profile != nullptr) && !es.active_profile->call_afp_set_flag_setup) {
        LOG("AFP", "Skipping afp_set_flag setup triple (gated off by profile '%s')",
            es.active_profile->slug);
    } else if (afp.afp_set_flag != nullptr) {
        afp.afp_set_flag(16, 0);
        afp.afp_set_flag(8, 0);
        afp.afp_set_flag(65537, 0);
        LOG("AFP", "afp_set_flag: 16, 8, 65537 (matching bm2dx)");
    }
}

void RunAfpuBoot(EngineSession& es) {
    AfpuFuncs const& afpu = es.afpu;
    AvsFuncs const& avs = es.avs;
    static uint8_t afpu_prop_buf[4096];
    memset(afpu_prop_buf, 0, sizeof(afpu_prop_buf));
    auto* afpu_prop = avs.property_create(7, afpu_prop_buf, 0xD18);
    void* afpu_config = nullptr;
    if (afpu_prop != nullptr) {
        avs.property_node_create(afpu_prop, nullptr, 0x4, "/config/render/max_nr_masks", 16);
        afpu_config = static_cast<void*>(avs.property_search(afpu_prop, nullptr, "/config"));
        LOG("AFP", "afpu config: %p", afpu_config);
    }

    static uint8_t afpu_data[128] = {};
    auto SetPtr = [&](size_t off, void* p) { *reinterpret_cast<void**>(&afpu_data[off]) = p; };
    auto SetF32 = [&](size_t off, float f) { *reinterpret_cast<float*>(&afpu_data[off]) = f; };
    auto SetU32 = [&](size_t off, uint32_t u) {
        *reinterpret_cast<uint32_t*>(&afpu_data[off]) = u;
    };

    SetPtr(0x00, (void*)AfpD3D9::TexCreate);
    SetPtr(0x08, (void*)AfpD3D9::TexDestroy);
    SetPtr(0x10, (void*)AfpD3D9::TexUpload);
    SetPtr(0x18, nullptr);
    SetPtr(0x20, (void*)AfpMalloc);
    SetPtr(0x28, (void*)AfpRealloc);
    SetPtr(0x30, (void*)AfpFree);
    SetF32(0x38, 10000.0F);
    SetF32(0x3C, 1.0F);
    SetU32(0x40, 2U);

    if ((es.active_profile != nullptr) && !es.active_profile->call_afpu_boot) {
        LOG("AFP", "Skipping afpu_boot (gated off by profile '%s')", es.active_profile->slug);
    } else {
        LOG("AFP", "Calling afpu_boot...");
        int const ret = afpu.afpu_boot(afpu_config, afpu_data);
        LOG("AFP", "afpu_boot returned: %d", ret);
    }
    if (afpu_prop != nullptr) avs.property_destroy(afpu_prop);
}

void PatchNearFarSlot(EngineSession& es) {
    DllLoader const& afp_dll = es.afp_dll;
    if ((es.active_profile != nullptr) && !es.active_profile->apply_iidx_data_segment_patches) {
        LOG("AFP", "Skipping nearfar callback patch (gated off by profile '%s')",
            es.active_profile->slug);
    } else {
        HMODULE afp_base = afp_dll.Module();
        const uintptr_t nearfar_off = (es.active_profile != nullptr)
                                          ? es.active_profile->offsets.afp_nearfar_slot
                                          : DllOffsets::AfpCore::kNearFarSlot;
        void** nearfar_slot = DllOffsets::At<void*>(afp_base, nearfar_off);
        DWORD old_protect = 0;
        if (VirtualProtect(static_cast<LPVOID>(nearfar_slot), 8, PAGE_READWRITE, &old_protect) !=
            0) {
            LOG("AFP", "Patching get_near_far callback at base+0x%X (was %p)",
                (unsigned)nearfar_off, *nearfar_slot);
            *nearfar_slot = (void*)StubGetNearFar;
            VirtualProtect(static_cast<LPVOID>(nearfar_slot), 8, old_protect, &old_protect);
            LOG("AFP", "Patched to %p", (void*)StubGetNearFar);
        } else {
            LOG("AFP", "Failed to patch get_near_far callback");
        }
    }
}

void RunRenderInits(EngineSession& es) {
    AfpFuncs const& afp = es.afp;
    AfpuFuncs const& afpu = es.afpu;
    AfpRenderContext& render_ctx = es.render_ctx;
    int ret = 0;
    if ((es.active_profile != nullptr) && !es.active_profile->call_afp_render_init) {
        LOG("AFP", "Skipping afp_render_init (gated off by profile '%s')", es.active_profile->slug);
    } else {
        LOG("AFP", "Calling afp_render_init...");
        afp.afp_render_init();
        LOG("AFP", "afp_render_init completed!");
    }

    if ((es.active_profile != nullptr) && !es.active_profile->call_afpu_render_init) {
        LOG("AFP", "Skipping afpu_render_init (gated off by profile '%s')",
            es.active_profile->slug);
    } else {
        LOG("AFP", "Calling afpu_render_init...");
        ret = afpu.afpu_render_init(&render_ctx);
        LOG("AFP", "afpu_render_init returned: %d", ret);
    }

    (void)ret;
}

void ClearRenderFlag0x800(const EngineSession& es, HMODULE afp_base, bool can_patch,
                          bool skip_set_data, const GameProfile::DllOffsetSet& off) {
    DWORD old_protect = 0;
    if (can_patch && !skip_set_data) {
        auto* flags_ptr = DllOffsets::At<uint32_t>(afp_base, off.afp_render_flags);
        if (VirtualProtect(flags_ptr, 4, PAGE_READWRITE, &old_protect) != 0) {
            uint32_t const old_flags = *flags_ptr;
            *flags_ptr &= ~0x800U;
            VirtualProtect(flags_ptr, 4, old_protect, &old_protect);
            LOG("AFP", "Cleared 0x800 flag in afp-core at base+0x%X (was 0x%X, now 0x%X)",
                (unsigned)off.afp_render_flags, old_flags, *flags_ptr);
        }
    } else if (skip_set_data) {
        LOG("AFP",
            "Leaving afp-core 0x800 flag SET (profile '%s' uses afpu_render_init's "
            "internal afp_set_afp_data rebind path)",
            es.active_profile->slug);
    } else {
        LOG("AFP", "Skipping afp-core 0x800 flag clear (gated off by profile '%s')",
            es.active_profile->slug);
    }
}

void CallSetAfpData(EngineSession& es, HMODULE afpu_base, bool can_patch, bool skip_set_data,
                    const GameProfile::DllOffsetSet& off) {
    AfpFuncs const& afp = es.afp;
    AfpRenderContext& render_ctx = es.render_ctx;
    if (can_patch && !skip_set_data) {
        void* afpu_data_ptr = DllOffsets::At<void>(afpu_base, off.afpu_data_struct);
        afp_set_afp_data_t set_data = afp.afp_set_afp_data;
        if (set_data != nullptr) {
            uint64_t const a2 = 0;
            uint64_t a3 = 0;
            void* a4 = nullptr;
            if ((es.active_profile != nullptr) && es.active_profile->afp_set_afp_data_wide_args) {
                a3 = 0x320;
                a4 = &render_ctx;
            }
            set_data(afpu_data_ptr, a2, a3, a4);
            LOG("AFP", "Called afp_set_afp_data(afpu+0x%X, ...) (wide=%d)",
                (unsigned)off.afpu_data_struct,
                es.active_profile && es.active_profile->afp_set_afp_data_wide_args);
        } else {
            LOG("AFP", "ERROR: Could not find afp_set_afp_data export!");
        }
    } else if (skip_set_data) {
        LOG("AFP",
            "Skipping explicit afp_set_afp_data (profile '%s' relies on "
            "afpu_render_init's internal call)",
            es.active_profile->slug);
    } else {
        LOG("AFP", "Skipping afp_set_afp_data (gated off by profile '%s')",
            es.active_profile->slug);
    }
}

void VerifyCallbackTable(HMODULE afp_base, HMODULE afpu_base, bool can_patch,
                         const GameProfile::DllOffsetSet& off) {
    if (can_patch) {
        void** afp_table = DllOffsets::At<void*>(afp_base, off.afp_callback_table);
        LOG("AFP",
            "Callback table verification (afp-core+0x%X):", (unsigned)off.afp_callback_table);
        for (int i = 0; i < 8; i++) {
            ptrdiff_t const pdiff =
                (afp_table[i] != nullptr) ? ((uint8_t*)afp_table[i] - (uint8_t*)afpu_base) : 0;
            LOG("AFP", "  slot[%d] = %p (afpu+0x%llX)", i, afp_table[i], (unsigned long long)pdiff);
        }
    }
}

void RepatchScreenStubs(HMODULE afp_base, bool can_patch, const GameProfile::DllOffsetSet& off) {
    if (can_patch) {
        void** afp_table = DllOffsets::At<void*>(afp_base, off.afp_callback_table);
        DWORD old_protect2 = 0;
        if (VirtualProtect(static_cast<LPVOID>(&afp_table[12]), 16, PAGE_READWRITE,
                           &old_protect2) != 0) {
            afp_table[12] = (void*)StubGetScreenSize;
            afp_table[13] = (void*)StubGetNearFar;
            VirtualProtect(static_cast<LPVOID>(&afp_table[12]), 16, old_protect2, &old_protect2);
            LOG("AFP", "Re-patched slot[12]=StubGetScreenSize=%p slot[13]=StubGetNearFar=%p",
                (void*)StubGetScreenSize, (void*)StubGetNearFar);
        } else {
            LOG("AFP", "Failed to re-patch screen-size/near-far stubs after afp_set_afp_data");
        }
    }
}

void BindD3D9AndDataSegment(EngineSession& es, D3D9State& d3d) {
    AfpuFuncs const& afpu = es.afpu;
    AfpD3D9::Init(d3d.device, d3d.width, d3d.height, d3d.afp_vs, d3d.afp_hsl_ps, d3d.afp_add_ps);
    AfpD3D9::SetStateCtx(&es.render_ctx);
    AfpD3D9::SetAfpuTexSlotResolver(afpu.afpuloc_get_texture_data_size);

    HMODULE afp_base = es.afp_dll.Module();
    HMODULE afpu_base = es.afpu_dll.Module();
    const bool can_patch =
        (es.active_profile == nullptr) || es.active_profile->apply_iidx_data_segment_patches;
    const bool skip_set_data =
        (es.active_profile != nullptr) && es.active_profile->skip_explicit_afp_set_afp_data;
    const auto& off = (es.active_profile != nullptr) ? es.active_profile->offsets
                                                     : GameProfile::kFallbackIidxOffsets;

    GameProfile::SetActiveOffsets(off);
    AfpD3D9::SetAfpuSetScreenRectFnOffset(off.afpu_set_screen_rect_fn);

    ClearRenderFlag0x800(es, afp_base, can_patch, skip_set_data, off);
    CallSetAfpData(es, afpu_base, can_patch, skip_set_data, off);
    VerifyCallbackTable(afp_base, afpu_base, can_patch, off);
    RepatchScreenStubs(afp_base, can_patch, off);
}

void ConfigureAfpu(EngineSession& es) {
    AfpuFuncs const& afpu = es.afpu;
    if (afpu.afpu_ext_command != nullptr) {
        const char* ver = nullptr;
        afpu.afpu_ext_command(2, static_cast<void*>(&ver));
        if (ver != nullptr) LOG("AFP", "AFPU Version: %s", ver);
    }

    if ((es.active_profile != nullptr) && !es.active_profile->call_afpu_set_config) {
        LOG("AFP", "Skipping afpu_set_config (gated off by profile '%s')", es.active_profile->slug);
    } else if (afpu.afpu_set_config != nullptr) {
        afpu.afpu_set_config(1, 4096);
        afpu.afpu_set_config(2, 10);
        const int clean_pos =
            ((es.active_profile != nullptr) && es.active_profile->afpu_set_config_safe_clean_pos)
                ? 0
                : 1;
        afpu.afpu_set_config(3, clean_pos);
        LOG("AFP", "afpu_set_config: buffer=4096, quality=10, clean_pos=%d", clean_pos);
    }
    if ((es.active_profile != nullptr) && !es.active_profile->call_afpu_set_flag_setup) {
        LOG("AFP", "Skipping afpu_set_flag setup triple (gated off by profile '%s')",
            es.active_profile->slug);
    } else if (afpu.afpu_set_flag != nullptr) {
        afpu.afpu_set_flag(4, 4);
        afpu.afpu_set_flag(8, 8);
        afpu.afpu_set_flag(16, 16);
    }
}

void SetStreamNrGuarded(const EngineSession& es) {
    AfpFuncs const& afp = es.afp;
    if ((es.active_profile != nullptr) && !es.active_profile->call_afp_set_stream_nr) {
        LOG("AFP", "Skipping afp_set_stream_nr (gated off by profile '%s')",
            es.active_profile->slug);
    } else {
        LOG("AFP", "Setting stream_nr=4096...");
        if (afp.afp_set_stream_nr != nullptr) {
            __try {
                afp.afp_set_stream_nr(4096);
                LOG("AFP", "afp_set_stream_nr returned cleanly");
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                LOG("AFP",
                    "afp_set_stream_nr threw 0x%08lx - likely an "
                    "ordinal mismatch on this game's afp-core. "
                    "Continuing; will probe afp_stream_create next.",
                    (unsigned long)GetExceptionCode());
            }
        }
    }
}

void ProbeStreamCreateGuarded(const EngineSession& es) {
    AfpFuncs const& afp = es.afp;
    if ((es.active_profile != nullptr) && !es.active_profile->call_afp_stream_create_test) {
        LOG("AFP", "Skipping test stream_create probe (gated off by profile '%s')",
            es.active_profile->slug);
    } else {
        __try {
            uint32_t const test_stream = afp.afp_stream_create();
            LOG("AFP", "Test stream_create = 0x%08x", test_stream);
            if (test_stream != 0xFFFFFFFC && (int)test_stream >= 0) {
                afp.afp_stream_destroy(5, test_stream, 0);
                LOG("AFP", "Stream creation works!");
            } else {
                LOG("AFP", "WARNING: stream_create still fails!");
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("AFP",
                "test stream_create probe threw 0x%08lx - skipping (diagnostic only; "
                "real streams are created via the package load path)",
                (unsigned long)GetExceptionCode());
        }
    }
}

}

bool AfpManager::Boot(EngineSession& es, D3D9State& d3d) {
    AfpFuncs const& afp = es.afp;
    LOG("AFP", "Booting AFP...");

    if (afp.afp_ext_command != nullptr) {
        const char* ver = nullptr;
        afp.afp_ext_command(9, static_cast<void*>(&ver));
        if (ver != nullptr) LOG("AFP", "AFP-Core Version: %s", ver);
    }

    FillRenderContext(es.render_ctx);
    RunAfpCoreBoot(es);
    RunAfpuBoot(es);
    PatchNearFarSlot(es);
    RunRenderInits(es);
    BindD3D9AndDataSegment(es, d3d);
    ConfigureAfpu(es);
    SetStreamNrGuarded(es);
    ProbeStreamCreateGuarded(es);

    es.afp_booted = true;
    return true;
}
