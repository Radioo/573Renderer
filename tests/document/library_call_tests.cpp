#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/library_call.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"

#include <cstdint>
#include <utility>
#include <string>
#include <vector>

namespace {

AfpAnimation::Animation Movie() {
    AfpAnimation::Animation animation;
    animation.strings = {"", "aeplib", "aep_set_set_frame", "loop"};
    return animation;
}

AfpAnimation::Bytecode CallBytes(uint8_t argument_count, const std::vector<uint8_t>& arguments) {
    std::vector<uint8_t> code = {AfpScript::Op::kPush, static_cast<uint8_t>(argument_count + 2)};
    code.insert(code.end(), arguments.begin(), arguments.end());
    code.push_back(AfpScript::PushType::kByte);
    code.push_back(argument_count);
    code.push_back(AfpScript::PushType::kShortString);
    code.push_back(0);
    code.push_back(AfpScript::Op::kGetVariable);
    code.push_back(AfpScript::Op::kPush);
    code.push_back(1);
    code.push_back(AfpScript::PushType::kShortString);
    code.push_back(1);
    code.push_back(AfpScript::Op::kCallMethod);
    code.push_back(AfpScript::Op::kPop);
    code.push_back(AfpScript::Op::kEnd);
    return AfpAnimation::Bytecode{
        .flags = 0, .strings = std::vector<AfpAnimation::StringId>{1, 2}, .code = std::move(code)};
}

}

TEST_CASE("A single aeplib call reads as its method and arguments") {
    const AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode bytecode =
        CallBytes(2, {AfpScript::PushType::kByte, 5, AfpScript::PushType::kStoredObject});
    const Document::LibraryCall call =
        Document::ReadLibraryCall(animation, bytecode).value_or(Document::LibraryCall{});
    CHECK(call.object == "aeplib");
    CHECK(call.method == "aep_set_set_frame");
    REQUIRE(call.arguments.size() == 2);
    CHECK(call.arguments[0].text == "this");
    CHECK_FALSE(call.arguments[0].is_string);
    CHECK(call.arguments[1].text == "5");
}

TEST_CASE("A call written back unchanged keeps its bytes") {
    AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode bytecode =
        CallBytes(2, {AfpScript::PushType::kByte, 5, AfpScript::PushType::kStoredObject});
    const auto call = Document::ReadLibraryCall(animation, bytecode);
    REQUIRE(call.has_value());
    const auto written = Document::WriteLibraryCall(animation, bytecode, *call);
    REQUIRE(written.has_value());
    CHECK(written->code == bytecode.code);
    CHECK(written->strings == bytecode.strings);
}

TEST_CASE("Changing an argument changes only that push item") {
    AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode bytecode =
        CallBytes(2, {AfpScript::PushType::kByte, 5, AfpScript::PushType::kStoredObject});
    Document::LibraryCall call =
        Document::ReadLibraryCall(animation, bytecode).value_or(Document::LibraryCall{});
    REQUIRE(call.arguments.size() == 2);
    call.arguments[1].text = "300";
    const auto written = Document::WriteLibraryCall(animation, bytecode, call);
    REQUIRE(written.has_value());
    const Document::LibraryCall again =
        Document::ReadLibraryCall(animation, *written).value_or(Document::LibraryCall{});
    REQUIRE(again.arguments.size() == 2);
    CHECK(again.arguments[1].text == "300");
    CHECK(again.arguments[0].text == "this");
    CHECK(again.method == "aep_set_set_frame");
}

TEST_CASE("A string argument interns the text it is given") {
    AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode bytecode =
        CallBytes(2, {AfpScript::PushType::kShortString, 1, AfpScript::PushType::kStoredObject});
    Document::LibraryCall call =
        Document::ReadLibraryCall(animation, bytecode).value_or(Document::LibraryCall{});
    REQUIRE(call.arguments.size() == 2);
    REQUIRE(call.arguments[1].is_string);
    CHECK(call.arguments[1].text == "aep_set_set_frame");
    call.arguments[1].text = "intro";
    const auto written = Document::WriteLibraryCall(animation, bytecode, call);
    REQUIRE(written.has_value());
    const Document::LibraryCall again =
        Document::ReadLibraryCall(animation, *written).value_or(Document::LibraryCall{});
    REQUIRE(again.arguments.size() == 2);
    CHECK(again.arguments[1].text == "intro");
    CHECK(Document::InternString(animation, "intro") < animation.strings.size());
}

TEST_CASE("An argument that is not a number is refused") {
    AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode bytecode =
        CallBytes(2, {AfpScript::PushType::kByte, 5, AfpScript::PushType::kStoredObject});
    Document::LibraryCall call =
        Document::ReadLibraryCall(animation, bytecode).value_or(Document::LibraryCall{});
    REQUIRE(call.arguments.size() == 2);
    call.arguments[1].text = "later";
    CHECK_FALSE(Document::WriteLibraryCall(animation, bytecode, call).has_value());
}

TEST_CASE("A script that is not one call is not read as one") {
    AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode stop{
        .flags = 0, .strings = {}, .code = {AfpScript::Op::kPop, AfpScript::Op::kEnd}};
    CHECK_FALSE(Document::ReadLibraryCall(animation, stop).has_value());
    CHECK_FALSE(Document::WriteLibraryCall(animation, stop, Document::LibraryCall{}).has_value());
}

TEST_CASE("A script the editor does not recognise still lists its instructions") {
    const AfpAnimation::Animation animation = Movie();
    const AfpAnimation::Bytecode bytecode{
        .flags = 0,
        .strings = std::vector<AfpAnimation::StringId>{3},
        .code = {AfpScript::Op::kPush, 2, AfpScript::PushType::kShortString, 0,
                 AfpScript::PushType::kByte, 7, AfpScript::Op::kStoreRegister, 1, 4,
                 AfpScript::Op::kEnd}};
    const std::vector<std::string> lines = Document::ScriptListing(animation, bytecode);
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "PUSH \"loop\" 7");
    CHECK(lines[1] == "STORE_REGISTER r4");
    CHECK(lines[2] == "END");
}

TEST_CASE("The aeplib calls the target build's data uses are named") {
    const AfpAnimation::Animation animation;
    const AfpAnimation::Bytecode bytecode{
        .flags = 0,
        .strings = {},
        .code = {AfpScript::Op::kPush, 3, AfpScript::PushType::kStoredObject,
                 AfpScript::PushType::kByte, 1, 19, 0x90, AfpScript::Op::kGetVariable,
                 AfpScript::Op::kPush, 1, 39, 0x36, AfpScript::Op::kCallMethod, AfpScript::Op::kPop,
                 AfpScript::Op::kEnd}};
    const Document::LibraryCall call =
        Document::ReadLibraryCall(animation, bytecode).value_or(Document::LibraryCall{});
    CHECK(call.object == "aeplib");
    CHECK(call.method == "aep_set_set_frame");
    REQUIRE(call.arguments.size() == 1);
    CHECK(call.arguments[0].text == "this");
}
