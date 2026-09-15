#include "formats/binary_xml_names.h"

#include "formats/binary_xml.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace BinaryXml::Detail {

namespace {

constexpr std::string_view kSixBitAlphabet =
    "0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
constexpr std::size_t kMaxSixBitLength = 36;
constexpr std::size_t kMaxOneByteLengthName = 64;
constexpr std::size_t kMaxLongName = 4096;
constexpr std::size_t kShortLengthBias = 63;
constexpr uint8_t kShortLengthMin = 0x40;
constexpr uint8_t kWideLengthFlag = 0x80;
constexpr std::size_t kWideLengthBias = 0x7FBF;
constexpr uint32_t kSixBitMask = 0x3F;

std::size_t SixBitBytes(std::size_t length) {
    return ((6 * length) + 7) / 8;
}

Support::Expected<std::string, std::string> DecodeSixBit(std::span<const uint8_t> bytes,
                                                         std::size_t& pos) {
    const std::size_t length = bytes[pos];
    if (length == 0) return Support::Unexpected(std::string("empty sixbit name"));
    if (length > kMaxSixBitLength) return Support::Unexpected(std::string("sixbit name too long"));
    const std::size_t packed = SixBitBytes(length);
    if (pos + 1 + packed > bytes.size()) return Support::Unexpected(std::string("name overruns"));
    std::string name;
    uint32_t acc = 0;
    int bits = 0;
    std::size_t next = pos + 1;
    for (std::size_t i = 0; i < length; i++) {
        while (bits < 6) {
            acc = (acc << 8U) | bytes[next++];
            bits += 8;
        }
        bits -= 6;
        name.push_back(kSixBitAlphabet[(acc >> static_cast<uint32_t>(bits)) & kSixBitMask]);
    }
    pos += 1 + packed;
    return name;
}

Support::Expected<std::string, std::string> DecodeLong(std::span<const uint8_t> bytes,
                                                       std::size_t& pos) {
    const uint8_t lead = bytes[pos];
    std::size_t length = 0;
    std::size_t header = 1;
    if ((lead & kWideLengthFlag) != 0) {
        if (pos + 2 > bytes.size()) return Support::Unexpected(std::string("name overruns"));
        length = ((static_cast<std::size_t>(lead) << 8U) | bytes[pos + 1]) - kWideLengthBias;
        header = 2;
        if (length > kMaxLongName) return Support::Unexpected(std::string("long name too long"));
    } else {
        if (lead < kShortLengthMin) return Support::Unexpected(std::string("bad name length"));
        length = lead - kShortLengthBias;
    }
    if (pos + header + length > bytes.size()) {
        return Support::Unexpected(std::string("name overruns"));
    }
    const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(pos + header);
    std::string name(first, first + static_cast<std::ptrdiff_t>(length));
    pos += header + length;
    return name;
}

Support::Expected<void, std::string> EncodeSixBit(const std::string& name,
                                                  std::vector<uint8_t>& out) {
    if (name.empty() || name.size() > kMaxSixBitLength) {
        return Support::Unexpected("sixbit name length out of range: " + name);
    }
    out.push_back(static_cast<uint8_t>(name.size()));
    uint32_t acc = 0;
    int bits = 0;
    for (const char c : name) {
        const std::size_t index = kSixBitAlphabet.find(c);
        if (index == std::string_view::npos) {
            return Support::Unexpected("character not allowed in a sixbit name: " + name);
        }
        acc = (acc << 6U) | static_cast<uint32_t>(index);
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((acc >> static_cast<uint32_t>(bits)) & 0xFFU));
        }
    }
    if (bits > 0) {
        out.push_back(static_cast<uint8_t>((acc << static_cast<uint32_t>(8 - bits)) & 0xFFU));
    }
    return {};
}

Support::Expected<void, std::string> EncodeLong(const std::string& name,
                                                std::vector<uint8_t>& out) {
    if (name.empty() || name.size() > kMaxLongName) {
        return Support::Unexpected("long name length out of range: " + name);
    }
    if (name.size() <= kMaxOneByteLengthName) {
        out.push_back(static_cast<uint8_t>(name.size() + kShortLengthBias));
    } else {
        const std::size_t wide = name.size() + kWideLengthBias;
        out.push_back(static_cast<uint8_t>((wide >> 8U) & 0xFFU));
        out.push_back(static_cast<uint8_t>(wide & 0xFFU));
    }
    out.insert(out.end(), name.begin(), name.end());
    return {};
}

}

Support::Expected<std::string, std::string> DecodeName(std::span<const uint8_t> bytes,
                                                       std::size_t& pos, uint8_t signature) {
    if (pos >= bytes.size()) return Support::Unexpected(std::string("name overruns"));
    return signature == kByteNames ? DecodeLong(bytes, pos) : DecodeSixBit(bytes, pos);
}

Support::Expected<void, std::string> EncodeName(const std::string& name, uint8_t signature,
                                                std::vector<uint8_t>& out) {
    return signature == kByteNames ? EncodeLong(name, out) : EncodeSixBit(name, out);
}

}
