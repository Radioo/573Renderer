#pragma once

#include "preset/scene_preset.h"

#include <string>
#include <vector>

namespace PresetHost {

struct Status {
    std::string id;
    std::string name;
    int countdown = 0;
    int countdown_start = 0;
    float model_speed = 0.0F;
    float model_alpha = 1.0F;
    int blend_mode = 0;
    std::vector<int> option_choices;
};

bool Load(const std::string& game_dir, const Preset::Scene& scene);

void Unload();

bool Active();

void RenderFrame(float dt);

Status GetStatus();

void SetCountdown(int frames);

void SetOption(int option, int choice);

int NaturalFrames();

void Restart();

}
