#pragma once

#include <string>

namespace IidxPlayfield {

enum class Gauge : int { Normal = 0, Hard = 1, ExHard = 2 };

struct Values {
    int score = 0;
    int max_combo = 0;
    int bpm = 0;
    int bpm_min = 0;
    int bpm_max = 0;
    int percent = 0;
    int hispeed = 0;
    int stage = 0;
    int difficulty = 0;
    Gauge gauge = Gauge::Normal;
    bool double_play = false;
    bool key_lights = false;
    std::string effect;
};

bool Parse(const std::string& spec, Values& out, std::string& err);

void Enable(const Values& values);

bool Enabled();

void Apply();

}
