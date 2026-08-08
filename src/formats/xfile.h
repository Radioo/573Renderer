#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace XFile {

using Matrix = std::array<float, 16>;

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Vec2 {
    float u = 0.0F;
    float v = 0.0F;
};

struct Material {
    std::array<float, 4> diffuse = {1.0F, 1.0F, 1.0F, 1.0F};
    std::string texture;
    std::string ref;
};

struct Mesh {
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> face_material;
    std::vector<uint32_t> triangle_face;
    std::vector<Material> materials;
};

struct Frame {
    std::string name;
    Matrix transform{};
    std::vector<Mesh> meshes;
    std::vector<int> children;
    int parent = -1;
};

struct AnimationKey {
    int time = 0;
    std::array<float, 16> value{};
};

struct AnimationChannel {
    std::string frame_name;
    std::vector<AnimationKey> rotation;
    std::vector<AnimationKey> scale;
    std::vector<AnimationKey> position;
    std::vector<AnimationKey> matrix;
};

struct NamedMaterial {
    std::string name;
    Material material;
};

struct Scene {
    std::vector<Frame> frames;
    std::vector<NamedMaterial> named_materials;
    std::vector<AnimationChannel> channels;
    int max_key_time = 0;
};

bool Parse(const std::string& text, Scene& out, std::string& err);

Matrix Identity();

}
