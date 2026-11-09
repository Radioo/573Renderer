#pragma once

#include "formats/binary_xml.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace TextureImages {

struct Image {
    std::string name;
    std::string format;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct List {
    bool compressed = false;
    std::vector<Image> images;
};

enum class Storage : uint8_t { Plain, Lz77, RawAfterHeader };

struct Blob {
    Storage storage = Storage::Plain;
    std::vector<uint8_t> pixels;
};

[[nodiscard]] Support::Expected<List, std::string> ReadList(const BinaryXml::Document& texturelist);

[[nodiscard]] Support::Expected<Blob, std::string> DecodeBlob(std::span<const uint8_t> bytes,
                                                              bool list_compressed);

[[nodiscard]] std::vector<uint8_t> EncodeBlob(const Blob& blob);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
PixelsToBgra(std::string_view format, std::span<const uint8_t> pixels);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
BgraToPixels(std::string_view format, std::span<const uint8_t> bgra);

}
