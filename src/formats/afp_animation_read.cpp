#include "formats/afp_animation.h"

#include "formats/afp_animation_detail.h"
#include "formats/afp_layout.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpAnimation {

namespace {

using Detail::ByteReader;
using Detail::CheckTail;
using Detail::StringTable;
using namespace AfpLayout;

Support::Expected<std::vector<Label>, std::string>
ReadLabels(ByteReader& r, std::size_t off, std::size_t count, const StringTable& strings) {
    std::vector<Label> labels;
    labels.reserve(count);
    for (std::size_t i = 0; i < count; i++) {
        const std::size_t entry = off + (i * kLabelSize);
        const uint16_t frame = r.U16(entry);
        auto name = strings.Resolve(r.U16(entry + 2));
        if (!name) return Support::Unexpected("label: " + name.error());
        labels.push_back(Label{.frame = frame, .name = *name});
    }
    return labels;
}

Support::Expected<Container, std::string> ReadContainer(std::span<const uint8_t> bytes,
                                                        const StringTable& strings,
                                                        std::size_t depth, std::size_t& end);

Support::Expected<Tag, std::string> ReadSprite(std::span<const uint8_t> record,
                                               const StringTable& strings, std::size_t depth) {
    ByteReader r(record);
    const uint16_t flags = r.U16(0);
    const uint16_t id = r.U16(2);
    const uint32_t offset = r.U32(4);
    if (r.Failed()) return Support::Unexpected(std::string("sprite is truncated"));
    if (flags != kSpriteFlags || offset != kSpriteContainerOffset) {
        return Support::Unexpected(std::format(
            "sprite form with flags {:#x} and offset {} is not modelled", flags, offset));
    }
    std::size_t end = 0;
    auto container = ReadContainer(record.subspan(kSpriteContainerOffset), strings, depth + 1, end);
    if (!container) return Support::Unexpected(container.error());
    auto tail = CheckTail(record, kSpriteContainerOffset + end);
    if (!tail) return Support::Unexpected(tail.error());
    return Tag{Sprite{.id = id, .container = std::move(*container)}};
}

Support::Expected<Tag, std::string> ReadAction(std::span<const uint8_t> record,
                                               const StringTable& strings) {
    auto bytecode = Detail::ReadBytecode(record, strings);
    if (!bytecode) return Support::Unexpected(bytecode.error());
    return Tag{Action{.bytecode = std::move(*bytecode)}};
}

Support::Expected<Tag, std::string> ReadPlacementTag(std::span<const uint8_t> record,
                                                     const StringTable& strings) {
    auto placement = Detail::ReadPlacement(record, strings);
    if (!placement) return Support::Unexpected(placement.error());
    return Tag{std::move(*placement)};
}

Support::Expected<Tag, std::string> ReadWordPair(std::span<const uint8_t> record, uint16_t code) {
    ByteReader r(record);
    const uint16_t first = r.U16(0);
    const uint16_t second = r.U16(2);
    if (r.Failed()) return Support::Unexpected(std::string("tag is truncated"));
    auto tail = CheckTail(record, kWordPairSize);
    if (!tail) return Support::Unexpected(tail.error());
    if (code == kTagRemove) return Tag{Remove{.unread_word = first, .depth = second}};
    return Tag{Shape{.unread_word = first, .id = second}};
}

Support::Expected<Tag, std::string> ReadImage(std::span<const uint8_t> record,
                                              const StringTable& strings) {
    ByteReader r(record);
    const uint32_t flags = r.U32(0);
    const uint16_t id = r.U16(4);
    const uint16_t reference = r.U16(6);
    if (r.Failed()) return Support::Unexpected(std::string("image is truncated"));
    auto name = strings.Resolve(reference);
    if (!name) return Support::Unexpected("image name: " + name.error());
    auto tail = CheckTail(record, kImageSize);
    if (!tail) return Support::Unexpected(tail.error());
    return Tag{Image{.flags = flags, .id = id, .name = *name}};
}

Support::Expected<Tag, std::string> ReadCamera(std::span<const uint8_t> record) {
    ByteReader r(record);
    const uint16_t flags = r.U16(0);
    Camera camera{.id = r.U16(2), .position = std::nullopt, .focal_length = std::nullopt};
    if ((flags & ~(kCameraPosition | kCameraFocalLength)) != 0)
        return Support::Unexpected(std::format("camera flags {:#x} are not modelled", flags));
    std::size_t pos = kWordPairSize;
    if ((flags & kCameraPosition) != 0) {
        camera.position = {r.S32(pos), r.S32(pos + 4), r.S32(pos + 8)};
        pos += 3 * kIntSize;
    }
    if ((flags & kCameraFocalLength) != 0) {
        camera.focal_length = r.S32(pos);
        pos += kIntSize;
    }
    if (r.Failed()) return Support::Unexpected(std::string("camera is truncated"));
    auto tail = CheckTail(record, pos);
    if (!tail) return Support::Unexpected(tail.error());
    return Tag{camera};
}

Support::Expected<Tag, std::string> ReadTag(uint16_t code, std::span<const uint8_t> record,
                                            const StringTable& strings, std::size_t depth) {
    switch (code) {
    case kTagSprite:
        return ReadSprite(record, strings, depth);
    case kTagAction:
        return ReadAction(record, strings);
    case kTagPlacement:
        return ReadPlacementTag(record, strings);
    case kTagRemove:
    case kTagShape:
        return ReadWordPair(record, code);
    case kTagImage:
        return ReadImage(record, strings);
    case kTagCamera:
        return ReadCamera(record);
    default:
        return Tag{UnknownTag{.code = code, .bytes = {record.begin(), record.end()}}};
    }
}

Support::Expected<std::vector<Tag>, std::string> ReadTags(std::span<const uint8_t> bytes,
                                                          std::size_t off, std::size_t count,
                                                          const StringTable& strings,
                                                          std::size_t depth, std::size_t& end) {
    ByteReader r(bytes);
    if (off > bytes.size())
        return Support::Unexpected(std::string("tag table lies outside the container"));
    if (count > bytes.size() / kTagHeaderSize)
        return Support::Unexpected(std::format("tag count {} exceeds the container", count));
    std::vector<Tag> tags;
    tags.reserve(count);
    std::size_t pos = off;
    for (std::size_t i = 0; i < count; i++) {
        const uint32_t header = r.U32(pos);
        const auto code = static_cast<uint16_t>(header >> kTagCodeShift);
        const std::size_t size = header & kTagSizeMask;
        if (r.Failed()) return Support::Unexpected(std::format("tag {} header is truncated", i));
        if ((size & kLongTagFlag) != 0) {
            return Support::Unexpected(std::format(
                "tag {} ({}) uses the long record form, which is not modelled", i, code));
        }
        if (size % kAlignment != 0 || !r.Has(pos + kTagHeaderSize, size)) {
            return Support::Unexpected(
                std::format("tag {} ({}) has a bad size of {} bytes", i, code, size));
        }
        auto tag = ReadTag(code, bytes.subspan(pos + kTagHeaderSize, size), strings, depth);
        if (!tag) return Support::Unexpected(std::format("tag {} ({}): {}", i, code, tag.error()));
        tags.push_back(std::move(*tag));
        pos += kTagHeaderSize + size;
    }
    end = pos;
    return tags;
}

Support::Expected<Container, std::string> ReadContainer(std::span<const uint8_t> bytes,
                                                        const StringTable& strings,
                                                        std::size_t depth, std::size_t& end) {
    if (depth > Detail::kMaxNesting)
        return Support::Unexpected(std::format("sprites nest deeper than {}", Detail::kMaxNesting));
    ByteReader r(bytes);
    const uint16_t flags = r.U16(0);
    const uint16_t label_count = r.U16(kContainerLabelCountField);
    const std::size_t frame_count = r.U32(kContainerFrameCountField);
    const std::size_t tag_count = r.U32(kContainerTagCountField);
    const std::size_t label_off = r.U32(kContainerLabelTableField);
    const std::size_t frame_off = r.U32(kContainerFrameTableField);
    const std::size_t tag_off = r.U32(kContainerTagTableField);
    const bool has_script_labels = (flags & kContainerScriptLabels) != 0;
    const std::size_t script_count = has_script_labels ? r.U32(kContainerHeaderSize) : 0;
    if (r.Failed()) return Support::Unexpected(std::string("container header is truncated"));
    if ((flags & ~kContainerScriptLabels) != 0)
        return Support::Unexpected(std::format("container flags {:#x} are not modelled", flags));

    const std::size_t label_total = label_count + script_count;
    if (script_count > bytes.size() / kLabelSize || !r.Has(label_off, label_total * kLabelSize))
        return Support::Unexpected(std::string("label table lies outside the container"));
    if (frame_count > bytes.size() / kFrameSize || !r.Has(frame_off, frame_count * kFrameSize))
        return Support::Unexpected(std::string("frame table lies outside the container"));

    Container container;
    auto labels = ReadLabels(r, label_off, label_count, strings);
    if (!labels) return Support::Unexpected(labels.error());
    container.labels = std::move(*labels);
    if (has_script_labels) {
        auto script = ReadLabels(r, label_off + (label_count * kLabelSize), script_count, strings);
        if (!script) return Support::Unexpected(script.error());
        container.script_labels = std::move(*script);
    }
    container.frames.reserve(frame_count);
    for (std::size_t i = 0; i < frame_count; i++) {
        const uint32_t value = r.U32(frame_off + (i * kFrameSize));
        container.frames.push_back(
            Frame{.first_tag = value & kFrameFirstTagMask, .tag_count = value >> kFrameCountShift});
    }
    auto tags = ReadTags(bytes, tag_off, tag_count, strings, depth, end);
    if (!tags) return Support::Unexpected(tags.error());
    container.tags = std::move(*tags);
    return container;
}

Support::Expected<void, std::string> ReadExportsAndImports(ByteReader& r, Animation& animation,
                                                           const StringTable& strings) {
    const std::size_t export_count = r.U16(kExportCountField);
    const std::size_t export_off = r.U32(kExportTableField);
    for (std::size_t i = 0; i < export_count; i++) {
        const std::size_t entry = export_off + (i * kWordPairSize);
        const uint16_t tag = r.U16(entry);
        auto name = strings.Resolve(r.U16(entry + 2));
        if (r.Failed()) return Support::Unexpected(std::string("export table is truncated"));
        if (!name) return Support::Unexpected("export: " + name.error());
        animation.exports.push_back(Export{.tag = tag, .name = *name});
    }
    const int16_t import_count = r.S16(kImportCountField);
    if (import_count < 0)
        return Support::Unexpected(std::format("import count {} is negative", import_count));
    const std::size_t import_off = r.U32(kImportTableField);
    const auto imports = static_cast<std::size_t>(import_count);
    std::size_t entry = import_off + (imports * kWordPairSize);
    for (std::size_t i = 0; i < imports; i++) {
        const std::size_t header = import_off + (i * kWordPairSize);
        auto movie = strings.Resolve(r.U16(header));
        const std::size_t asset_count = r.U16(header + 2);
        if (r.Failed() || !r.Has(entry, asset_count * kWordPairSize))
            return Support::Unexpected(std::string("import table is truncated"));
        if (!movie) return Support::Unexpected("import: " + movie.error());
        Import import{.movie = *movie, .assets = {}};
        for (std::size_t j = 0; j < asset_count; j++, entry += kWordPairSize) {
            const uint16_t tag = r.U16(entry);
            auto name = strings.Resolve(r.U16(entry + 2));
            if (!name) return Support::Unexpected("imported asset: " + name.error());
            import.assets.push_back(ImportedAsset{.tag = tag, .name = *name});
        }
        animation.imports.push_back(std::move(import));
    }
    return {};
}

Support::Expected<ImportInitializers, std::string> ReadImportInitializers(ByteReader& r) {
    const std::size_t off = r.U32(kHeaderSize);
    ImportInitializers initializers{.leading_word = r.U16(off), .entries = {}};
    const std::size_t count = r.U16(off + 2);
    if (r.Failed() || !r.Has(off + kWordPairSize, count * kInitializerEntrySize))
        return Support::Unexpected(std::string("import initializer section is truncated"));
    for (std::size_t i = 0; i < count; i++) {
        const std::size_t entry = off + kWordPairSize + (i * kInitializerEntrySize);
        if (r.U32(entry + 4) != 0 || r.U32(entry + 8) != 0)
            return Support::Unexpected(std::string("import initializer bytecode is not modelled"));
        initializers.entries.push_back(
            ImportInitializer{.tag = r.U16(entry), .frame = r.U16(entry + 2)});
    }
    return initializers;
}

}

Support::Expected<Animation, std::string> Read(std::span<const uint8_t> native) {
    ByteReader r(native);
    if (!r.Has(0, kHeaderSize))
        return Support::Unexpected(std::string("animation header is truncated"));
    if (((r.U32(0) ^ kNativeMagicXor) & kMagicMask) != 0)
        return Support::Unexpected(std::string("data is not a native byte order animation"));
    if (r.U32(kLengthField) != native.size()) {
        return Support::Unexpected(std::format("length field {} does not match the data size {}",
                                               r.U32(kLengthField), native.size()));
    }
    const uint32_t flags = r.U32(kFlagsField);
    if ((flags & ~kModelledHeaderFlags) != 0)
        return Support::Unexpected(std::format("header flags {:#x} are not modelled", flags));

    Animation animation;
    animation.container_version = native[0];
    animation.magic = {native[1], native[2], native[3]};
    animation.data_version = r.U16(kDataVersionField);
    animation.flags = flags & ~kFlagImportInitializers;
    animation.rect = {r.U16(kRectField), r.U16(kRectField + 2), r.U16(kRectField + 4),
                      r.U16(kRectField + 6)};
    animation.fps = r.U32(kFpsField);
    std::copy_n(native.begin() + kBackgroundColourField, animation.background_colour.size(),
                animation.background_colour.begin());

    const std::size_t table_off = r.U32(kStringTableField);
    const std::size_t table_size = r.U32(kStringTableSizeField);
    if (!r.Has(table_off, table_size))
        return Support::Unexpected(std::string("string table lies outside the animation"));
    auto strings = StringTable::Read(native.subspan(table_off, table_size), animation.strings);
    if (!strings) return Support::Unexpected(strings.error());
    auto name = strings->Resolve(r.U16(kNameField));
    if (!name) return Support::Unexpected("movie name: " + name.error());
    animation.name = *name;

    auto tables = ReadExportsAndImports(r, animation, *strings);
    if (!tables) return Support::Unexpected(tables.error());
    if ((flags & kFlagImportInitializers) != 0) {
        auto initializers = ReadImportInitializers(r);
        if (!initializers) return Support::Unexpected(initializers.error());
        animation.import_initializers = std::move(*initializers);
    }

    const std::size_t root_off = r.U32(kRootContainerField);
    if (root_off > native.size())
        return Support::Unexpected(std::string("root container lies outside the animation"));
    std::size_t end = 0;
    auto root = ReadContainer(native.subspan(root_off), *strings, 0, end);
    if (!root) return Support::Unexpected(root.error());
    animation.root = std::move(*root);
    return animation;
}

}
