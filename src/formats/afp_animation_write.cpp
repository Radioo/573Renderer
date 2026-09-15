#include "formats/afp_animation.h"

#include "formats/afp_animation_detail.h"
#include "formats/afp_layout.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace AfpAnimation {

namespace {

using Detail::ByteWriter;
using namespace AfpLayout;

constexpr std::size_t kMaxImports = std::numeric_limits<int16_t>::max();

void WriteContainer(ByteWriter& out, const Container& container, std::size_t depth);

uint16_t TagCode(const Tag& tag) {
    if (const auto* unknown = std::get_if<UnknownTag>(&tag.body)) return unknown->code;
    if (std::holds_alternative<Sprite>(tag.body)) return kTagSprite;
    if (std::holds_alternative<Action>(tag.body)) return kTagAction;
    if (std::holds_alternative<Placement>(tag.body)) return kTagPlacement;
    if (std::holds_alternative<Remove>(tag.body)) return kTagRemove;
    if (std::holds_alternative<Image>(tag.body)) return kTagImage;
    if (std::holds_alternative<Shape>(tag.body)) return kTagShape;
    return kTagCamera;
}

bool IsModelledCode(uint16_t code) {
    return code == kTagSprite || code == kTagAction || code == kTagPlacement ||
           code == kTagRemove || code == kTagImage || code == kTagShape || code == kTagCamera;
}

void WriteCamera(ByteWriter& out, const Camera& camera) {
    const uint16_t flags =
        (camera.position ? kCameraPosition : 0U) | (camera.focal_length ? kCameraFocalLength : 0U);
    out.U16(flags);
    out.U16(camera.id);
    if (camera.position) {
        for (const int32_t v : *camera.position)
            out.S32(v);
    }
    if (camera.focal_length) out.S32(*camera.focal_length);
}

void WriteTagBody(ByteWriter& out, const Tag& tag, std::size_t depth) {
    if (const auto* sprite = std::get_if<Sprite>(&tag.body)) {
        out.U16(kSpriteFlags);
        out.U16(sprite->id);
        out.U32(kSpriteContainerOffset);
        WriteContainer(out, sprite->container, depth + 1);
    } else if (const auto* action = std::get_if<Action>(&tag.body)) {
        Detail::WriteBytecode(out, action->bytecode);
    } else if (const auto* placement = std::get_if<Placement>(&tag.body)) {
        Detail::WritePlacement(out, *placement);
    } else if (const auto* remove = std::get_if<Remove>(&tag.body)) {
        out.U16(remove->unread_word);
        out.U16(remove->depth);
    } else if (const auto* image = std::get_if<Image>(&tag.body)) {
        out.U32(image->flags);
        out.U16(image->id);
        out.StringRef(image->name);
    } else if (const auto* shape = std::get_if<Shape>(&tag.body)) {
        out.U16(shape->unread_word);
        out.U16(shape->id);
    } else if (const auto* camera = std::get_if<Camera>(&tag.body)) {
        WriteCamera(out, *camera);
    } else if (const auto* unknown = std::get_if<UnknownTag>(&tag.body)) {
        if (IsModelledCode(unknown->code))
            out.Fail(std::format("unknown tag uses the modelled code {}", unknown->code));
        out.OpaqueRaw(unknown->bytes, std::format("tag {}", unknown->code));
    }
}

void WriteTag(ByteWriter& out, const Tag& tag, std::size_t depth) {
    const uint16_t code = TagCode(tag);
    const std::size_t header = out.Position();
    out.U32(0);
    const std::size_t start = out.Position();
    WriteTagBody(out, tag, depth);
    out.Align4();
    const std::size_t size = out.Position() - start;
    if (code > kMaxTagCode || size > kTagSizeMask) {
        out.Fail(std::format("tag {} with {} bytes does not fit a short tag record", code, size));
        return;
    }
    out.PatchU32(header,
                 (static_cast<uint32_t>(code) << kTagCodeShift) | static_cast<uint32_t>(size));
}

void WriteLabels(ByteWriter& out, const std::vector<Label>& labels) {
    for (const Label& label : labels) {
        out.U16(label.frame);
        out.StringRef(label.name);
    }
}

void WriteContainer(ByteWriter& out, const Container& container, std::size_t depth) {
    if (depth > Detail::kMaxNesting) {
        out.Fail(std::format("sprites nest deeper than {}", Detail::kMaxNesting));
        return;
    }
    const std::size_t script_count = container.script_labels ? container.script_labels->size() : 0;
    if (container.labels.size() > kMaxWordValue) {
        out.Fail(std::format("{} labels do not fit a container", container.labels.size()));
        return;
    }
    const std::size_t header = kContainerHeaderSize + (container.script_labels ? kIntSize : 0);
    const std::size_t frame_off = header + ((container.labels.size() + script_count) * kLabelSize);
    const std::size_t tag_off = frame_off + (container.frames.size() * kFrameSize);
    out.U16(container.script_labels ? kContainerScriptLabels : 0U);
    out.U16(static_cast<uint16_t>(container.labels.size()));
    out.U32(static_cast<uint32_t>(container.frames.size()));
    out.U32(static_cast<uint32_t>(container.tags.size()));
    out.U32(static_cast<uint32_t>(header));
    out.U32(static_cast<uint32_t>(frame_off));
    out.U32(static_cast<uint32_t>(tag_off));
    if (container.script_labels) out.U32(static_cast<uint32_t>(script_count));
    WriteLabels(out, container.labels);
    if (container.script_labels) WriteLabels(out, *container.script_labels);
    for (const Frame& frame : container.frames) {
        if (frame.first_tag > kFrameFirstTagMask || frame.tag_count > kMaxFrameTagCount) {
            out.Fail(std::format("frame starting at tag {} with {} tags does not fit a frame entry",
                                 frame.first_tag, frame.tag_count));
        }
        out.U32(frame.first_tag | (frame.tag_count << kFrameCountShift));
    }
    for (const Tag& tag : container.tags) {
        WriteTag(out, tag, depth);
        if (out.Failed()) return;
    }
}

void WriteExportsAndImports(ByteWriter& out, const Animation& animation) {
    out.PatchU32(kExportTableField, static_cast<uint32_t>(out.Position()));
    for (const Export& e : animation.exports) {
        out.U16(e.tag);
        out.StringRef(e.name);
    }
    out.PatchU32(kImportTableField, static_cast<uint32_t>(out.Position()));
    for (const Import& import : animation.imports) {
        if (import.assets.size() > kMaxWordValue)
            out.Fail("an import lists more than 65535 assets");
        out.StringRef(import.movie);
        out.U16(static_cast<uint16_t>(import.assets.size()));
    }
    for (const Import& import : animation.imports) {
        for (const ImportedAsset& asset : import.assets) {
            out.U16(asset.tag);
            out.StringRef(asset.name);
        }
    }
}

void WriteImportInitializers(ByteWriter& out, const ImportInitializers& initializers) {
    out.PatchU32(kHeaderSize, static_cast<uint32_t>(out.Position()));
    if (initializers.entries.size() > kMaxWordValue)
        out.Fail("more than 65535 import initializers");
    out.U16(initializers.leading_word);
    out.U16(static_cast<uint16_t>(initializers.entries.size()));
    for (const ImportInitializer& entry : initializers.entries) {
        out.U16(entry.tag);
        out.U16(entry.frame);
        out.U32(0);
        out.U32(0);
    }
}

void WriteHeader(ByteWriter& out, const Animation& animation, uint32_t flags) {
    out.U32(animation.container_version | (static_cast<uint32_t>(animation.magic[0]) << 8U) |
            (static_cast<uint32_t>(animation.magic[1]) << 16U) |
            (static_cast<uint32_t>(animation.magic[2]) << 24U));
    out.U32(0);
    out.U16(animation.data_version);
    out.StringRef(animation.name);
    out.U32(flags);
    for (const uint16_t v : animation.rect)
        out.U16(v);
    out.U32(animation.fps);
    if (animation.stored_form.background_colour_swapped) {
        const auto& c = animation.background_colour;
        out.U32(c[0] | (static_cast<uint32_t>(c[1]) << 8U) | (static_cast<uint32_t>(c[2]) << 16U) |
                (static_cast<uint32_t>(c[3]) << 24U));
    } else {
        out.Raw(animation.background_colour);
    }
    out.U16(static_cast<uint16_t>(animation.exports.size()));
    out.S16(static_cast<int16_t>(animation.imports.size()));
    for (std::size_t field = kRootContainerField; field < kHeaderSize; field += kIntSize)
        out.U32(0);
    if (animation.import_initializers) out.U32(0);
}

}

Support::Expected<Native, std::string> Write(const Animation& animation) {
    const uint32_t flags = (animation.flags & ~kFlagImportInitializers) |
                           (animation.import_initializers ? kFlagImportInitializers : 0U);
    if ((flags & ~kModelledHeaderFlags) != 0)
        return Support::Unexpected(std::format("header flags {:#x} are not modelled", flags));
    if (animation.strings.empty() || !animation.strings.front().empty()) {
        return Support::Unexpected(
            std::string("the string table must start with the empty string"));
    }
    if (animation.exports.size() > kMaxWordValue || animation.imports.size() > kMaxImports)
        return Support::Unexpected(std::string("too many exports or imports for the header"));

    std::vector<uint32_t> string_offsets;
    const std::vector<uint8_t> table = Detail::BuildStringTable(animation.strings, string_offsets);
    ByteWriter out(std::move(string_offsets));
    WriteHeader(out, animation, flags);
    WriteExportsAndImports(out, animation);
    if (animation.import_initializers) WriteImportInitializers(out, *animation.import_initializers);
    out.PatchU32(kRootContainerField, static_cast<uint32_t>(out.Position()));
    WriteContainer(out, animation.root, 0);
    out.PatchU32(kStringTableField, static_cast<uint32_t>(out.Position()));
    out.PatchU32(kStringTableSizeField, static_cast<uint32_t>(table.size()));
    out.Raw(table);
    if (out.Failed()) return Support::Unexpected(out.Error());
    if (out.Position() > std::numeric_limits<uint32_t>::max())
        return Support::Unexpected(std::string("animation exceeds 4 GB"));
    out.PatchU32(kLengthField, static_cast<uint32_t>(out.Position()));
    return std::move(out).Finish();
}

}
