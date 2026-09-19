#pragma once

#include "document/authored.h"
#include "document/keyframes.h"
#include "support/expected.h"

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace Document {

struct KeyRef {
    std::string property;
    uint32_t frame = 0;

    friend auto operator<=>(const KeyRef&, const KeyRef&) = default;
};

enum class EasySide : uint8_t { Both, In, Out };

struct KeyClip {
    std::vector<Track> tracks;

    friend bool operator==(const KeyClip&, const KeyClip&) = default;
};

[[nodiscard]] std::vector<KeyRef> AllKeys(const AuthoredDepth& authored);

[[nodiscard]] Support::Expected<KeyClip, std::string> CopyKeys(const AuthoredDepth& authored,
                                                               const std::vector<KeyRef>& keys);

[[nodiscard]] Support::Expected<std::vector<KeyRef>, std::string>
PasteKeys(AuthoredDepth& authored, const BakedDepth& baked, const KeyClip& clip, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> RemoveKeys(AuthoredDepth& authored,
                                                              const std::vector<KeyRef>& keys);

[[nodiscard]] Support::Expected<void, std::string>
SetKeysEase(AuthoredDepth& authored, const std::vector<KeyRef>& keys, Ease ease, Bezier bezier);

[[nodiscard]] Support::Expected<std::vector<KeyRef>, std::string>
ShiftKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, int64_t by);

[[nodiscard]] Support::Expected<std::vector<KeyRef>, std::string>
ReverseKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys);

[[nodiscard]] Support::Expected<void, std::string>
EasyEaseKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, EasySide side);

}
