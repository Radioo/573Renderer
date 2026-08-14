#include "preset/scene_preset.h"

#include "preset/scene_registry.h"

#include <span>
#include <string_view>
#include <vector>

namespace Preset {

std::vector<const Scene*> ForBuild(std::string_view build) {
    std::vector<const Scene*> matched;
    for (const std::span<const Scene> group : {Iidx10Scenes(), Iidx11Scenes()}) {
        for (const Scene& scene : group) {
            if (scene.build == build) matched.push_back(&scene);
        }
    }
    return matched;
}

}
