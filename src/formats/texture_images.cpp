#include "formats/texture_images.h"

#include "formats/avs_lz77.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace TextureImages {

namespace {

constexpr std::string_view kCompressAttribute = "compress";
constexpr std::string_view kAvsLz = "avslz";
constexpr std::string_view kArgb8888Rev = "argb8888rev";
constexpr std::size_t kBlobHeaderSize = 8;
constexpr std::size_t kRectBytes = 8;

std::string AttributeText(const BinaryXml::Node& node, std::string_view name) {
    for (const BinaryXml::Node& attribute : node.attributes) {
        if (attribute.name != name) continue;
        std::string text(attribute.value.begin(), attribute.value.end());
        if (!text.empty() && text.back() == '\0') text.pop_back();
        return text;
    }
    return {};
}

const BinaryXml::Node* Child(const BinaryXml::Node& node, std::string_view name) {
    const auto found = std::ranges::find_if(
        node.children, [&](const BinaryXml::Node& c) { return c.name == name; });
    return found == node.children.end() ? nullptr : &*found;
}

Support::Expected<Image, std::string> ReadImage(const BinaryXml::Node& node,
                                                const std::string& format) {
    Image image;
    image.name = AttributeText(node, "name");
    image.format = format;
    const BinaryXml::Node* rect = Child(node, "imgrect");
    if (rect == nullptr || rect->type != BinaryXml::Type::k4U16 ||
        rect->value.size() != kRectBytes) {
        return Support::Unexpected("image " + image.name + " has no usable imgrect");
    }
    const uint16_t x0 = BigEndian::ReadU16(rect->value, 0);
    const uint16_t x1 = BigEndian::ReadU16(rect->value, 2);
    const uint16_t y0 = BigEndian::ReadU16(rect->value, 4);
    const uint16_t y1 = BigEndian::ReadU16(rect->value, 6);
    if (x1 < x0 || y1 < y0)
        return Support::Unexpected("image " + image.name + " has an inverted imgrect");
    image.width = (x1 - x0) / 2U;
    image.height = (y1 - y0) / 2U;
    return image;
}

}

Support::Expected<List, std::string> ReadList(const BinaryXml::Document& texturelist) {
    List list;
    list.compressed = AttributeText(texturelist.root, kCompressAttribute) == kAvsLz;
    for (const BinaryXml::Node& texture : texturelist.root.children) {
        if (texture.name != "texture") continue;
        const std::string format = AttributeText(texture, "format");
        for (const BinaryXml::Node& node : texture.children) {
            if (node.name != "image") continue;
            auto image = ReadImage(node, format);
            if (!image) return Support::Unexpected(image.error());
            list.images.push_back(std::move(*image));
        }
    }
    return list;
}

Support::Expected<Blob, std::string> DecodeBlob(std::span<const uint8_t> bytes,
                                                bool list_compressed) {
    Blob blob;
    if (!list_compressed) {
        blob.pixels.assign(bytes.begin(), bytes.end());
        return blob;
    }
    if (bytes.size() < kBlobHeaderSize)
        return Support::Unexpected(std::string("image blob shorter than its header"));
    const std::size_t uncompressed = BigEndian::ReadU32(bytes, 0);
    const std::size_t compressed = BigEndian::ReadU32(bytes, 4);
    const std::span<const uint8_t> body = bytes.subspan(kBlobHeaderSize);
    if (compressed == 0) {
        if (body.size() != uncompressed)
            return Support::Unexpected(std::string("raw image size disagrees with its header"));
        blob.storage = Storage::RawAfterHeader;
        blob.pixels.assign(body.begin(), body.end());
        return blob;
    }
    if (body.size() != compressed)
        return Support::Unexpected(std::string("compressed image size disagrees with its header"));
    blob.storage = Storage::Lz77;
    blob.pixels = AvsLz77::Decompress(body, uncompressed);
    if (blob.pixels.size() != uncompressed) {
        return Support::Unexpected(
            std::string("image decompressed to a different size than its header"));
    }
    return blob;
}

std::vector<uint8_t> EncodeBlob(const Blob& blob) {
    if (blob.storage == Storage::Plain) return blob.pixels;
    std::vector<uint8_t> out;
    BigEndian::AppendU32(out, static_cast<uint32_t>(blob.pixels.size()));
    if (blob.storage == Storage::RawAfterHeader) {
        BigEndian::AppendU32(out, 0);
        out.insert(out.end(), blob.pixels.begin(), blob.pixels.end());
        return out;
    }
    const std::vector<uint8_t> packed = AvsLz77::Compress(blob.pixels);
    BigEndian::AppendU32(out, static_cast<uint32_t>(packed.size()));
    out.insert(out.end(), packed.begin(), packed.end());
    return out;
}

Support::Expected<std::vector<uint8_t>, std::string> PixelsToBgra(std::string_view format,
                                                                  std::span<const uint8_t> pixels) {
    if (format != kArgb8888Rev)
        return Support::Unexpected("unsupported pixel format " + std::string(format));
    return std::vector<uint8_t>(pixels.begin(), pixels.end());
}

Support::Expected<std::vector<uint8_t>, std::string> BgraToPixels(std::string_view format,
                                                                  std::span<const uint8_t> bgra) {
    if (format != kArgb8888Rev)
        return Support::Unexpected("unsupported pixel format " + std::string(format));
    return std::vector<uint8_t>(bgra.begin(), bgra.end());
}

}
