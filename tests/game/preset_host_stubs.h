#pragma once

#include "gc2d/gc_host.h"
#include "gc2d/gc_sprite.h"
#include "scene3d/scene3d_render.h"

#include <string>
#include <vector>

namespace PresetStub {

Gc2d::Canvas Canvas();

std::vector<Gc2dHost::SpritePlacement> Sprites();

std::vector<Scene3d::Light> Lights();

bool LoadAssetLengths(const std::string& path, std::string& err);

void Reset();

std::vector<std::string> Take();

const std::string& Error();

}
