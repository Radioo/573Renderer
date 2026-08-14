#pragma once

#include "preset/scene_preset.h"

#include <array>
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

struct ParamView {
    std::string id;
    std::string label;
    std::string group;
    std::string unit;
    std::string help;
    std::string aliases;
    int kind = 0;
    float min = 0.0F;
    float max = 0.0F;
    float step = 0.0F;
    bool soft = false;
    std::array<float, 3> value = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> fallback = {0.0F, 0.0F, 0.0F};
    int ivalue = 0;
    int ifallback = 0;
    bool overridden = false;
    std::vector<std::string> enum_labels;
};

struct StateView {
    std::string id;
    std::string label;
    std::vector<std::string> choices;
    int choice = 0;
    bool moves_camera = false;
};

std::vector<StateView> ListStates();

std::vector<ParamView> ListParams();

void SetParam(const std::string& id, const std::array<float, 3>& value, int ivalue);

void ResetParam(const std::string& id);

void ResetGroup(const std::string& group);

void ResetAllParams();

int ChangedParamCount();

int LoadTweaks(const std::string& path);

void SetOption(int option, int choice);

int NaturalFrames();

void Restart();

}
