#include "formats/afp_script.h"

#include "formats/big_endian.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpScript {

namespace {

std::optional<std::size_t> PushOperandSize(uint8_t type) {
    switch (type) {
    case 0:
    case 2:
    case 3:
    case 5:
    case 6:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 34:
    case 35:
        return 0;
    case 4:
    case 8:
    case 16:
    case 19:
    case 22:
    case 25:
    case 28:
    case 31:
    case 36:
    case 39:
    case 42:
    case 45:
    case 48:
    case 52:
    case 55:
    case 56:
        return 1;
    case 9:
    case 17:
        return 2;
    case 18:
        return 3;
    case 1:
    case 7:
        return 4;
    case 51:
        return 8;
    default:
        return std::nullopt;
    }
}

Support::Expected<std::size_t, std::string> ReadPush(std::span<const uint8_t> code, std::size_t pos,
                                                     Instruction& out) {
    if (pos >= code.size()) return Support::Unexpected(std::string("push has no item count"));
    const uint8_t count = code[pos];
    pos++;
    for (uint8_t i = 0; i < count; i++) {
        if (pos >= code.size()) return Support::Unexpected(std::string("push item is truncated"));
        const uint8_t type = code[pos];
        pos++;
        const std::optional<std::size_t> operand = PushOperandSize(type);
        if (!operand) return Support::Unexpected(std::format("unknown push type {}", type));
        const std::size_t size = *operand;
        if (code.size() - pos < size)
            return Support::Unexpected(std::format("push type {} is truncated", type));
        Item item{.type = type, .operand = {}};
        item.operand.assign(code.begin() + static_cast<std::ptrdiff_t>(pos),
                            code.begin() + static_cast<std::ptrdiff_t>(pos + size));
        out.items.push_back(std::move(item));
        pos += size;
    }
    return pos;
}

Support::Expected<std::size_t, std::string> ReadOperands(std::span<const uint8_t> code,
                                                         std::size_t pos, Instruction& out) {
    if (out.opcode == Op::kPush) return ReadPush(code, pos, out);
    if (out.opcode == Op::kStoreRegister) {
        if (pos >= code.size())
            return Support::Unexpected(std::string("store register has no count"));
        const uint8_t count = code[pos];
        pos++;
        if (code.size() - pos < count)
            return Support::Unexpected(std::string("store register list is truncated"));
        out.registers.assign(code.begin() + static_cast<std::ptrdiff_t>(pos),
                             code.begin() + static_cast<std::ptrdiff_t>(pos + count));
        return pos + count;
    }
    if (out.opcode == Op::kGotoFrame2) {
        if (pos >= code.size())
            return Support::Unexpected(std::string("goto frame 2 has no flags"));
        out.flags = code[pos];
        pos++;
        if ((out.flags & 0x2) == 0) return pos;
        if (code.size() - pos < 2)
            return Support::Unexpected(std::string("goto frame 2 bias is truncated"));
        out.frame_bias = BigEndian::ReadU16(code, pos);
        out.has_frame_bias = true;
        return pos + 2;
    }
    if (out.opcode != Op::kPop && out.opcode != Op::kGetVariable &&
        out.opcode != Op::kCallFunction && out.opcode != Op::kSetMember &&
        out.opcode != Op::kCallMethod) {
        return Support::Unexpected(std::format("unknown action {:#04x}", out.opcode));
    }
    return pos;
}

}

Support::Expected<Script, std::string> Read(std::span<const uint8_t> code) {
    Script out;
    std::size_t pos = 0;
    while (pos < code.size()) {
        Instruction instruction{.opcode = code[pos],
                                .items = {},
                                .registers = {},
                                .flags = 0,
                                .frame_bias = 0,
                                .has_frame_bias = false};
        pos++;
        if (instruction.opcode == Op::kEnd) {
            out.instructions.push_back(instruction);
            out.trailing.assign(code.begin() + static_cast<std::ptrdiff_t>(pos), code.end());
            return out;
        }
        auto next = ReadOperands(code, pos, instruction);
        if (!next) return Support::Unexpected(next.error());
        pos = *next;
        out.instructions.push_back(std::move(instruction));
    }
    return out;
}

Support::Expected<std::vector<uint8_t>, std::string> Write(const Script& script) {
    std::vector<uint8_t> out;
    for (const Instruction& instruction : script.instructions) {
        out.push_back(instruction.opcode);
        if (instruction.opcode == Op::kPush) {
            if (instruction.items.size() > 0xFF)
                return Support::Unexpected(std::string("a push holds more than 255 items"));
            out.push_back(static_cast<uint8_t>(instruction.items.size()));
            for (const Item& item : instruction.items) {
                const std::optional<std::size_t> size = PushOperandSize(item.type);
                if (!size || item.operand.size() != *size) {
                    return Support::Unexpected(
                        std::format("push type {} has the wrong operand", item.type));
                }
                out.push_back(item.type);
                out.insert(out.end(), item.operand.begin(), item.operand.end());
            }
        } else if (instruction.opcode == Op::kStoreRegister) {
            if (instruction.registers.size() > 0xFF)
                return Support::Unexpected(std::string("a store register holds more than 255"));
            out.push_back(static_cast<uint8_t>(instruction.registers.size()));
            out.insert(out.end(), instruction.registers.begin(), instruction.registers.end());
        } else if (instruction.opcode == Op::kGotoFrame2) {
            out.push_back(instruction.flags);
            if (((instruction.flags & 0x2) != 0) != instruction.has_frame_bias)
                return Support::Unexpected(std::string("goto frame 2 flags and bias disagree"));
            if (instruction.has_frame_bias) BigEndian::AppendU16(out, instruction.frame_bias);
        }
    }
    out.insert(out.end(), script.trailing.begin(), script.trailing.end());
    return out;
}

std::string OpcodeName(uint8_t opcode) {
    switch (opcode) {
    case Op::kEnd:
        return "END";
    case Op::kPop:
        return "POP";
    case Op::kGetVariable:
        return "GET_VARIABLE";
    case Op::kCallFunction:
        return "CALL_FUNCTION";
    case Op::kSetMember:
        return "SET_MEMBER";
    case Op::kCallMethod:
        return "CALL_METHOD";
    case Op::kStoreRegister:
        return "STORE_REGISTER";
    case Op::kPush:
        return "PUSH";
    case Op::kGotoFrame2:
        return "GOTO_FRAME2";
    default:
        return std::format("{:#04x}", opcode);
    }
}

bool ItemIsString(const Item& item) {
    return item.type == PushType::kShortString || item.type == PushType::kLongString;
}

std::optional<uint16_t> BuiltinId(const Item& item) {
    if (item.operand.empty()) return std::nullopt;
    const auto base = [&]() -> std::optional<uint16_t> {
        switch (item.type) {
        case 16:
        case 17:
        case 18:
            return uint16_t{0x100};
        case 25:
            return uint16_t{0x200};
        case 19:
            return uint16_t{0x300};
        case 22:
            return uint16_t{0x400};
        case 28:
            return uint16_t{0x500};
        case 31:
            return uint16_t{0x600};
        case 36:
            return uint16_t{0x700};
        case 39:
            return uint16_t{0x800};
        case 42:
            return uint16_t{0x900};
        case 45:
            return uint16_t{0xA00};
        case 48:
            return uint16_t{0xB00};
        case 52:
            return uint16_t{0xC00};
        case 56:
            return uint16_t{0xD00};
        default:
            return std::nullopt;
        }
    }();
    if (!base) return std::nullopt;
    return static_cast<uint16_t>(*base + item.operand.front());
}

uint16_t StringIndex(const Item& item) {
    if (item.type == PushType::kShortString) return item.operand.front();
    return BigEndian::ReadU16(item.operand, 0);
}

Support::Expected<int32_t, std::string> ItemNumber(const Item& item) {
    switch (item.type) {
    case PushType::kZero:
        return 0;
    case PushType::kByte:
        return static_cast<int8_t>(item.operand.front());
    case PushType::kInteger:
        return static_cast<int32_t>(BigEndian::ReadU32(item.operand, 0));
    default:
        return Support::Unexpected(std::format("push type {} is not a number", item.type));
    }
}

Item NumberItem(int32_t value) {
    if (value == 0) return Item{.type = PushType::kZero, .operand = {}};
    if (value >= -128 && value <= 127) {
        return Item{.type = PushType::kByte,
                    .operand = {static_cast<uint8_t>(static_cast<int8_t>(value))}};
    }
    std::vector<uint8_t> bytes;
    BigEndian::AppendU32(bytes, static_cast<uint32_t>(value));
    return Item{.type = PushType::kInteger, .operand = std::move(bytes)};
}

}
