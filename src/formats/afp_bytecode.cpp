#include "formats/afp_animation.h"
#include "formats/afp_animation_detail.h"
#include "formats/afp_layout.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpAnimation::Detail {

Support::Expected<Bytecode, std::string> ReadBytecode(std::span<const uint8_t> bytes,
                                                      const StringTable& strings) {
    ByteReader r(bytes);
    if (r.U8(0) != AfpLayout::kBytecodeMarker || r.Failed())
        return Support::Unexpected(std::string("bytecode does not start with the AP2 marker"));
    const uint8_t flags = r.U8(1);
    Bytecode bytecode{.flags = static_cast<uint8_t>(flags & ~AfpLayout::kBytecodeStrings),
                      .strings = std::nullopt,
                      .code = {}};
    std::size_t pos = AfpLayout::kBytecodeHeaderSize;
    if ((flags & AfpLayout::kBytecodeStrings) != 0) {
        const std::size_t count = r.U16(pos);
        pos += 2;
        if (r.Failed() || !r.Has(pos, count * 2))
            return Support::Unexpected(std::string("bytecode string list is truncated"));
        std::vector<StringId> ids;
        for (std::size_t i = 0; i < count; i++, pos += 2) {
            auto id = strings.Resolve(r.U16(pos));
            if (!id) return Support::Unexpected("bytecode string: " + id.error());
            ids.push_back(*id);
        }
        bytecode.strings = std::move(ids);
    }
    if (r.Failed()) return Support::Unexpected(std::string("bytecode is truncated"));
    const auto code = bytes.subspan(pos);
    bytecode.code.assign(code.begin(), code.end());
    return bytecode;
}

void WriteBytecode(ByteWriter& out, const Bytecode& bytecode) {
    if ((bytecode.flags & AfpLayout::kBytecodeStrings) != 0)
        out.Fail("bytecode flags hold the string list bit; set strings instead");
    out.U8(AfpLayout::kBytecodeMarker);
    out.U8(static_cast<uint8_t>(bytecode.flags |
                                (bytecode.strings ? AfpLayout::kBytecodeStrings : 0U)));
    if (bytecode.strings) {
        if (bytecode.strings->size() > AfpLayout::kMaxWordValue)
            out.Fail("bytecode lists more than 65535 strings");
        out.U16(static_cast<uint16_t>(bytecode.strings->size()));
        for (const StringId id : *bytecode.strings)
            out.StringRef(id);
    }
    out.Raw(bytecode.code);
}

}
