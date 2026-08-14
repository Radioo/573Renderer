#pragma once

#include "formats/gcanim.h"

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

std::vector<std::string> ListCells();

std::vector<std::string> ListParts(const std::string& animation);

struct DrawInfo {
    std::string cell;
    int blend = 0;
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;
    float alpha = 1.0F;
};

std::vector<DrawInfo> ListDrawNodes();

int AnimationLength(const std::string& animation);

struct SpritePlacement {
    std::string name;
    bool animated = false;
    int priority = 0;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    GcAnim::Blend blend = GcAnim::Blend::Normal;
    GcAnim::Timing timing = {};
    std::vector<std::string> skip_parts;
    float time = 0.0F;
    float scroll_x = 0.0F;
    float scroll_wrap = 0.0F;
};

bool SelectAnimation(const std::string& name);

struct SpriteStatus {
    std::string name;
    int frame = 0;
    int length = 0;
    int playhead = 0;
    int scroll = 0;
    int scroll_wrap = 0;
};

struct CellDraw {
    std::string name;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    int blend = 0;
};

bool LoadParticles(const std::string& dir);

void DrawParticles(const std::vector<CellDraw>& cells);

void SetSprites(std::vector<SpritePlacement> sprites);

std::vector<SpriteStatus> ListSprites();

void SetSpriteFrame(int index, int frame);

void SetSpriteScroll(int index, int offset);

void SetSpriteScale(int index, float scale);

void AdvanceSprites(float dt);

void DrawSprites(int min_priority, int max_priority);

void SetPaused(bool on);

void SetFrame(int frame);

void SetSpeed(float speed);

}
