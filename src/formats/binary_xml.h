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
constexpr uint8_t kAttributeType = 46;

struct Node {
    uint8_t type = 1;
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
