#pragma once

#include "gc2d/gc_host.h"
#include "iidx_playfield.h"

#include <string>
#include <vector>

namespace Gc2dPlayfield {

void Enable(const std::string& parts_asset, const IidxPlayfield::Values& values);

[[nodiscard]] bool Enabled();

void Append(std::vector<Gc2dHost::SpritePlacement>& sprites);

}
