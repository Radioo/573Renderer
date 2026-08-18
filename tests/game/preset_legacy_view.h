#pragma once

#include "preset/eval/eval_push.h"

#include <map>
#include <string>
#include <vector>

namespace PresetLegacy {

struct Countdown {
    int start_frames = 0;
    int ramp_below = 0;
    float speed_base = 1.0F;
    float speed_per_frame = 0.0F;
    float fade_from = 1.0F;
    float fade_per_frame = 0.0F;
};

struct Compat {
    std::string lead = {};
    Countdown countdown = {};
    std::vector<int> phase_starts = {};
    std::vector<std::vector<std::string>> hidden_movers = {};
};

std::map<std::string, Compat> LoadCompat(const std::string& text, std::string& err);

struct FrameCalls {
    std::vector<std::string> calls;
    float countdown_drift = 0.0F;
};

class Adapter {
public:
    explicit Adapter(Compat compat);

    FrameCalls Filter(const std::vector<Preset::Eval::Push>& pushes, int record_frame) const;

    bool Excluded(int record_frame) const;

    std::vector<std::string> StripHidden(const std::vector<std::string>& calls,
                                         int record_frame) const;

private:
    int PhaseAt(int frame) const;

    Compat compat_;
};

}
