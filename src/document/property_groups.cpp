#include "document/property_groups.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr std::string_view kTranslation = "Translation";

struct GroupMember {
    std::string_view property;
    PropertyGroup group;
    std::array<int64_t, 4> identity;
    std::size_t width;
};

constexpr std::array<GroupMember, 9> kMembers{{
    {.property = "Scale", .group = PropertyGroup::Matrix, .identity = {1024, 1024}, .width = 2},
    {.property = "Rotate skew", .group = PropertyGroup::Matrix, .identity = {0, 0}, .width = 2},
    {.property = kTranslation, .group = PropertyGroup::Matrix, .identity = {0, 0}, .width = 2},
    {.property = "Short scale",
     .group = PropertyGroup::Matrix,
     .identity = {32768, 32768},
     .width = 2},
    {.property = "Short rotate skew",
     .group = PropertyGroup::Matrix,
     .identity = {0, 0},
     .width = 2},
    {.property = "Multiply colour",
     .group = PropertyGroup::Colour,
     .identity = {255, 255, 255, 255},
     .width = 4},
    {.property = "Add colour",
     .group = PropertyGroup::Colour,
     .identity = {0, 0, 0, 0},
     .width = 4},
    {.property = "Packed multiply colour",
     .group = PropertyGroup::Colour,
     .identity = {0xFFFFFFFF},
     .width = 1},
    {.property = "Packed add colour", .group = PropertyGroup::Colour, .identity = {0}, .width = 1},
}};

const GroupMember* MemberFor(std::string_view property) {
    const auto found = std::ranges::find(kMembers, property, &GroupMember::property);
    return found == kMembers.end() ? nullptr : &*found;
}

}

PropertyGroup GroupOf(std::string_view property, bool three_d) {
    const GroupMember* member = MemberFor(property);
    if (member == nullptr) return PropertyGroup::None;
    if (three_d && member->group == PropertyGroup::Matrix && property != kTranslation)
        return PropertyGroup::None;
    return member->group;
}

uint32_t GroupBit(PropertyGroup group) {
    switch (group) {
    case PropertyGroup::Matrix:
        return kUseMatrix;
    case PropertyGroup::Colour:
        return kUseColour;
    case PropertyGroup::None:
        return 0;
    }
    return 0;
}

std::optional<std::vector<int64_t>> IdentityOf(std::string_view property) {
    const GroupMember* member = MemberFor(property);
    if (member == nullptr) return std::nullopt;
    return std::vector<int64_t>(member->identity.begin(),
                                member->identity.begin() +
                                    static_cast<std::ptrdiff_t>(member->width));
}

}
