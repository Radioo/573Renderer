#pragma once

#include "formats/binary_xml.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace TestSupport {

inline std::vector<uint8_t> FromHex(const std::string& hex) {
    std::vector<uint8_t> out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

inline std::vector<uint8_t> TextBytes(const std::string& text) {
    std::vector<uint8_t> out(text.begin(), text.end());
    out.push_back(0);
    return out;
}

inline BinaryXml::Node MakeNode(uint8_t type, std::string name, std::vector<uint8_t> value = {}) {
    BinaryXml::Node node;
    node.type = type;
    node.name = std::move(name);
    node.value = std::move(value);
    return node;
}

inline BinaryXml::Node MakeAttribute(std::string name, const std::string& text) {
    return MakeNode(BinaryXml::Type::kAttribute, std::move(name), TextBytes(text));
}

}
