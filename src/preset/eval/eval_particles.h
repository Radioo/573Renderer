#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_rng.h"

#include <array>
#include <string>
#include <vector>

namespace Preset::Eval {

struct Particle {
    std::string asset;
    std::string cell;
    int from_x = 320;
    int from_y = 240;
    int to_x = 0;
    int to_y = 0;
    int life = 1;
    int age = -1;
    int priority = 0;
    int blend = 0;
    float scale = 1.0F;
};

struct BeatSnapshot {
    std::array<int, 2> index = {0, 0};
    std::array<int, 2> since = {0, 0};
};

void AgeParticles(std::vector<Particle>& particles);

void SpawnParticles(const FrameState& state, int frame, const BeatSnapshot& beat, int canvas_w,
                    int canvas_h, Preset::Ran3& rng, std::vector<Particle>& particles);

float DrawJitter(const JitterState& jitter, Preset::Ran3& rng);

void CollectParticles(const std::vector<Particle>& particles, int split, bool behind_models,
                      std::vector<CellDraw>& cells);

}
