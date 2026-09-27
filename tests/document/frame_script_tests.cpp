#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/clip_edit.h"
#include "document/inspector.h"
#include "document/library_call.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

AfpAnimation::Bytecode CallOf(AfpAnimation::Animation& animation, const std::string& argument) {
    AfpAnimation::Bytecode code;
    code.strings = std::vector<AfpAnimation::StringId>{Document::InternString(animation, argument),
                                                       Document::InternString(animation, "play")};
    AfpScript::Script script;
    AfpScript::Instruction push;
    push.opcode = AfpScript::Op::kPush;
    push.items = {
        AfpScript::Item{.type = AfpScript::PushType::kShortString, .operand = {0}},
        AfpScript::Item{.type = AfpScript::PushType::kByte, .operand = {1}},
        AfpScript::Item{.type = 19, .operand = {0}},
    };
    AfpScript::Instruction get;
    get.opcode = AfpScript::Op::kGetVariable;
    AfpScript::Instruction method;
    method.opcode = AfpScript::Op::kPush;
    method.items = {AfpScript::Item{.type = 39, .operand = {0}}};
    AfpScript::Instruction call;
    call.opcode = AfpScript::Op::kCallMethod;
    AfpScript::Instruction stop;
    stop.opcode = AfpScript::Op::kEnd;
    script.instructions = {push, get, method, call, stop};
    const auto written = AfpScript::Write(script);
    REQUIRE(written.has_value());
    code.code = *written;
    return code;
}

AfpAnimation::Animation Scripted(const std::string& argument = "intro") {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Action action;
    action.bytecode = CallOf(animation, argument);
    animation.root.tags = {AfpAnimation::Tag{action}};
    animation.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 0},
                             AfpAnimation::Frame{.first_tag = 0, .tag_count = 1}};
    return animation;
}

std::vector<std::string> RowNames(const std::vector<Document::InspectedRow>& rows) {
    std::vector<std::string> named;
    named.reserve(rows.size());
    for (const Document::InspectedRow& row : rows)
        named.push_back(row.field.name);
    return named;
}

const Document::InspectedRow* RowNamed(const std::vector<Document::InspectedRow>& rows,
                                       const std::string& name) {
    const auto found = std::ranges::find(
        rows, name, [](const Document::InspectedRow& row) { return row.field.name; });
    return found == rows.end() ? nullptr : &*found;
}

}

TEST_CASE("The script tag of a frame is found only on the frame that carries it") {
    const AfpAnimation::Animation animation = Scripted();

    CHECK_FALSE(Document::FrameScriptTag(animation.root, 0).has_value());
    CHECK(Document::FrameScriptTag(animation.root, 1) == std::optional<std::size_t>{0});
    CHECK_FALSE(Document::FrameScriptTag(animation.root, 9).has_value());
}

TEST_CASE("A frame script shows in the inspector with its call and an editable argument") {
    const AfpAnimation::Animation animation = Scripted();

    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = std::nullopt,
                                                              .frame = 1,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt,
                                                              .clip = {}});

    const std::vector<std::string> named = RowNames(rows);
    CHECK(std::ranges::find(named, "Script on frame") != named.end());
    const Document::InspectedRow* argument = RowNamed(rows, "Call argument 1");
    REQUIRE(argument != nullptr);
    CHECK(argument->field.value == "intro");
    CHECK(argument->edits == Document::EditTarget::FrameCallArgument);
}

TEST_CASE("A frame with no script shows no script rows") {
    const AfpAnimation::Animation animation = Scripted();

    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = std::nullopt,
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt,
                                                              .clip = {}});

    const std::vector<std::string> named = RowNames(rows);
    CHECK(std::ranges::find(named, "Script on frame") == named.end());
}

TEST_CASE("Editing a frame script argument rewrites the call it belongs to") {
    AfpAnimation::Animation animation = Scripted();

    REQUIRE(Document::EditFrameCallArgument(animation, {}, 1, 0, "outro").has_value());

    const auto* action = std::get_if<AfpAnimation::Action>(&animation.root.tags[0].body);
    REQUIRE(action != nullptr);
    const std::optional<Document::LibraryCall> call =
        Document::ReadLibraryCall(animation, action->bytecode);
    REQUIRE(call.has_value());
    if (!call) return;
    REQUIRE(call->arguments.size() == 1);
    CHECK(call->arguments.front().text == "outro");
}

TEST_CASE("Editing a frame that carries no script is refused by name") {
    AfpAnimation::Animation animation = Scripted();

    const auto refused = Document::EditFrameCallArgument(animation, {}, 0, 0, "outro");

    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error() == std::string("frame 0 carries no script"));
}

TEST_CASE("An argument the call does not have is refused rather than written past") {
    AfpAnimation::Animation animation = Scripted();

    CHECK_FALSE(Document::EditFrameCallArgument(animation, {}, 1, 4, "outro").has_value());
}
