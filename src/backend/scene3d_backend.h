#pragma once

#include "backend/backend.h"

#include <memory>

struct D3D9State;

namespace Backend {

std::unique_ptr<IBackend> MakeScene3dBackend();

void ApplyPresetClearColor(D3D9State& d3d, bool exporting);

}
