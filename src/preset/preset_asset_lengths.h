#pragma once

#include <map>
#include <string>

namespace Preset {

struct AssetLengths {
    std::map<std::string, float> scene_ticks;
    std::map<std::string, std::map<std::string, int>> animation_frames;

    float MaxTime(const std::string& dir) const {
        const auto found = scene_ticks.find(dir);
        return (found == scene_ticks.end()) ? 0.0F : found->second;
    }

    int AnimationLength(const std::string& dir, const std::string& animation) const {
        const auto package = animation_frames.find(dir);
        if (package == animation_frames.end()) return 0;
        const auto found = package->second.find(animation);
        return (found == package->second.end()) ? 0 : found->second;
    }
};

}
