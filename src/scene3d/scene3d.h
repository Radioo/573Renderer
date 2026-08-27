#pragma once

#include "formats/inz.h"
#include "formats/xfile.h"
#include "scene3d/atlas.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Scene3d {

struct DrawChunk {
    int frame = 0;
    int tile = -1;
    std::array<float, 4> diffuse = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 3> emissive = {0.0F, 0.0F, 0.0F};
    std::vector<float> vertices;
    std::vector<uint16_t> indices;
};

constexpr int kBlendOpaque = 0;
constexpr int kBlendAlpha = 2;
constexpr int kBlendAdditive = 3;
constexpr int kBlendSubtract = 4;

struct Model {
    std::string name;
    int blend_mode = kBlendOpaque;
    bool visible = true;
    float alpha = 1.0F;
    float anim_speed = 1.0F;
    float time = 0.0F;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
    XFile::Scene scene;
    std::vector<DrawChunk> chunks;
};

struct Scene {
    std::string name;
    Inz::Manifest manifest;
    std::vector<Tile> tiles;
    std::vector<Model> models;
    int camera_model = -1;
    int camera_frame = -1;
    float max_time = 0.0F;
    std::array<float, 3> bounds_min = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> bounds_max = {0.0F, 0.0F, 0.0F};
};

constexpr int kVertexFloats = 8;

bool IsSceneDir(const std::string& dir);

bool Load(const std::string& dir, Scene& out, std::string& err);

XFile::Matrix ModelTransform(const Model& model);

}
