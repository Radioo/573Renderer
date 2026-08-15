#pragma once

#include "preset/doc/preset_enum_names.h"

#include <string>
#include <vector>

namespace Preset {

struct AssetAnimation {
    std::string name;
    int frames = 0;
};

struct AssetEntry {
    std::string id;
    Doc::AssetKind kind = Doc::AssetKind::Package2d;
    std::string dir;
    bool loaded = false;
    std::vector<std::string> models;
    std::vector<std::string> cells;
    std::vector<AssetAnimation> animations;
};

struct AssetIndex {
    std::vector<AssetEntry> assets;
};

}
