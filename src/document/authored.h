#pragma once

#include "document/clip.h"
#include "document/keyframes.h"
#include "document/placement_effect.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

struct AuthoredDepth {
    std::string animation;
    uint16_t depth = 0;
    uint32_t first_frame = 0;
    uint32_t last_frame = 0;
    std::vector<Track> tracks;
    std::optional<std::string> script;
    ClipId clip;

    friend bool operator==(const AuthoredDepth&, const AuthoredDepth&) = default;
};

struct FrameControls {
    uint32_t frame = 0;
    uint32_t bits = 0;

    friend bool operator==(const FrameControls&, const FrameControls&) = default;
};

struct ExplicitIdentity {
    uint32_t frame = 0;
    std::string property;

    friend bool operator==(const ExplicitIdentity&, const ExplicitIdentity&) = default;
};

struct BakedDepth {
    AfpAnimation::Placement create;
    uint32_t update_flags = 0;
    std::optional<uint32_t> update_extended_flags;
    std::vector<uint32_t> blank_frames;
    std::vector<FrameControls> extra_controls;
    std::vector<ExplicitIdentity> explicit_identities;

    friend bool operator==(const BakedDepth&, const BakedDepth&) = default;
};

struct OwnedDepth {
    AuthoredDepth authored;
    BakedDepth baked;

    friend bool operator==(const OwnedDepth&, const OwnedDepth&) = default;
};

[[nodiscard]] Support::Expected<OwnedDepth, std::string>
OwnDepth(const AfpAnimation::Animation& animation, ClipId clip, std::string_view animation_path,
         uint16_t depth, uint32_t frame);

[[nodiscard]] Support::Expected<BakedDepth, std::string>
BakedFor(const AfpAnimation::Animation& animation, const AuthoredDepth& authored);

[[nodiscard]] Support::Expected<std::vector<std::pair<uint32_t, AfpAnimation::Placement>>,
                                std::string>
AuthoredPlacements(const AuthoredDepth& authored, const BakedDepth& baked);

[[nodiscard]] AppliedState KeyedState(const AuthoredDepth& authored, const BakedDepth& baked,
                                      uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> WriteAuthored(AfpAnimation::Animation& animation,
                                                                 const AuthoredDepth& authored,
                                                                 const BakedDepth& baked);

}
