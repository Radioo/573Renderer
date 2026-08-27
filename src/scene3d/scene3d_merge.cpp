#include "scene3d/scene3d_merge.h"

#include "scene3d/atlas.h"
#include "scene3d/scene3d.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Scene3d {

void Merge(Scene& into, Scene other) {
    const bool first = into.models.empty() && into.tiles.empty();
    const auto tile_base = (int)into.tiles.size();
    const auto model_base = (int)into.models.size();

    if (first) {
        into.bounds_min = other.bounds_min;
        into.bounds_max = other.bounds_max;
    } else {
        for (std::size_t k = 0; k < into.bounds_min.size(); k++) {
            into.bounds_min[k] = std::min(into.bounds_min[k], other.bounds_min[k]);
            into.bounds_max[k] = std::max(into.bounds_max[k], other.bounds_max[k]);
        }
    }
    into.max_time = std::max(into.max_time, other.max_time);
    if (into.camera_model < 0 && other.camera_model >= 0) {
        into.camera_model = other.camera_model + model_base;
        into.camera_frame = other.camera_frame;
    }

    for (Model& model : other.models) {
        for (DrawChunk& chunk : model.chunks) {
            if (chunk.tile >= 0) chunk.tile += tile_base;
        }
        into.models.push_back(std::move(model));
    }
    for (Tile& tile : other.tiles)
        into.tiles.push_back(std::move(tile));
    if (into.name.empty()) into.name = other.name;
}

namespace {

Model* Named(std::vector<Model>& models, const std::string& name) {
    for (Model& model : models) {
        if (model.name == name) return &model;
    }
    return nullptr;
}

}

std::vector<InstanceProblem> MakeInstances(std::vector<Model>& models,
                                           const std::vector<Instance>& instances) {
    std::vector<InstanceProblem> problems;
    std::set<std::string> cloned;
    for (std::size_t i = 0; i < instances.size(); i++) {
        const Instance& want = instances[i];
        if (want.target == want.model || cloned.contains(want.target)) continue;
        const Model* mesh = Named(models, want.model);
        if (mesh == nullptr) {
            problems.push_back(InstanceProblem{.index = i, .missing_model = true});
            continue;
        }
        Model clone = *mesh;
        clone.name = want.target;
        Model* taken = Named(models, want.target);
        if (taken == nullptr) {
            models.push_back(std::move(clone));
        } else {
            problems.push_back(InstanceProblem{.index = i, .missing_model = false});
            *taken = std::move(clone);
        }
        cloned.insert(want.target);
    }
    return problems;
}

}
