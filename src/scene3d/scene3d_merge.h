#pragma once

#include "scene3d/scene3d.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Scene3d {

void Merge(Scene& into, Scene other);

struct Instance {
    std::string target;
    std::string model;
};

struct InstanceProblem {
    std::size_t index = 0;
    bool missing_model = false;
};

std::vector<InstanceProblem> MakeInstances(std::vector<Model>& models,
                                           const std::vector<Instance>& instances);

}
