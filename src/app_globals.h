#pragma once

#include "engine_session.h"
#include "gpu_context.h"

inline EngineSession g_engine;

inline DllLoader& g_avs_dll = g_engine.avs_dll;
inline DllLoader& g_afp_dll = g_engine.afp_dll;
inline DllLoader& g_afpu_dll = g_engine.afpu_dll;

inline AvsFuncs& g_avs = g_engine.avs;
inline AfpFuncs& g_afp = g_engine.afp;
inline AfpuFuncs& g_afpu = g_engine.afpu;

inline D3D9State& g_d3d = g_gpu.d3d;
inline AfpRenderContext& g_render_ctx = g_engine.render_ctx;
