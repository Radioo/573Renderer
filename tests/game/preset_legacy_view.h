#pragma once

#include "preset/eval/eval_push.h"
#include "preset/scene_preset.h"

#include <string>
#include <vector>

namespace PresetLegacy {

struct FrameCalls {
    std::vector<std::string> calls;
    float countdown_drift = 0.0F;
};

class Adapter {
public:
    explicit Adapter(const Preset::Scene& scene);

    FrameCalls Filter(const std::vector<Preset::Eval::Push>& pushes, int record_frame) const;

    bool Excluded(int record_frame) const;

    std::vector<std::string> StripHidden(const std::vector<std::string>& calls,
                                         int record_frame) const;

private:
    int PhaseAt(int frame) const;

    const Preset::Scene& scene_;
    std::vector<std::vector<std::string>> hidden_movers_;
    std::string lead_;
};

}
