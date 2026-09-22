#include <catch2/catch_test_macros.hpp>

#include "formats/afp_animation.h"
#include "formats/afp_byte_order.h"
#include "formats/little_endian.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace AfpAnimation;

using LittleEndian::ReadU16;
using LittleEndian::ReadU32;

constexpr std::size_t kTinyTagHeader = 80;
constexpr std::size_t kTinyTagData = 84;

Animation TinyAnimation(Tag tag) {
    Animation a;
    a.container_version = 8;
    a.magic = {0xB2, 0xD0, 0xC1};
    a.data_version = 0x200;
    a.strings = {""};
    a.root.tags = {std::move(tag)};
    return a;
}

std::vector<uint8_t> GrowTag(std::vector<uint8_t> data, std::size_t at, std::size_t bytes) {
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(at), bytes, 0);
    const uint32_t header = ReadU32(data, kTinyTagHeader);
    LittleEndian::WriteU32(data, kTinyTagHeader, header + static_cast<uint32_t>(bytes));
    LittleEndian::WriteU32(data, 48, ReadU32(data, 48) + static_cast<uint32_t>(bytes));
    LittleEndian::WriteU32(data, 4, static_cast<uint32_t>(data.size()));
    return data;
}

Bytecode CallBytecode() {
    return Bytecode{.flags = 0,
                    .strings = std::vector<StringId>{4},
                    .code = {0x10, 0x20, 0x00, 0x00, 0x00, 0x00}};
}

Placement FullPlacement() {
    Placement p;
    p.flags = 0x1 | 0x4 | 0x8;
    p.depth = 3;
    p.end_frame = 9;
    p.extended_flags = 0x1;
    p.character = uint16_t{7};
    p.ratio = uint16_t{2};
    p.name = StringId{5};
    p.clip_depth = uint16_t{4};
    p.blend = uint8_t{8};
    p.scale = {{1024, -1024}};
    p.rotate_skew = {{12, 34}};
    p.translation = {{-2000, 400}};
    p.multiply_colour = {{255, 128, 64, -1}};
    p.add_colour = {{1, 2, 3, 4}};
    p.packed_multiply_colour = 0xFF804020;
    p.packed_add_colour = 0x01020304;
    p.clip_actions = ClipActions{
        .unread_value = 1,
        .unread_word = 0,
        .events = {ClipEvent{.triggers = 1, .unread_bytes = {0, 0}, .bytecode = CallBytecode()},
                   ClipEvent{
                       .triggers = 2,
                       .unread_bytes = {3, 4},
                       .bytecode =
                           Bytecode{.flags = 2, .strings = std::nullopt, .code = {0x07, 0x00}}}},
    };
    ColourMatrixFilter matrix{.head = {6, 1, 0x64, 0}, .matrix = {}, .hsv = std::nullopt};
    matrix.matrix[0] = 65536;
    matrix.matrix[19] = -7;
    matrix.hsv = Hsv{.hue = -30, .saturation = -5, .value = 12};
    LookupFilter lookup{
        .head = {0x67, 1, 0, 0, 0x1C, 2}, .unread_bytes = {1, 0, 1, 0}, .table = {}};
    lookup.table.assign(256, 0xAB);
    p.filters = std::vector<Filter>{
        matrix, lookup,
        ColourMatrixFilter{.head = {6, 0, 0, 0}, .matrix = {}, .hsv = std::nullopt}};
    p.origin = {{10, 20}};
    p.origin_z = -30;
    p.geometry = 0xDEADBEEF;
    p.short_scale = {{16384, -16384}};
    p.short_rotate_skew = {{-1, 1}};
    p.class_name = StringId{5};
    p.translation_z = 77;
    p.matrix_3d = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
    p.hsv = Hsv{.hue = 180, .saturation = 1, .value = -1};
    p.discarded_words = DiscardedWords{.mask = 0x5, .values = {0x1111, 0x2222}};
    p.curves = std::vector<Curve>{
        Curve{.slot = 1, .flags = 0, .values = {1, -2, 3, -4}},
        Curve{.slot = 30, .flags = 0x11, .values = {100000, 2, 3, 4, 5, 6}},
    };
    p.colour_controller =
        ColourController{.colour = {0x3A, 0x00, 0x9A, 0xFF}, .first = -1, .second = 2};
    p.grid_controller = GridController{.tag = 12, .first = 3, .second = -4};
    return p;
}

Placement MinimalPlacement() {
    Placement p;
    p.flags = 0x1;
    p.depth = 4;
    p.end_frame = 1;
    return p;
}

Animation SampleAnimation() {
    Animation a;
    a.container_version = 8;
    a.magic = {0xB2, 0xD0, 0xC1};
    a.data_version = 0x200;
    a.flags = 0xC3;
    a.rect = {0, 1920, 0, 1080};
    a.fps = 60 * 1024;
    a.background_colour = {0, 0, 0, 0xFF};
    a.strings = {"", "movie", "__Packages.aeplib", "aeplib", "aep_set_frame", "layer", "aep_dummy"};
    a.name = 1;
    a.exports = {Export{.tag = 0, .name = 1}};
    a.imports = {Import{.movie = 2, .assets = {ImportedAsset{.tag = 1, .name = 3}}}};
    a.import_initializers =
        ImportInitializers{.leading_word = 0, .entries = {ImportInitializer{.tag = 2}}};

    Container sprite_body;
    sprite_body.frames = {Frame{.first_tag = 0, .tag_count = 1}};
    sprite_body.tags = {Tag{Remove{.unread_word = 0, .depth = 3}}};

    a.root.labels = {Label{.frame = 1, .name = 5}};
    a.root.script_labels = std::vector<Label>{Label{.frame = 0, .name = 4}};
    a.root.frames = {Frame{.first_tag = 3, .tag_count = 3}, Frame{.first_tag = 6, .tag_count = 2}};
    a.root.tags = {
        Tag{Sprite{.id = 1, .container = sprite_body}},
        Tag{Shape{.unread_word = 2, .id = 2}},
        Tag{Image{.flags = 0, .id = 3, .name = 5}},
        Tag{FullPlacement()},
        Tag{Action{.bytecode = CallBytecode()}},
        Tag{Camera{.id = 1, .position = {{1, 2, 3}}, .focal_length = 400}},
        Tag{Remove{.unread_word = 0, .depth = 3}},
        Tag{MinimalPlacement()},
    };
    return a;
}

}

TEST_CASE("Write then Read reproduces every modelled field") {
    const Animation animation = SampleAnimation();
    const auto native = Write(animation);
    REQUIRE(native.has_value());
    const auto back = Read(native->data);
    REQUIRE(back.has_value());
    CHECK(*back == animation);
}

TEST_CASE("Write lays out the header, tables and records") {
    const auto native = Write(SampleAnimation());
    REQUIRE(native.has_value());
    const std::vector<uint8_t>& d = native->data;
    CHECK(ReadU32(d, 0) == 0xC1D0B208);
    CHECK(ReadU32(d, 4) == d.size());
    CHECK(ReadU32(d, 12) == 0xC7);
    CHECK(ReadU32(d, 40) == 60);
    CHECK(ReadU32(d, 44) == 64);
    CHECK(ReadU32(d, 56) == 72);
    CHECK(ReadU16(d, 72) == 0);
    CHECK(ReadU16(d, 74) == 1);
    CHECK(ReadU32(d, 36) == 88);
    const std::size_t root = 88;
    CHECK(ReadU16(d, root) == 4);
    CHECK(ReadU32(d, root + 12) == 28);
    CHECK(ReadU32(d, root + 16) == 36);
    CHECK(ReadU32(d, root + 20) == 44);
    CHECK(ReadU32(d, root + 36) == ((3U << 20U) | 3U));
    CHECK(ReadU32(d, root + 44) >> 22U == 121);
    CHECK((ReadU32(d, root + 44) & 0x3FFFFF) % 4 == 0);
    const std::size_t table = ReadU32(d, 48);
    CHECK(ReadU32(d, 52) == 4 + 8 + 20 + 8 + 16 + 8 + 12);
    CHECK(table + ReadU32(d, 52) == d.size());
    CHECK(ReadU16(d, 10) == 4);
    CHECK(d[table + 4] == 'm');
}

TEST_CASE("Str references pack the high offset bits into the low bits") {
    Animation animation = SampleAnimation();
    animation.strings.resize(7);
    animation.strings.emplace_back(70000, 'x');
    animation.strings.emplace_back("far");
    animation.exports.push_back(Export{.tag = 9, .name = 8});
    const auto native = Write(animation);
    REQUIRE(native.has_value());
    const std::size_t far_off = 4 + 8 + 20 + 8 + 16 + 8 + 12 + ((70000 + 4) & ~3U);
    CHECK(ReadU16(native->data, 60 + 6) == ((far_off & 0xFFFC) | (far_off >> 16U)));
    const auto back = Read(native->data);
    REQUIRE(back.has_value());
    CHECK(back->exports[1].name == 8);
}

TEST_CASE("WriteStored then ReadStored reproduces the animation and its stored form") {
    for (const StoredForm form : {StoredForm{}, StoredForm{.strings_scrambled = false,
                                                           .background_colour_swapped = false}}) {
        Animation animation = SampleAnimation();
        animation.stored_form = form;
        const auto stored = WriteStored(animation);
        REQUIRE(stored.has_value());
        CHECK(stored->data[0] == 0xC1);
        CHECK(stored->data[3] == 0x08);
        const bool swapped_colour = stored->data[28] == 0xFF;
        CHECK(swapped_colour == form.background_colour_swapped);
        const auto back = ReadStored(stored->data, stored->script);
        REQUIRE(back.has_value());
        CHECK(*back == animation);
    }
}

TEST_CASE("Stored form swaps every multi-byte field and leaves raw bytes alone") {
    const auto native = Write(SampleAnimation());
    REQUIRE(native.has_value());
    REQUIRE(native->swaps.has_value());
    const auto stored = AfpByteOrder::Store(native->data, *native->swaps, false);
    REQUIRE(stored.has_value());
    CHECK(stored->at(8) == 0x02);
    CHECK(stored->at(9) == 0x00);
    CHECK(stored->at(60) == 0x00);
    const std::size_t table = ReadU32(native->data, 48);
    CHECK(
        std::vector<uint8_t>(stored->begin() + static_cast<std::ptrdiff_t>(table), stored->end()) ==
        std::vector<uint8_t>(native->data.begin() + static_cast<std::ptrdiff_t>(table),
                             native->data.end()));
}

TEST_CASE("Unknown tags and filters survive but have no known byte order") {
    Animation animation = SampleAnimation();
    animation.root.tags.push_back(Tag{UnknownTag{.code = 125, .bytes = {1, 2, 3, 4}}});
    const auto native = Write(animation);
    REQUIRE(native.has_value());
    CHECK_FALSE(native->swaps.has_value());
    const auto back = Read(native->data);
    REQUIRE(back.has_value());
    CHECK(*back == animation);
    CHECK_FALSE(WriteStored(animation).has_value());

    Animation filtered = SampleAnimation();
    Placement with_unknown_filter = FullPlacement();
    std::vector<Filter> filters = with_unknown_filter.filters.value_or(std::vector<Filter>{});
    filters.emplace_back(UnknownFilter{.bytes = {0, 1, 2, 3}});
    with_unknown_filter.filters = std::move(filters);
    filtered.root.tags[3] = Tag{with_unknown_filter};
    const auto filtered_native = Write(filtered);
    REQUIRE(filtered_native.has_value());
    CHECK_FALSE(filtered_native->swaps.has_value());
    const auto filtered_back = Read(filtered_native->data);
    REQUIRE(filtered_back.has_value());
    CHECK(*filtered_back == filtered);
}

TEST_CASE("Write rejects models that cannot be encoded") {
    Animation bad_string = SampleAnimation();
    bad_string.name = 99;
    CHECK_FALSE(Write(bad_string).has_value());

    Animation bad_flags = SampleAnimation();
    std::get<Placement>(bad_flags.root.tags[7].body).flags |= 0x2;
    CHECK_FALSE(Write(bad_flags).has_value());

    Animation bad_curve = SampleAnimation();
    Placement odd_curve = FullPlacement();
    std::vector<Curve> curves = odd_curve.curves.value_or(std::vector<Curve>{});
    REQUIRE_FALSE(curves.empty());
    curves.front().values.push_back(1);
    odd_curve.curves = std::move(curves);
    bad_curve.root.tags[3] = Tag{odd_curve};
    CHECK_FALSE(Write(bad_curve).has_value());

    Animation no_empty_string = SampleAnimation();
    no_empty_string.strings[0] = "first";
    CHECK_FALSE(Write(no_empty_string).has_value());

    Animation no_strings = SampleAnimation();
    no_strings.strings.clear();
    CHECK_FALSE(Write(no_strings).has_value());

    Animation bad_frame = SampleAnimation();
    bad_frame.root.frames[0].tag_count = 0x1000;
    CHECK_FALSE(Write(bad_frame).has_value());
}

TEST_CASE("Read rejects malformed animations") {
    const auto native = Write(SampleAnimation());
    REQUIRE(native.has_value());
    const std::vector<uint8_t> good = native->data;

    std::vector<uint8_t> truncated(good.begin(), good.end() - 4);
    CHECK_FALSE(Read(truncated).has_value());

    std::vector<uint8_t> bad_length = good;
    bad_length[4] ^= 1U;
    CHECK_FALSE(Read(bad_length).has_value());

    std::vector<uint8_t> bad_name = good;
    bad_name[10] = 1;
    CHECK_FALSE(Read(bad_name).has_value());

    std::vector<uint8_t> scrambled = good;
    scrambled[ReadU32(good, 48)] = 0x80;
    CHECK_FALSE(Read(scrambled).has_value());

    std::vector<uint8_t> container_flags = good;
    container_flags[88] = 0x1;
    CHECK_FALSE(Read(container_flags).has_value());

    std::vector<uint8_t> huge_tags = good;
    huge_tags[88 + 8] = 0xFF;
    huge_tags[88 + 11] = 0x7F;
    CHECK_FALSE(Read(huge_tags).has_value());

    std::vector<uint8_t> long_tag = good;
    long_tag[88 + 44] |= 1U;
    CHECK_FALSE(Read(long_tag).has_value());

    CHECK_FALSE(Read(std::vector<uint8_t>(40, 0)).has_value());
}

TEST_CASE("Write requires extended_flags when extended fields are present") {
    Placement placement = MinimalPlacement();
    placement.origin_z = 5;
    CHECK_FALSE(Write(TinyAnimation(Tag{placement})).has_value());
    placement.extended_flags = 0;
    const auto native = Write(TinyAnimation(Tag{placement}));
    REQUIRE(native.has_value());
    const auto back = Read(native->data);
    REQUIRE(back.has_value());
    CHECK(std::get<Placement>(back->root.tags[0].body) == placement);
}

TEST_CASE("Write refuses filters that would read back as something else") {
    const auto with_filter = [](Filter filter) {
        Placement placement = MinimalPlacement();
        placement.filters = std::vector<Filter>{std::move(filter)};
        return Write(TinyAnimation(Tag{placement}));
    };
    CHECK(with_filter(ColourMatrixFilter{.head = {6, 0, 0, 0}, .matrix = {}, .hsv = std::nullopt})
              .has_value());
    CHECK_FALSE(
        with_filter(ColourMatrixFilter{.head = {5, 0, 0, 0}, .matrix = {}, .hsv = std::nullopt})
            .has_value());
    CHECK_FALSE(
        with_filter(LookupFilter{.head = {0, 1, 0, 0, 0, 2}, .unread_bytes = {}, .table = {}})
            .has_value());
    CHECK_FALSE(with_filter(UnknownFilter{.bytes = {}}).has_value());
    std::vector<uint8_t> matrix_shaped(84, 0);
    matrix_shaped[0] = 6;
    CHECK_FALSE(with_filter(UnknownFilter{.bytes = matrix_shaped}).has_value());
}

TEST_CASE("Read refuses empty clip action blocks and filter lists that carry extra bytes") {
    Placement clip = MinimalPlacement();
    clip.clip_actions = ClipActions{.unread_value = 1, .unread_word = 0, .events = {}};
    const auto clip_native = Write(TinyAnimation(Tag{clip}));
    REQUIRE(clip_native.has_value());
    REQUIRE(Read(clip_native->data).has_value());
    const std::size_t clip_block = kTinyTagData + 8;
    std::vector<uint8_t> clip_grown = GrowTag(clip_native->data, clip_block + 12, 4);
    LittleEndian::WriteU32(clip_grown, clip_block + 4, 16);
    CHECK_FALSE(Read(clip_grown).has_value());

    Placement filtered = MinimalPlacement();
    filtered.filters = std::vector<Filter>{};
    const auto filter_native = Write(TinyAnimation(Tag{filtered}));
    REQUIRE(filter_native.has_value());
    REQUIRE(Read(filter_native->data).has_value());
    const std::size_t filter_block = kTinyTagData + 8;
    std::vector<uint8_t> filter_grown = GrowTag(filter_native->data, filter_block + 4, 4);
    LittleEndian::WriteU16(filter_grown, filter_block + 2, 8);
    CHECK_FALSE(Read(filter_grown).has_value());
}

TEST_CASE("Read names a sprite tag table that lies outside its container") {
    const auto native = Write(TinyAnimation(Tag{Sprite{.id = 1, .container = {}}}));
    REQUIRE(native.has_value());
    std::vector<uint8_t> data = native->data;
    LittleEndian::WriteU32(data, kTinyTagData + 8 + 20, 0x10000);
    const auto back = Read(data);
    REQUIRE_FALSE(back.has_value());
    CHECK(back.error().find("tag table") != std::string::npos);
}
