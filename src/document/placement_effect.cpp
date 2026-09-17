#include "document/placement_effect.h"

#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr double kLongUnit = 1024.0;
constexpr double kShortUnit = 32768.0;
constexpr double kColourUnit = 255.0;
constexpr std::size_t kScaleX = 0;
constexpr std::size_t kSkewB = 1;
constexpr std::size_t kSkewC = 2;
constexpr std::size_t kScaleY = 3;
constexpr std::size_t kMoveX = 4;
constexpr std::size_t kMoveY = 5;

std::array<double, 4> Colour(const std::array<int16_t, 4>& colour) {
    return {colour[0] / kColourUnit, colour[1] / kColourUnit, colour[2] / kColourUnit,
            colour[3] / kColourUnit};
}

std::array<double, 4> Packed(uint32_t packed) {
    return {static_cast<double>((packed >> 24U) & 0xFFU) / kColourUnit,
            static_cast<double>((packed >> 16U) & 0xFFU) / kColourUnit,
            static_cast<double>((packed >> 8U) & 0xFFU) / kColourUnit,
            static_cast<double>(packed & 0xFFU) / kColourUnit};
}

std::array<double, 6> Matrix(const AfpAnimation::Placement& placement) {
    std::array<double, 6> matrix = AppliedState{}.matrix;
    if (placement.scale) {
        matrix[kScaleX] = (*placement.scale)[0] / kLongUnit;
        matrix[kScaleY] = (*placement.scale)[1] / kLongUnit;
    }
    if (placement.rotate_skew) {
        matrix[kSkewB] = (*placement.rotate_skew)[0] / kLongUnit;
        matrix[kSkewC] = (*placement.rotate_skew)[1] / kLongUnit;
    }
    if (placement.translation) {
        matrix[kMoveX] = (*placement.translation)[0];
        matrix[kMoveY] = (*placement.translation)[1];
    }
    if (placement.short_scale) {
        matrix[kScaleX] = (*placement.short_scale)[0] / kShortUnit;
        matrix[kScaleY] = (*placement.short_scale)[1] / kShortUnit;
    }
    if (placement.short_rotate_skew) {
        matrix[kSkewB] = (*placement.short_rotate_skew)[0] / kShortUnit;
        matrix[kSkewC] = (*placement.short_rotate_skew)[1] / kShortUnit;
    }
    return matrix;
}

std::optional<std::size_t> PlacingTag(const AfpAnimation::Container& clip,
                                      const AfpAnimation::Frame& frame, std::size_t i) {
    const std::size_t index = frame.first_tag + i;
    if (index >= clip.tags.size()) return std::nullopt;
    return index;
}

}

void ApplyPlacement(AppliedState& state, const AfpAnimation::Placement& placement) {
    if ((placement.flags & kUseMatrix) != 0) {
        const std::array<double, 6> carried = Matrix(placement);
        if ((placement.flags & kThreeD) != 0) {
            state.matrix[kMoveX] = carried[kMoveX];
            state.matrix[kMoveY] = carried[kMoveY];
        } else {
            state.matrix = carried;
        }
    }
    if ((placement.flags & kUseColour) != 0) {
        const AppliedState identity;
        state.multiply = identity.multiply;
        state.add = identity.add;
        if (placement.multiply_colour) state.multiply = Colour(*placement.multiply_colour);
        if (placement.add_colour) state.add = Colour(*placement.add_colour);
        if (placement.packed_multiply_colour)
            state.multiply = Packed(*placement.packed_multiply_colour);
        if (placement.packed_add_colour) state.add = Packed(*placement.packed_add_colour);
    }
}

std::vector<std::pair<uint32_t, AppliedState>>
ReplayDepth(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last) {
    std::vector<std::pair<uint32_t, AppliedState>> out;
    std::optional<AppliedState> live;
    for (uint32_t frame = 0; frame <= last && frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (std::size_t i = 0; i < owner.tag_count; i++) {
            const std::optional<std::size_t> index = PlacingTag(clip, owner, i);
            if (!index) break;
            const AfpAnimation::Tag& tag = clip.tags[*index];
            if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) {
                if (remove->depth == depth) live.reset();
                continue;
            }
            const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement == nullptr || placement->depth != depth) continue;
            if ((placement->flags & kUpdateExisting) == 0) {
                live = AppliedState{};
            } else if (!live) {
                continue;
            }
            ApplyPlacement(*live, *placement);
        }
        if (live && frame >= first) out.emplace_back(frame, *live);
    }
    return out;
}

}
