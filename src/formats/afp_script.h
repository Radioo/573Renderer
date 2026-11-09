#pragma once

#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace AfpScript {

namespace Op {
constexpr uint8_t kEnd = 0x00;
constexpr uint8_t kPop = 0x0D;
constexpr uint8_t kGetVariable = 0x0E;
constexpr uint8_t kCallFunction = 0x1E;
constexpr uint8_t kSetMember = 0x2F;
constexpr uint8_t kCallMethod = 0x32;
constexpr uint8_t kStoreRegister = 0x3F;
constexpr uint8_t kPush = 0x43;
constexpr uint8_t kGotoFrame2 = 0x47;
}

namespace PushType {
constexpr uint8_t kZero = 0;
constexpr uint8_t kFloat = 1;
constexpr uint8_t kRegister = 4;
constexpr uint8_t kInteger = 7;
constexpr uint8_t kShortString = 8;
constexpr uint8_t kLongString = 9;
constexpr uint8_t kStoredObject = 12;
constexpr uint8_t kDouble = 51;
constexpr uint8_t kByte = 55;
}

struct Item {
    uint8_t type = PushType::kZero;
    std::vector<uint8_t> operand;
};

struct Instruction {
    uint8_t opcode = Op::kEnd;
    std::vector<Item> items;
    std::vector<uint8_t> registers;
    uint8_t flags = 0;
    uint16_t frame_bias = 0;
    bool has_frame_bias = false;
};

struct Script {
    std::vector<Instruction> instructions;
    std::vector<uint8_t> trailing;
};

[[nodiscard]] Support::Expected<Script, std::string> Read(std::span<const uint8_t> code);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string> Write(const Script& script);

[[nodiscard]] std::string OpcodeName(uint8_t opcode);

[[nodiscard]] bool ItemIsString(const Item& item);

[[nodiscard]] uint16_t StringIndex(const Item& item);

[[nodiscard]] std::optional<uint16_t> BuiltinId(const Item& item);

[[nodiscard]] Support::Expected<int32_t, std::string> ItemNumber(const Item& item);

[[nodiscard]] Item NumberItem(int32_t value);

[[nodiscard]] std::optional<Item> BuiltinItem(uint16_t id);

}
