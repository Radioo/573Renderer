#include "formats/binary_xml.h"

#include "formats/big_endian.h"
#include "formats/binary_xml_names.h"
#include "formats/binary_xml_types.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace BinaryXml {

namespace {

constexpr int kMaxDepth = 1024;
constexpr std::size_t kMaxValueBytes = 0x1000000;

using Detail::ValueClass;

Support::Expected<void, std::string> Fail(const std::string& message) {
    return Support::Unexpected(message);
}

class Reader {
public:
    explicit Reader(std::span<const uint8_t> bytes) : bytes_(bytes) {}

    Support::Expected<Document, std::string> Run();

private:
    Support::Expected<void, std::string> ReadHeader();
    Support::Expected<Node, std::string> ParseElement(uint8_t type, int depth);
    Support::Expected<void, std::string> ParseNodes();
    Support::Expected<void, std::string> ReadValues(Node& node);
    Support::Expected<void, std::string> ReadValue(Node& node);
    Support::Expected<std::span<const uint8_t>, std::string> Take(std::size_t count);

    std::span<const uint8_t> bytes_;
    Document doc_;
    std::size_t node_length_ = 0;
    std::size_t pos_ = Detail::kHeaderSize;
    std::size_t data_pos_ = 0;
    std::size_t byte_slot_ = 0;
    std::size_t byte_count_ = 0;
    std::size_t word_slot_ = 0;
    std::size_t word_count_ = 0;
};

Support::Expected<void, std::string> Reader::ReadHeader() {
    if (bytes_.size() < Detail::kHeaderSize) return Fail("document shorter than its header");
    if (bytes_[0] != Detail::kMagic) return Fail("bad binary xml magic");
    doc_.signature = bytes_[1];
    if (doc_.signature != kSixBitNames && doc_.signature != kByteNames) {
        return Fail("unsupported binary xml signature");
    }
    doc_.encoding = bytes_[2];
    if ((bytes_[2] ^ bytes_[3]) != Detail::kEncodingComplement) {
        return Fail("encoding byte and its complement disagree");
    }
    node_length_ = BigEndian::ReadU32(bytes_, 4);
    if (Detail::kHeaderSize + node_length_ + Detail::kLengthSize > bytes_.size()) {
        return Fail("node section overruns the document");
    }
    return {};
}

Support::Expected<Node, std::string> Reader::ParseElement(uint8_t type, int depth) {
    if (depth > kMaxDepth) return Support::Unexpected(std::string("nesting too deep"));
    Node node;
    node.type = type;
    auto name = Detail::DecodeName(bytes_, pos_, doc_.signature);
    if (!name) return Support::Unexpected(name.error());
    node.name = std::move(*name);
    for (;;) {
        if (pos_ >= bytes_.size())
            return Support::Unexpected(std::string("node stream ends early"));
        const uint8_t next = bytes_[pos_++];
        if (next == Detail::kNodeEnd) break;
        if (!Detail::IsValidType(next))
            return Support::Unexpected(std::string("invalid node type"));
        if (next == Type::kAttribute) {
            Node attribute;
            attribute.type = next;
            auto attribute_name = Detail::DecodeName(bytes_, pos_, doc_.signature);
            if (!attribute_name) return Support::Unexpected(attribute_name.error());
            attribute.name = std::move(*attribute_name);
            const bool duplicate = std::ranges::any_of(
                node.attributes, [&](const Node& a) { return a.name == attribute.name; });
            if (duplicate) return Support::Unexpected("duplicate attribute " + attribute.name);
            node.attributes.push_back(std::move(attribute));
            continue;
        }
        auto child = ParseElement(next, depth + 1);
        if (!child) return child;
        node.children.push_back(std::move(*child));
    }
    std::ranges::stable_sort(node.attributes, {}, &Node::name);
    return node;
}

Support::Expected<void, std::string> Reader::ParseNodes() {
    if (pos_ >= bytes_.size()) return Fail("missing root node");
    const uint8_t type = bytes_[pos_++];
    if (!Detail::IsValidType(type) || type == Type::kAttribute) return Fail("invalid root node");
    auto root = ParseElement(type, 0);
    if (!root) return Fail(root.error());
    doc_.root = std::move(*root);
    if (pos_ >= bytes_.size() || bytes_[pos_] != Detail::kDocumentEnd) {
        return Fail("missing document end marker");
    }
    pos_++;
    if (Detail::PaddedTo4(pos_ - Detail::kHeaderSize) != node_length_) {
        return Fail("node section length disagrees with its content");
    }
    return {};
}

Support::Expected<std::span<const uint8_t>, std::string> Reader::Take(std::size_t count) {
    if (data_pos_ + count > bytes_.size()) {
        return Support::Unexpected(std::string("data section ends early"));
    }
    const auto chunk = bytes_.subspan(data_pos_, count);
    data_pos_ += count;
    return chunk;
}

Support::Expected<void, std::string> Reader::ReadValue(Node& node) {
    switch (Detail::ClassOf(node.type)) {
    case ValueClass::None:
        return {};
    case ValueClass::Byte: {
        if (byte_count_ == 0) {
            byte_slot_ = data_pos_;
            if (!Take(4)) return Fail("data section ends early");
        }
        node.value = {bytes_[byte_slot_ + byte_count_]};
        byte_count_ = (byte_count_ + 1) & 3U;
        return {};
    }
    case ValueClass::Word: {
        if (word_count_ == 0) {
            word_slot_ = data_pos_;
            if (!Take(4)) return Fail("data section ends early");
        }
        const std::size_t at = word_slot_ + (2 * word_count_);
        node.value = {bytes_[at], bytes_[at + 1]};
        word_count_ ^= 1U;
        return {};
    }
    case ValueClass::Prefixed: {
        const auto length_bytes = Take(Detail::kLengthSize);
        if (!length_bytes) return Fail(length_bytes.error());
        const std::size_t length = BigEndian::ReadU32(*length_bytes, 0);
        if (length >= kMaxValueBytes) return Fail("value too large");
        const auto raw = Take(Detail::PaddedTo4(length));
        if (!raw) return Fail(raw.error());
        node.value.assign(raw->begin(), raw->begin() + static_cast<std::ptrdiff_t>(length));
        return {};
    }
    case ValueClass::Fixed: {
        const std::size_t size = Detail::FixedSize(node.type);
        const auto raw = Take(Detail::PaddedTo4(size));
        if (!raw) return Fail(raw.error());
        node.value.assign(raw->begin(), raw->begin() + static_cast<std::ptrdiff_t>(size));
        return {};
    }
    }
    return Fail("unknown value class");
}

Support::Expected<void, std::string> Reader::ReadValues(Node& node) {
    if (auto own = ReadValue(node); !own) return own;
    for (Node& attribute : node.attributes) {
        if (auto value = ReadValue(attribute); !value) return value;
    }
    for (Node& child : node.children) {
        if (auto values = ReadValues(child); !values) return values;
    }
    return {};
}

Support::Expected<Document, std::string> Reader::Run() {
    if (auto header = ReadHeader(); !header) return Support::Unexpected(header.error());
    if (auto nodes = ParseNodes(); !nodes) return Support::Unexpected(nodes.error());
    const std::size_t length_at = Detail::kHeaderSize + node_length_;
    const std::size_t data_length = BigEndian::ReadU32(bytes_, length_at);
    const std::size_t data_start = length_at + Detail::kLengthSize;
    data_pos_ = data_start;
    if (auto values = ReadValues(doc_.root); !values) return Support::Unexpected(values.error());
    if (data_pos_ - data_start != data_length) {
        return Support::Unexpected(std::string("data section length disagrees with its content"));
    }
    return std::move(doc_);
}

}

Support::Expected<Document, std::string> Read(std::span<const uint8_t> bytes) {
    return Reader(bytes).Run();
}

}
