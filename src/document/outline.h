#pragma once

#include "document/timeline.h"
#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

enum class Role : uint8_t {
    Unknown,
    PackageMagic,
    PackageVersion,
    TextureList,
    Texture,
    AnimationList,
    Animation,
    ByteOrderScript,
    Shape,
};

[[nodiscard]] std::string_view RoleName(Role role);

struct Node {
    Ifs::EntryKind kind = Ifs::EntryKind::File;
    Role role = Role::Unknown;
    std::string name;
    std::string stored_name;
    std::string path;
    uint32_t stored_size = 0;
    int32_t time = 0;
    std::optional<uint8_t> super_index;
    std::vector<Node> children;
};

struct TextureDetails {
    std::string format;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct AnimationLabel {
    std::string name;
    uint32_t frame = 0;
};

struct AnimationDetails {
    uint32_t frame_count = 0;
    std::vector<AnimationLabel> labels;
    std::vector<DepthRow> depths;
};

struct Details {
    Role role = Role::Unknown;
    std::string name;
    std::string stored_name;
    std::string path;
    uint32_t stored_size = 0;
    int32_t time = 0;
    std::optional<uint8_t> super_index;
    std::optional<TextureDetails> texture;
    std::optional<AnimationDetails> animation;
};

struct Field {
    std::string name;
    std::string value;
};

[[nodiscard]] std::vector<Field> Fields(const Details& details);

class Outline {
public:
    [[nodiscard]] static Outline Build(const Ifs::Archive& archive);

    [[nodiscard]] const std::vector<Node>& Nodes() const { return nodes_; }

    [[nodiscard]] const std::vector<std::string>& Problems() const { return problems_; }

    [[nodiscard]] Support::Expected<Details, std::string> Describe(const Ifs::Archive& archive,
                                                                   std::string_view path) const;

private:
    void NameTextures(const Ifs::Archive& archive);
    void NameAnimations(const Ifs::Archive& archive);

    std::vector<Node> nodes_;
    std::vector<std::string> problems_;
    std::map<std::string, TextureDetails, std::less<>> textures_;
};

}
