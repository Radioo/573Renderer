#include <catch2/catch_test_macros.hpp>

#include "formats/afp_script.h"

#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> LibraryCallBytes() {
    return {AfpScript::Op::kPush,
            4,
            AfpScript::PushType::kByte,
            5,
            AfpScript::PushType::kStoredObject,
            AfpScript::PushType::kByte,
            2,
            AfpScript::PushType::kShortString,
            0,
            AfpScript::Op::kGetVariable,
            AfpScript::Op::kPush,
            1,
            AfpScript::PushType::kShortString,
            1,
            AfpScript::Op::kCallMethod,
            AfpScript::Op::kPop,
            AfpScript::Op::kEnd};
}

}

TEST_CASE("A library call reads as the instructions afp-core would run") {
    const auto script = AfpScript::Read(LibraryCallBytes());
    REQUIRE(script.has_value());
    REQUIRE(script->instructions.size() == 6);
    CHECK(script->instructions[0].opcode == AfpScript::Op::kPush);
    REQUIRE(script->instructions[0].items.size() == 4);
    CHECK(script->instructions[0].items[0].type == AfpScript::PushType::kByte);
    CHECK(script->instructions[0].items[1].type == AfpScript::PushType::kStoredObject);
    CHECK(script->instructions[0].items[1].operand.empty());
    CHECK(AfpScript::ItemIsString(script->instructions[0].items[3]));
    CHECK(AfpScript::StringIndex(script->instructions[0].items[3]) == 0);
    CHECK(script->instructions[1].opcode == AfpScript::Op::kGetVariable);
    CHECK(script->instructions[4].opcode == AfpScript::Op::kPop);
    CHECK(script->instructions[5].opcode == AfpScript::Op::kEnd);
}

TEST_CASE("Every script writes back the bytes it was read from") {
    const std::vector<uint8_t> bytes = LibraryCallBytes();
    const auto script = AfpScript::Read(bytes);
    REQUIRE(script.has_value());
    const auto written = AfpScript::Write(*script);
    REQUIRE(written.has_value());
    CHECK(*written == bytes);
}

TEST_CASE("Every push type carries the operand size afp-core reads") {
    const std::vector<uint8_t> bytes = {AfpScript::Op::kPush,
                                        7,
                                        AfpScript::PushType::kZero,
                                        AfpScript::PushType::kFloat,
                                        0x3F,
                                        0x80,
                                        0x00,
                                        0x00,
                                        AfpScript::PushType::kRegister,
                                        3,
                                        AfpScript::PushType::kInteger,
                                        0x00,
                                        0x00,
                                        0x01,
                                        0x2C,
                                        AfpScript::PushType::kLongString,
                                        0x01,
                                        0x02,
                                        AfpScript::PushType::kDouble,
                                        1,
                                        2,
                                        3,
                                        4,
                                        5,
                                        6,
                                        7,
                                        8,
                                        18,
                                        1,
                                        0,
                                        2,
                                        AfpScript::Op::kEnd};
    const auto script = AfpScript::Read(bytes);
    REQUIRE(script.has_value());
    REQUIRE(script->instructions.size() == 2);
    REQUIRE(script->instructions[0].items.size() == 7);
    CHECK(script->instructions[0].items[1].operand.size() == 4);
    CHECK(script->instructions[0].items[2].operand.size() == 1);
    CHECK(script->instructions[0].items[4].operand.size() == 2);
    CHECK(AfpScript::StringIndex(script->instructions[0].items[4]) == 0x0102);
    CHECK(script->instructions[0].items[5].operand.size() == 8);
    CHECK(script->instructions[0].items[6].operand.size() == 3);
    const auto written = AfpScript::Write(*script);
    REQUIRE(written.has_value());
    CHECK(*written == bytes);
}

TEST_CASE("A store register and a goto frame keep their operands") {
    const std::vector<uint8_t> bytes = {AfpScript::Op::kStoreRegister,
                                        2,
                                        4,
                                        5,
                                        AfpScript::Op::kGotoFrame2,
                                        0x3,
                                        0x01,
                                        0x2C,
                                        AfpScript::Op::kGotoFrame2,
                                        0x1,
                                        AfpScript::Op::kEnd};
    const auto script = AfpScript::Read(bytes);
    REQUIRE(script.has_value());
    REQUIRE(script->instructions.size() == 4);
    CHECK(script->instructions[0].registers == std::vector<uint8_t>{4, 5});
    CHECK(script->instructions[1].has_frame_bias);
    CHECK(script->instructions[1].frame_bias == 300);
    CHECK_FALSE(script->instructions[2].has_frame_bias);
    const auto written = AfpScript::Write(*script);
    REQUIRE(written.has_value());
    CHECK(*written == bytes);
}

TEST_CASE("Bytes after the end opcode are kept and written back") {
    const std::vector<uint8_t> bytes = {AfpScript::Op::kPop, AfpScript::Op::kEnd, 0xAB, 0xCD};
    const auto script = AfpScript::Read(bytes);
    REQUIRE(script.has_value());
    CHECK(script->instructions.size() == 2);
    CHECK(script->trailing == std::vector<uint8_t>{0xAB, 0xCD});
    const auto written = AfpScript::Write(*script);
    REQUIRE(written.has_value());
    CHECK(*written == bytes);
}

TEST_CASE("A built-in id is the type's family plus its operand byte") {
    const std::vector<uint8_t> bytes = {
        AfpScript::Op::kPush, 3, 19, 0x90, 39, 0x36, 7, 0, 0, 1, 0x2C, AfpScript::Op::kEnd};
    const auto script = AfpScript::Read(bytes);
    REQUIRE(script.has_value());
    const std::vector<AfpScript::Item>& items = script->instructions[0].items;
    REQUIRE(items.size() == 3);
    CHECK(AfpScript::BuiltinId(items[0]) == 0x390);
    CHECK(AfpScript::BuiltinId(items[1]) == 0x836);
    CHECK_FALSE(AfpScript::BuiltinId(items[2]).has_value());
}

TEST_CASE("A script afp-core could not run is refused") {
    CHECK_FALSE(AfpScript::Read(std::vector<uint8_t>{0x99}).has_value());
    CHECK_FALSE(AfpScript::Read(std::vector<uint8_t>{AfpScript::Op::kPush}).has_value());
    CHECK_FALSE(AfpScript::Read(std::vector<uint8_t>{AfpScript::Op::kPush, 1, 20}).has_value());
    CHECK_FALSE(AfpScript::Read(std::vector<uint8_t>{AfpScript::Op::kPush, 1,
                                                     AfpScript::PushType::kInteger, 1, 2})
                    .has_value());
    CHECK_FALSE(
        AfpScript::Read(std::vector<uint8_t>{AfpScript::Op::kStoreRegister, 3, 1}).has_value());
    CHECK_FALSE(
        AfpScript::Read(std::vector<uint8_t>{AfpScript::Op::kGotoFrame2, 0x2, 1}).has_value());
}

TEST_CASE("Numbers round trip through the smallest push that holds them") {
    CHECK(AfpScript::NumberItem(0).type == AfpScript::PushType::kZero);
    CHECK(AfpScript::NumberItem(-1).type == AfpScript::PushType::kByte);
    CHECK(AfpScript::NumberItem(127).type == AfpScript::PushType::kByte);
    CHECK(AfpScript::NumberItem(128).type == AfpScript::PushType::kInteger);
    for (const int32_t value : {0, 1, -1, 127, -128, 128, 300, -30000, 1000000, -1000000}) {
        const auto read = AfpScript::ItemNumber(AfpScript::NumberItem(value));
        REQUIRE(read.has_value());
        CHECK(*read == value);
    }
}

TEST_CASE("Opcode names come from the table afp-core logs with") {
    CHECK(AfpScript::OpcodeName(AfpScript::Op::kCallMethod) == "CALL_METHOD");
    CHECK(AfpScript::OpcodeName(AfpScript::Op::kGotoFrame2) == "GOTO_FRAME2");
    CHECK(AfpScript::OpcodeName(0x99) == "0x99");
}
