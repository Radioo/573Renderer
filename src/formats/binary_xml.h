#pragma once

#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace BinaryXml {

constexpr uint8_t kSixBitNames = 0x42;
constexpr uint8_t kByteNames = 0x45;
constexpr uint8_t kArrayFlag = 0x40;

namespace Type {
constexpr uint8_t kVoid = 1;
constexpr uint8_t kS8 = 2;
constexpr uint8_t kU8 = 3;
constexpr uint8_t kS16 = 4;
constexpr uint8_t kU16 = 5;
constexpr uint8_t kS32 = 6;
constexpr uint8_t kU32 = 7;
constexpr uint8_t kBin = 10;
constexpr uint8_t kStr = 11;
constexpr uint8_t k2S8 = 16;
constexpr uint8_t k2U8 = 17;
constexpr uint8_t k2S32 = 20;
constexpr uint8_t k3S32 = 30;
constexpr uint8_t k4U16 = 39;
constexpr uint8_t kAttribute = 46;
constexpr uint8_t kArrayMarker = 47;
constexpr uint8_t kBool = 52;
constexpr uint8_t k2B = 53;
}

struct Node {
    uint8_t type = Type::kVoid;
    std::string name;
    std::vector<uint8_t> value;
    std::vector<Node> attributes;
    std::vector<Node> children;
};

struct Document {
    uint8_t signature = kSixBitNames;
    uint8_t encoding = 0;
    Node root;
};

[[nodiscard]] Support::Expected<Document, std::string> Read(std::span<const uint8_t> bytes);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string> Write(const Document& doc);

}
