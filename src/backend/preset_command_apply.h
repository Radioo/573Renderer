#pragma once

#include <any>
#include <functional>
#include <string>

namespace Backend {

struct LoadReporter {
    std::function<void(const std::string& what)> begin;
    std::function<void(const std::string& stage, float fraction)> stage;
    std::function<void()> end;
};

bool ApplyPresetCommand(const std::any& payload, const LoadReporter& reporter = {});

}
