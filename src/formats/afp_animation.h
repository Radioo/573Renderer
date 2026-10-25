#pragma once

#include "formats/afp_byte_order.h"
#include "support/expected.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace AfpAnimation {

using StringId = uint32_t;

struct Label {
    uint16_t frame = 0;
    StringId name = 0;

    friend bool operator==(const Label&, const Label&) = default;
};

struct Frame {
    uint32_t first_tag = 0;
    uint32_t tag_count = 0;

    friend bool operator==(const Frame&, const Frame&) = default;
};

struct Bytecode {
    uint8_t flags = 0;
    std::optional<std::vector<StringId>> strings;
    std::vector<uint8_t> code;

    friend bool operator==(const Bytecode&, const Bytecode&) = default;
};

struct Hsv {
    int16_t hue = 0;
    int8_t saturation = 0;
    int8_t value = 0;

    friend bool operator==(const Hsv&, const Hsv&) = default;
};

struct ClipEvent {
    uint32_t triggers = 0;
    std::array<uint8_t, 2> unread_bytes{};
    Bytecode bytecode;

    friend bool operator==(const ClipEvent&, const ClipEvent&) = default;
};

struct ClipActions {
    uint32_t unread_value = 0;
    uint16_t unread_word = 0;
    std::vector<ClipEvent> events;

    friend bool operator==(const ClipActions&, const ClipActions&) = default;
};

struct ColourMatrixFilter {
    std::array<uint8_t, 4> head{};
    std::array<int32_t, 20> matrix{};
    std::optional<Hsv> hsv;

    friend bool operator==(const ColourMatrixFilter&, const ColourMatrixFilter&) = default;
};

struct LookupFilter {
    std::array<uint8_t, 6> head{};
    std::array<uint8_t, 4> unread_bytes{};
    std::vector<uint8_t> table;

    friend bool operator==(const LookupFilter&, const LookupFilter&) = default;
};

struct UnknownFilter {
    std::vector<uint8_t> bytes;

    friend bool operator==(const UnknownFilter&, const UnknownFilter&) = default;
};

using Filter = std::variant<ColourMatrixFilter, LookupFilter, UnknownFilter>;

struct Curve {
    uint8_t slot = 0;
    uint16_t flags = 0;
    std::vector<int32_t> values;

    friend bool operator==(const Curve&, const Curve&) = default;
};

struct DiscardedWords {
    uint32_t mask = 0;
    std::vector<uint16_t> values;

    friend bool operator==(const DiscardedWords&, const DiscardedWords&) = default;
};

struct ColourController {
    std::array<uint8_t, 4> colour{};
    int16_t first = 0;
    int16_t second = 0;

    friend bool operator==(const ColourController&, const ColourController&) = default;
};

struct GridController {
    uint16_t tag = 0;
    int16_t first = 0;
    int16_t second = 0;

    friend bool operator==(const GridController&, const GridController&) = default;
};

struct Placement {
    uint32_t flags = 0;
    uint16_t depth = 0;
    uint16_t end_frame = 0;
    std::optional<uint32_t> extended_flags;
    std::optional<uint16_t> character;
    std::optional<uint16_t> ratio;
    std::optional<StringId> name;
    std::optional<uint16_t> clip_depth;
    std::optional<uint8_t> blend;
    std::optional<std::array<int32_t, 2>> scale;
    std::optional<std::array<int32_t, 2>> rotate_skew;
    std::optional<std::array<int32_t, 2>> translation;
    std::optional<std::array<int16_t, 4>> multiply_colour;
    std::optional<std::array<int16_t, 4>> add_colour;
    std::optional<uint32_t> packed_multiply_colour;
    std::optional<uint32_t> packed_add_colour;
    std::optional<ClipActions> clip_actions;
    std::optional<std::vector<Filter>> filters;
    std::optional<std::array<int32_t, 2>> origin;
    std::optional<int32_t> origin_z;
    std::optional<uint32_t> geometry;
    std::optional<std::array<int16_t, 2>> short_scale;
    std::optional<std::array<int16_t, 2>> short_rotate_skew;
    std::optional<StringId> class_name;
    std::optional<int32_t> translation_z;
    std::optional<std::array<int32_t, 9>> matrix_3d;
    std::optional<Hsv> hsv;
    std::optional<DiscardedWords> discarded_words;
    std::optional<std::vector<Curve>> curves;
    std::optional<ColourController> colour_controller;
    std::optional<GridController> grid_controller;

    friend bool operator==(const Placement&, const Placement&) = default;
};

struct Remove {
    uint16_t unread_word = 0;
    uint16_t depth = 0;

    friend bool operator==(const Remove&, const Remove&) = default;
};

struct Image {
    uint32_t flags = 0;
    uint16_t id = 0;
    StringId name = 0;

    friend bool operator==(const Image&, const Image&) = default;
};

struct Shape {
    uint16_t unread_word = 0;
    uint16_t id = 0;

    friend bool operator==(const Shape&, const Shape&) = default;
};

struct Camera {
    uint16_t id = 0;
    std::optional<std::array<int32_t, 3>> position;
    std::optional<int32_t> focal_length;

    friend bool operator==(const Camera&, const Camera&) = default;
};

struct Action {
    Bytecode bytecode;

    friend bool operator==(const Action&, const Action&) = default;
};

struct UnknownTag {
    uint16_t code = 0;
    std::vector<uint8_t> bytes;

    friend bool operator==(const UnknownTag&, const UnknownTag&) = default;
};

struct Tag;

struct Container {
    std::vector<Label> labels;
    std::optional<std::vector<Label>> script_labels;
    std::vector<Frame> frames;
    std::vector<Tag> tags;

    friend bool operator==(const Container&, const Container&) = default;
};

struct Sprite {
    uint16_t id = 0;
    Container container;

    friend bool operator==(const Sprite&, const Sprite&) = default;
};

struct Tag {
    std::variant<Sprite, Action, Placement, Remove, Image, Shape, Camera, UnknownTag> body;

    friend bool operator==(const Tag&, const Tag&) = default;
};

struct Export {
    uint16_t tag = 0;
    StringId name = 0;

    friend bool operator==(const Export&, const Export&) = default;
};

struct ImportedAsset {
    uint16_t tag = 0;
    StringId name = 0;

    friend bool operator==(const ImportedAsset&, const ImportedAsset&) = default;
};

struct Import {
    StringId movie = 0;
    std::vector<ImportedAsset> assets;

    friend bool operator==(const Import&, const Import&) = default;
};

struct ImportInitializer {
    uint16_t tag = 0;
    uint16_t frame = 0;

    friend bool operator==(const ImportInitializer&, const ImportInitializer&) = default;
};

struct ImportInitializers {
    uint16_t leading_word = 0;
    std::vector<ImportInitializer> entries;

    friend bool operator==(const ImportInitializers&, const ImportInitializers&) = default;
};

struct StoredForm {
    bool strings_scrambled = true;
    bool background_colour_swapped = true;

    friend bool operator==(const StoredForm&, const StoredForm&) = default;
};

struct Animation {
    uint8_t container_version = 0;
    std::array<uint8_t, 3> magic{};
    uint16_t data_version = 0;
    StringId name = 0;
    uint32_t flags = 0;
    std::array<uint16_t, 4> rect{};
    uint32_t fps = 0;
    std::array<uint8_t, 4> background_colour{};
    std::vector<Export> exports;
    std::vector<Import> imports;
    std::optional<ImportInitializers> import_initializers;
    std::vector<std::string> strings;
    Container root;
    StoredForm stored_form;

    friend bool operator==(const Animation&, const Animation&) = default;
};

struct Native {
    std::vector<uint8_t> data;
    Support::Expected<std::vector<AfpByteOrder::Swap>, std::string> swaps;
};

struct Stored {
    std::vector<uint8_t> data;
    std::vector<uint8_t> script;
};

[[nodiscard]] Support::Expected<Animation, std::string> Read(std::span<const uint8_t> native);

[[nodiscard]] Support::Expected<Native, std::string> Write(const Animation& animation);

[[nodiscard]] Support::Expected<Animation, std::string> ReadStored(std::span<const uint8_t> stored,
                                                                   std::span<const uint8_t> script);

[[nodiscard]] Support::Expected<Stored, std::string> WriteStored(const Animation& animation);

}
