#pragma once

#include "state/app_state.h"
#include "engine_session.h"
#include "render_backend.h"

namespace Export {

void OnMainLoopTick(EngineSession& es, D3D9State& d3d);

void HandleStartRequest(const App::Request& req, EngineSession& es, D3D9State& d3d);
void HandleCancelRequest(D3D9State& d3d);

bool IsCapturing();

int TargetFps();

}
