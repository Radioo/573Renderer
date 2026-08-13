#pragma once

#include "export_capture.h"
#include "state/commands.h"

struct D3D9State;

namespace Export {

void OnMainLoopTick(D3D9State& d3d);

void HandleStartRequest(const App::ExportRequest& req, D3D9State& d3d);
void HandleCancelRequest(D3D9State& d3d);

bool IsCapturing();

[[nodiscard]] int PlannedFrames(int max_frames, int preset_frames, int package_frames);

Capabilities ActiveCapabilities();

int TargetFps();

}
