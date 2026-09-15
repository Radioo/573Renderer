#include "formats/binary_xml.h"

#include "formats/big_endian.h"
#include "formats/binary_xml_names.h"
#include "formats/binary_xml_types.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BinaryXml {

namespace {

using Detail::ValueClass;

void PadTo4(std::vector<uint8_t>& out) {
    out.resize(Detail::PaddedTo4(out.size()), 0);
}

Support::Expected<void, std::string> Fail(const std::string& message) {
    return Support::Unexpected(message);
}

class Writer {
public:
    explicit Writer(const Document& doc) : doc_(&doc) {}

    Support::Expected<std::vector<uint8_t>, std::string> Run();

private:
    Support::Expected<void, std::string> WriteElement(const Node& node);
    Support::Expected<void, std::string> WriteAttributes(const Node& node);
    Support::Expected<void, std::string> WriteValue(const Node& node);

    const Document* doc_;
    std::vector<uint8_t> nodes_;
    std::vector<uint8_t> data_;
    std::size_t byte_slot_ = 0;
    std::size_t byte_count_ = 0;
    std::size_t word_slot_ = 0;
    std::size_t word_count_ = 0;
};

Support::Expected<void, std::string> Writer::WriteValue(const Node& node) {
    const ValueClass value_class = Detail::ClassOf(node.type);
    switch (value_class) {
    case ValueClass::None:
        return node.value.empty() ? Support::Expected<void, std::string>{}
                                  : Fail("value on a node type without data: " + node.name);
    case ValueClass::Byte:
        if (node.value.size() != 1) return Fail("byte value must be 1 byte: " + node.name);
        if (byte_count_ == 0) {
            byte_slot_ = data_.size();
            data_.resize(data_.size() + 4, 0);
        }
        data_[byte_slot_ + byte_count_] = node.value[0];
        byte_count_ = (byte_count_ + 1) & 3U;
        return {};
    case ValueClass::Word:
        if (node.value.size() != 2) return Fail("word value must be 2 bytes: " + node.name);
        if (word_count_ == 0) {
            word_slot_ = data_.size();
            data_.resize(data_.size() + 4, 0);
        }
        data_[word_slot_ + (2 * word_count_)] = node.value[0];
        data_[word_slot_ + (2 * word_count_) + 1] = node.value[1];
        word_count_ ^= 1U;
        return {};
    case ValueClass::Prefixed:
        BigEndian::AppendU32(data_, static_cast<uint32_t>(node.value.size()));
        break;
    case ValueClass::Fixed:
        if (node.value.size() != Detail::FixedSize(node.type)) {
            return Fail("fixed value has the wrong size: " + node.name);
        }
        break;
    }
    data_.insert(data_.end(), node.value.begin(), node.value.end());
    PadTo4(data_);
    return {};
}

Support::Expected<void, std::string> Writer::WriteAttributes(const Node& node) {
    std::vector<const Node*> sorted;
    sorted.reserve(node.attributes.size());
    for (const Node& attribute : node.attributes) {
        if (attribute.type != Type::kAttribute) return Fail("attribute with a non-attribute type");
        sorted.push_back(&attribute);
    }
    std::ranges::stable_sort(sorted, {},
                             [](const Node* a) -> const std::string& { return a->name; });
    for (std::size_t i = 0; i < sorted.size(); i++) {
        if (i > 0 && sorted[i]->name == sorted[i - 1]->name) {
            return Fail("duplicate attribute " + sorted[i]->name);
        }
        nodes_.push_back(Type::kAttribute);
        if (auto name = Detail::EncodeName(sorted[i]->name, doc_->signature, nodes_); !name) {
            return name;
        }
        if (auto value = WriteValue(*sorted[i]); !value) return value;
    }
    return {};
}

Support::Expected<void, std::string> Writer::WriteElement(const Node& node) {
    if (!Detail::IsValidType(node.type) || node.type == Type::kAttribute) {
        return Fail("invalid element type: " + node.name);
    }
    nodes_.push_back(node.type);
    if (auto name = Detail::EncodeName(node.name, doc_->signature, nodes_); !name) return name;
    if (auto value = WriteValue(node); !value) return value;
    if (auto attributes = WriteAttributes(node); !attributes) return attributes;
    for (const Node& child : node.children) {
        if (auto written = WriteElement(child); !written) return written;
    }
    nodes_.push_back(Detail::kNodeEnd);
    return {};
}

Support::Expected<std::vector<uint8_t>, std::string> Writer::Run() {
    if (doc_->signature != kSixBitNames && doc_->signature != kByteNames) {
        return Support::Unexpected(std::string("unsupported binary xml signature"));
    }
    if (auto root = WriteElement(doc_->root); !root) return Support::Unexpected(root.error());
    nodes_.push_back(Detail::kDocumentEnd);
    PadTo4(nodes_);

    std::vector<uint8_t> out = {Detail::kMagic, doc_->signature, doc_->encoding,
                                static_cast<uint8_t>(doc_->encoding ^ Detail::kEncodingComplement)};
    out.reserve(out.size() + (2 * Detail::kLengthSize) + nodes_.size() + data_.size());
    BigEndian::AppendU32(out, static_cast<uint32_t>(nodes_.size()));
    out.insert(out.end(), nodes_.begin(), nodes_.end());
    BigEndian::AppendU32(out, static_cast<uint32_t>(data_.size()));
    out.insert(out.end(), data_.begin(), data_.end());
    return out;
}

}

Support::Expected<std::vector<uint8_t>, std::string> Write(const Document& doc) {
    return Writer(doc).Run();
}

}
