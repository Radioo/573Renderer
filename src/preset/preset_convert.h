#pragma once

#include "preset/doc/preset_document.h"
#include "preset/preset_asset_lengths.h"
#include "preset/scene_preset.h"

namespace Preset {

Doc::Document FromScene(const Scene& scene, const AssetLengths& lengths);

}
