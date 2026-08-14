#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace SysIdx {

constexpr int kMaxTextures = 12;
constexpr int kAtlasWidth = 1024;
constexpr int kAtlasTileHeight = 1024;

constexpr uint16_t kFlagAlphaTrack = 0x0002;
constexpr uint16_t kFlagSubtract = 0x0010;

constexpr int16_t kRecDrawCell = 0;
constexpr int16_t kRecNested = 2;
constexpr int16_t kRecExternal = 3;
constexpr int16_t kRecEndAnimation = -1;
constexpr int16_t kRecEndTable = -2;

struct Cell {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
};

struct Key {
    int16_t t = 0;
    int16_t a = 0;
    int16_t b = 0;
};

struct Record {
    int16_t type = 0;
    int16_t id = 0;
    uint16_t flags = 0;
    int16_t duration = 0;
    int16_t t_start = 0;
    int16_t t_end = 0;
    int16_t t_base = 0;
    int16_t anchor_x = 0;
    int16_t anchor_y = 0;
    std::vector<Key> position;
    std::vector<Key> scale;
    std::vector<Key> alpha;
    std::vector<Key> rotation;
};

struct Package {
    uint16_t texture_count = 0;
    std::vector<std::string> texture_paths;
    std::vector<Cell> cells;
    std::vector<Record> records;
    std::unordered_map<std::string, uint16_t> cell_names;
    std::unordered_map<std::string, uint16_t> animation_names;
};

bool Parse(std::span<const uint8_t> file, Package& out, std::string& err);

int AnimationLength(const Package& pkg, size_t start_index);

int AnimationContentEnd(const Package& pkg, size_t start_index);

}
