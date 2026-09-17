#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/animation_settings.h"
#include "document/inspector.h"
#include "document/outline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kFixedRate = 0x2;
constexpr uint32_t kUseColour = 0x1;

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    animation.flags = 0xC3;
    animation.rect = {0, 1920, 0, 1080};
    animation.fps = 60 * 1024;
    animation.background_colour = {0, 0, 0, 255};
    animation.strings = {""};
    animation.root.frames = {AfpAnimation::Frame{}};
    return animation;
}

std::optional<std::string> ValueOf(const std::vector<Document::Field>& fields,
                                   const std::string& name) {
    const auto found = std::ranges::find(fields, name, &Document::Field::name);
    if (found == fields.end()) return std::nullopt;
    return found->value;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("An animation's settings show its stage, rate and background") {
    AfpAnimation::Animation animation = Scene();
    animation.rect = {10, 1930, 20, 1100};
    animation.fps = 30689;
    const std::vector<Document::Field> fields = Document::AnimationSettingFields(animation);
    CHECK(ValueOf(fields, "Stage size") == "1920, 1080");
    CHECK(ValueOf(fields, "Frame rate") == "29.9697");
    CHECK(ValueOf(fields, "Background colour") == "0, 0, 0, 255");
    CHECK(ValueOf(fields, "Use background colour") == "on");

    animation.flags &= ~(kFixedRate | kUseColour);
    animation.fps = std::bit_cast<uint32_t>(24.0F);
    const std::vector<Document::Field> floated = Document::AnimationSettingFields(animation);
    CHECK(ValueOf(floated, "Frame rate") == "24");
    CHECK(ValueOf(floated, "Use background colour") == "off");
}

TEST_CASE("An animation's settings take new values in the form the header stores") {
    AfpAnimation::Animation animation = Scene();
    animation.rect = {10, 1930, 20, 1100};
    const auto sized = Document::SetAnimationSetting(animation, "Stage size", "1280, 720");
    INFO(Error(sized));
    REQUIRE(sized.has_value());
    CHECK(animation.rect == std::array<uint16_t, 4>{10, 1290, 20, 740});

    REQUIRE(Document::SetAnimationSetting(animation, "Frame rate", "29.97").has_value());
    CHECK(animation.fps == 30689);
    REQUIRE(Document::SetAnimationSetting(animation, "Background colour", "255, 128, 0, 200")
                .has_value());
    CHECK(animation.background_colour == std::array<uint8_t, 4>{255, 128, 0, 200});
    REQUIRE(Document::SetAnimationSetting(animation, "Use background colour", "off").has_value());
    CHECK((animation.flags & kUseColour) == 0);
    REQUIRE(Document::SetAnimationSetting(animation, "Use background colour", "on").has_value());
    CHECK((animation.flags & kUseColour) != 0);

    animation.flags &= ~kFixedRate;
    REQUIRE(Document::SetAnimationSetting(animation, "Frame rate", "12.5").has_value());
    CHECK(animation.fps == std::bit_cast<uint32_t>(12.5F));
}

TEST_CASE("A setting that does not fit the header changes nothing") {
    AfpAnimation::Animation animation = Scene();
    animation.rect = {10, 1930, 20, 1100};
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Stage size", "0, 720").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Stage size", "65530, 720").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Stage size", "1280").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Frame rate", "0").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Frame rate", "-5").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Frame rate", "fast").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Frame rate", "3000000").has_value());
    CHECK_FALSE(
        Document::SetAnimationSetting(animation, "Background colour", "256, 0, 0, 0").has_value());
    CHECK_FALSE(
        Document::SetAnimationSetting(animation, "Use background colour", "maybe").has_value());
    CHECK_FALSE(Document::SetAnimationSetting(animation, "Name", "x").has_value());
    CHECK(animation == before);
}

TEST_CASE("The inspector offers the settings when no depth is chosen") {
    const AfpAnimation::Animation animation = Scene();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = std::nullopt,
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt,
                                                              .clip = {}});
    const auto rate = std::ranges::find_if(
        rows, [](const Document::InspectedRow& row) { return row.field.name == "Frame rate"; });
    REQUIRE(rate != rows.end());
    CHECK(rate->edits == Document::EditTarget::Animation);

    const std::vector<Document::InspectedRow> chosen =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt,
                                                              .clip = {}});
    CHECK(std::ranges::none_of(
        chosen, [](const Document::InspectedRow& row) { return row.field.name == "Frame rate"; }));
}
