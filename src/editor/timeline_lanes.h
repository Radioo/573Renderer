#pragma once

#include "preset/doc/preset_document.h"

namespace Editor {

int LaneCount(const Preset::Doc::Track& track);

int LaneOf(const Preset::Doc::Track& track, const Preset::Doc::Clip& clip);

}
