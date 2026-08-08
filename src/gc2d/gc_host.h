#pragma once

#include <string>
#include <vector>

namespace Gc2dHost {

struct Status {
    std::string package;
    std::string animation;
    int cells = 0;
    int records = 0;
    int animations = 0;
    int tiles = 0;
    int frame = 0;
    int length = 0;
    int draw_nodes = 0;
    bool paused = false;
    float speed = 1.0F;
};

bool Load(const std::string& dir);

void Unload();

bool Active();

void RenderFrame(float dt);

Status GetStatus();

std::vector<std::string> ListAnimations();

void SelectAnimation(const std::string& name);

void SetPaused(bool on);

void SetFrame(int frame);

void SetSpeed(float speed);

}
