#include "backend/afp_shaders.h"

#include "gpu_context.h"
#include "support/log.h"
#include "support/module_handle.h"

#include <d3d9.h>
#include <utility>
#include <windows.h>

namespace Render {

namespace {

using D3DXAssembleShaderFn = HRESULT(WINAPI*)(LPCSTR, UINT, const void*, void*, DWORD, void**,
                                              void**);

D3DXAssembleShaderFn ResolveD3dxAssembler() {
    static Support::ModuleHandle s_d3dx_mod;
    static D3DXAssembleShaderFn s_assemble = nullptr;
    static bool s_resolved = false;
    if (s_resolved) return s_assemble;
    s_resolved = true;
    const char* d3dx_dlls[] = {"d3dx9_43.dll",  "d3dx9_42.dll",  "d3dx9_41.dll",
                               "d3dx9d_43.dll", "d3dx9d_42.dll", "d3dx9d_41.dll"};
    for (const char* n : d3dx_dlls) {
        Support::ModuleHandle m = Support::LoadModule(n);
        if (m == nullptr) continue;
        s_assemble = (D3DXAssembleShaderFn)GetProcAddress(m.get(), "D3DXAssembleShader");
        if (s_assemble != nullptr) {
            s_d3dx_mod = std::move(m);
            break;
        }
    }
    return s_assemble;
}

void CreateAfpVertexShader(IDirect3DDevice9* device, GpuContext& gpu) {
    HRESULT hr = 0;
    static const char vs_source[] = "vs.1.1\n"
                                    "dcl_position v0\n"
                                    "dcl_color v1\n"
                                    "dcl_texcoord0 v2\n"
                                    "m4x4 oPos, v0, c0\n"
                                    "mul oD0, v1, c4\n"
                                    "mov oT0.xy, v2\n";

    D3DXAssembleShaderFn pD3DXAssembleShader = ResolveD3dxAssembler();
    if (pD3DXAssembleShader != nullptr) {
        void* blob = nullptr;
        void* errors = nullptr;
        hr = pD3DXAssembleShader(vs_source, sizeof(vs_source) - 1, nullptr, nullptr, 0, &blob,
                                 &errors);
        if (SUCCEEDED(hr) && (blob != nullptr)) {
            void** blob_vtable = *(void***)blob;
            using GetBufferPointerFn = void*(__stdcall*)(void*);
            void* bytecode = ((GetBufferPointerFn)blob_vtable[3])(blob);

            hr = device->CreateVertexShader((DWORD*)bytecode, &gpu.afp_vs);
            using ReleaseFn = void(__stdcall*)(void*);
            ((ReleaseFn)blob_vtable[2])(blob);

            if (SUCCEEDED(hr)) {
                LOG("D3D9", "AFP vertex shader created via D3DXAssembleShader");
            } else {
                LOG("D3D9", "CreateVertexShader failed (hr=0x%08lx)", hr);
            }
        } else {
            LOG("D3D9", "D3DXAssembleShader failed (hr=0x%08lx)", hr);
            if (errors != nullptr) {
                void** err_vtable = *(void***)errors;
                using GetBufFn = void*(__stdcall*)(void*);
                LOG("D3D9", "Errors: %s", (char*)((GetBufFn)err_vtable[3])(errors));
                using RelFn = void(__stdcall*)(void*);
                ((RelFn)err_vtable[2])(errors);
            }
        }
    } else {
        LOG("D3D9", "WARNING: D3DXAssembleShader not available, using fixed-function");
    }
}

const char kHsvPsSource[] = "float4 g_addColor : register(c0);\n"
                            "float4 hsv : register(c1);\n"
                            "float4 scope : register(c2);\n"
                            "float4 hsv2 : register(c3);\n"
                            "float4 scope2 : register(c4);\n"
                            "sampler2D s0 : register(s0);\n"
                            "float3 hueRot(float3 rgb, float4 h){\n"
                            " float ang=frac(h.x/360.0+0.5)*6.28318531-3.14159265;\n"
                            " float mag=h.y*h.z;\n"
                            " float si=sin(ang)*mag, co=cos(ang)*mag;\n"
                            " float Y=dot(rgb, float3(0.299,0.587,0.114));\n"
                            " float I=dot(rgb, float3(0.595716,-0.274453,-0.321263));\n"
                            " float Q=dot(rgb, float3(0.211456,-0.522591,0.311135));\n"
                            " float I2=I*co-Q*si;\n"
                            " float Q2=I*si+Q*co;\n"
                            " float3 r;\n"
                            " r.r=Y+0.9563*I2+0.6210*Q2;\n"
                            " r.g=Y-0.2721*I2-0.6474*Q2;\n"
                            " r.b=Y-1.1070*I2+1.7046*Q2;\n"
                            " return saturate(r);\n"
                            "}\n"
                            "float4 main(float2 uv:TEXCOORD0, float4 diff:COLOR0):COLOR0{\n"
                            " float4 color=tex2D(s0,uv);\n"
                            " float2 lo=step(scope.xz,uv), hi=step(uv,scope.yw);\n"
                            " float k=lo.x*lo.y*hi.x*hi.y;\n"
                            " color.rgb=lerp(color.rgb,hueRot(color.rgb,hsv),k);\n"
                            " float2 lo2=step(scope2.xz,uv), hi2=step(uv,scope2.yw);\n"
                            " float k2=lo2.x*lo2.y*hi2.x*hi2.y;\n"
                            " color.rgb=lerp(color.rgb,hueRot(color.rgb,hsv2),k2);\n"
                            " return color*diff+g_addColor;\n"
                            "}\n";

const char kAddPsSource[] = "sampler2D s0 : register(s0);\n"
                            "float4 main(float2 uv:TEXCOORD0, float4 diff:COLOR0):COLOR0{\n"
                            " float4 c=tex2D(s0,uv)*diff;\n"
                            " float3 g=c.rgb*c.a;\n"
                            " float cov=max(g.r,max(g.g,g.b));\n"
                            " return float4(g,cov);\n"
                            "}\n";

using D3DXCompileShaderFn = HRESULT(WINAPI*)(LPCSTR, UINT, const void*, void*, LPCSTR, LPCSTR,
                                             DWORD, void**, void**, void**);

D3DXCompileShaderFn ResolveD3dxCompiler() {
    static Support::ModuleHandle s_d3dx_mod;
    static D3DXCompileShaderFn s_compile = nullptr;
    static bool s_resolved = false;
    if (s_resolved) return s_compile;
    s_resolved = true;
    const char* d3dx_dlls[] = {"d3dx9_43.dll", "d3dx9_42.dll", "d3dx9_41.dll"};
    for (const char* n : d3dx_dlls) {
        Support::ModuleHandle m = Support::LoadModule(n);
        if (m == nullptr) continue;
        s_compile = (D3DXCompileShaderFn)GetProcAddress(m.get(), "D3DXCompileShader");
        if (s_compile != nullptr) {
            s_d3dx_mod = std::move(m);
            break;
        }
    }
    return s_compile;
}

void CompilePs2b(D3DXCompileShaderFn pCompile, IDirect3DDevice9* device, const char* src, UINT len,
                 IDirect3DPixelShader9** out, const char* name) {
    void* blob = nullptr;
    void* errs = nullptr;
    void* ct = nullptr;
    HRESULT chr = pCompile(src, len, nullptr, nullptr, "main", "ps_2_b", 0, &blob, &errs, &ct);
    if (SUCCEEDED(chr) && (blob != nullptr)) {
        void** bv = *(void***)blob;
        using GetPtrFn = void*(__stdcall*)(void*);
        void* bc = ((GetPtrFn)bv[3])(blob);
        chr = device->CreatePixelShader((DWORD*)bc, out);
        using RelFn = void(__stdcall*)(void*);
        ((RelFn)bv[2])(blob);
        LOG("D3D9", "%s pixel shader: %s", name,
            SUCCEEDED(chr) ? "created (ps_2_b)" : "CreatePixelShader failed");
    } else {
        LOG("D3D9", "%s D3DXCompileShader(ps_2_b) failed (hr=0x%08lx)", name, chr);
        if (errs != nullptr) {
            void** ev = *(void***)errs;
            using GetBufFn = void*(__stdcall*)(void*);
            LOG("D3D9", "%s PS errors: %s", name, (char*)((GetBufFn)ev[3])(errs));
            using RelFn = void(__stdcall*)(void*);
            ((RelFn)ev[2])(errs);
        }
    }
    if (ct != nullptr) {
        void** cv = *(void***)ct;
        using RelFn = void(__stdcall*)(void*);
        ((RelFn)cv[2])(ct);
    }
}

void CreateAfpPixelShaders(IDirect3DDevice9* device, GpuContext& gpu) {
    D3DXCompileShaderFn pCompile = ResolveD3dxCompiler();
    if (pCompile == nullptr) {
        LOG("D3D9", "D3DXCompileShader unavailable; HSL hue filter disabled");
        return;
    }
    CompilePs2b(pCompile, device, kHsvPsSource, sizeof(kHsvPsSource) - 1, &gpu.afp_hsl_ps,
                "AFP HSV");
    CompilePs2b(pCompile, device, kAddPsSource, sizeof(kAddPsSource) - 1, &gpu.afp_add_ps,
                "AFP additive-coverage");
}

}

void CompileAfpShaders(IDirect3DDevice9* device, GpuContext& gpu) {
    if (device == nullptr) return;
    CreateAfpVertexShader(device, gpu);
    CreateAfpPixelShaders(device, gpu);
}

}
