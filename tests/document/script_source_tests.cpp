#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/script_source.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace {

AfpAnimation::Animation Animation() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    return animation;
}

std::string Shape(const AfpAnimation::Bytecode& bytecode) {
    const auto script = AfpScript::Read(bytecode.code);
    if (!script) return script.error();
    std::string out;
    for (const AfpScript::Instruction& instruction : script->instructions) {
        if (!out.empty()) out += " ";
        out += AfpScript::OpcodeName(instruction.opcode);
        if (instruction.opcode == AfpScript::Op::kPush)
            out += "(" + std::to_string(instruction.items.size()) + ")";
    }
    return out;
}

}

TEST_CASE("A call compiles to the shape the shipped scripts use") {
    AfpAnimation::Animation animation = Animation();
    const auto code = Document::CompileScript(animation, "aep_set_set_frame(1, 0)");
    if (!code) FAIL(code.error());
    CHECK(Shape(*code) == "PUSH(4) GET_VARIABLE PUSH(1) CALL_METHOD POP END");
}

TEST_CASE("Two calls compile to one script") {
    AfpAnimation::Animation animation = Animation();
    const auto code = Document::CompileScript(animation, "stop()\ndeepStop()\n");
    REQUIRE(code.has_value());
    CHECK(Shape(*code) ==
          "PUSH(2) GET_VARIABLE PUSH(1) CALL_METHOD POP PUSH(2) GET_VARIABLE PUSH(1) CALL_METHOD "
          "POP END");
}

TEST_CASE("A compiled script reads back as the source it came from") {
    AfpAnimation::Animation animation = Animation();
    const std::string source = "gotoAndPlay(\"loop\")\naep_set_rect_mask(0, 0, 1920, 1080)\n";
    const auto code = Document::CompileScript(animation, source);
    REQUIRE(code.has_value());
    const std::optional<std::string> back = Document::ScriptSourceText(animation, *code);
    REQUIRE(back.has_value());
    CHECK(*back == source);
}

TEST_CASE("Compiling the same source twice gives the same bytes") {
    AfpAnimation::Animation first = Animation();
    AfpAnimation::Animation second = Animation();
    const std::string source = "aep_set_set_frame(2, 7)\ngotoAndStop(\"end\")\n";
    const auto one = Document::CompileScript(first, source);
    const auto two = Document::CompileScript(second, source);
    REQUIRE(one.has_value());
    REQUIRE(two.has_value());
    CHECK(one->code == two->code);
    CHECK(one->strings == two->strings);
}

TEST_CASE("Blank lines and spacing do not change what is compiled") {
    AfpAnimation::Animation plain = Animation();
    AfpAnimation::Animation spaced = Animation();
    const auto one = Document::CompileScript(plain, "stop()");
    const auto two = Document::CompileScript(spaced, "\n   stop( )  \n\n");
    REQUIRE(one.has_value());
    REQUIRE(two.has_value());
    CHECK(one->code == two->code);
}

TEST_CASE("A call the game's own data never makes is refused") {
    AfpAnimation::Animation animation = Animation();
    const auto refused = Document::CompileScript(animation, "play()");
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().find("play") != std::string::npos);

    CHECK_FALSE(Document::CompileScript(animation, "aeplib.stop()").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "stop").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "stop(").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "\n\n").has_value());
}

TEST_CASE("An argument that is neither a number nor quoted text is refused") {
    AfpAnimation::Animation animation = Animation();
    CHECK_FALSE(Document::CompileScript(animation, "gotoAndPlay(loop)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "gotoAndPlay(\"loop)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "aep_set_set_frame(1.5)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "aep_set_set_frame(1, )").has_value());
}

TEST_CASE("A comma inside quoted text is part of the text") {
    AfpAnimation::Animation animation = Animation();
    const auto code = Document::CompileScript(animation, "gotoAndPlay(\"a,b\")");
    REQUIRE(code.has_value());
    const std::optional<std::string> back = Document::ScriptSourceText(animation, *code);
    REQUIRE(back.has_value());
    CHECK(*back == "gotoAndPlay(\"a,b\")\n");
}

TEST_CASE("The calls the editor knows are the ones the survey counted") {
    CHECK(Document::ScriptCalls().size() == 8);
    for (const std::string_view name :
         {"stop", "gotoAndPlay", "gotoAndStop", "deepStop", "deepGotoAndPlay",
          "aep_set_frame_control", "aep_set_rect_mask", "aep_set_set_frame"}) {
        CHECK(std::ranges::find(Document::ScriptCalls(), name) != Document::ScriptCalls().end());
    }
}

TEST_CASE("Bytecode that is not a run of aeplib calls has no source") {
    const AfpAnimation::Animation animation = Animation();
    const AfpAnimation::Bytecode empty;
    CHECK_FALSE(Document::ScriptSourceText(animation, empty).has_value());

    AfpScript::Script script;
    script.instructions.push_back(AfpScript::Instruction{.opcode = AfpScript::Op::kGetVariable,
                                                         .items = {},
                                                         .registers = {},
                                                         .flags = 0,
                                                         .frame_bias = 0,
                                                         .has_frame_bias = false});
    script.instructions.push_back(AfpScript::Instruction{.opcode = AfpScript::Op::kEnd,
                                                         .items = {},
                                                         .registers = {},
                                                         .flags = 0,
                                                         .frame_bias = 0,
                                                         .has_frame_bias = false});
    AfpAnimation::Bytecode other;
    other.code = *AfpScript::Write(script);
    CHECK_FALSE(Document::ScriptSourceText(animation, other).has_value());
}

namespace {

AfpAnimation::Placement Scripted(bool with_actions) {
    AfpAnimation::Placement placement;
    placement.depth = 1;
    placement.end_frame = 2;
    placement.character = uint16_t{5};
    if (!with_actions) return placement;
    AfpAnimation::ClipEvent event;
    event.triggers = 0x20000;
    AfpAnimation::Animation scratch = Animation();
    event.bytecode = *Document::CompileScript(scratch, "stop()");
    placement.clip_actions =
        AfpAnimation::ClipActions{.unread_value = 7, .unread_word = 3, .events = {event}};
    return placement;
}

Document::BakedDepth Baked(bool with_actions) {
    return Document::BakedDepth{.create = Scripted(with_actions),
                                .update_flags = 1,
                                .update_extended_flags = std::nullopt,
                                .blank_frames = {}};
}

Document::AuthoredDepth Owning(const std::string& source) {
    return Document::AuthoredDepth{.animation = "afp/a",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 0,
                                   .tracks = {},
                                   .script = source};
}

}

TEST_CASE("An owned script is compiled into the placement export writes") {
    AfpAnimation::Animation animation = Animation();
    animation.root.frames = {AfpAnimation::Frame{}};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Scripted(true)});

    const Document::BakedDepth baked = Baked(true);
    const auto written =
        Document::WriteAuthored(animation, Owning("gotoAndPlay(\"loop\")\n"), baked);
    if (!written) FAIL(written.error());

    const auto* placement = std::get_if<AfpAnimation::Placement>(&animation.root.tags[0].body);
    REQUIRE(placement != nullptr);
    REQUIRE(placement->clip_actions.has_value());
    const AfpAnimation::ClipActions& actions = *placement->clip_actions;
    REQUIRE(actions.events.size() == 1);
    CHECK(actions.events.front().triggers == 0x20000);
    CHECK(actions.unread_value == 7);
    const std::optional<std::string> back =
        Document::ScriptSourceText(animation, actions.events.front().bytecode);
    REQUIRE(back.has_value());
    CHECK(*back == "gotoAndPlay(\"loop\")\n");
}

TEST_CASE("A depth with no script of its own refuses to be given one") {
    AfpAnimation::Animation animation = Animation();
    animation.root.frames = {AfpAnimation::Frame{}};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Scripted(false)});

    const auto written = Document::WriteAuthored(animation, Owning("stop()\n"), Baked(false));
    REQUIRE_FALSE(written.has_value());
    CHECK(written.error().find("no script") != std::string::npos);
}

TEST_CASE("A script that will not compile stops the write") {
    AfpAnimation::Animation animation = Animation();
    animation.root.frames = {AfpAnimation::Frame{}};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Scripted(true)});
    const AfpAnimation::Container before = animation.root;

    CHECK_FALSE(Document::WriteAuthored(animation, Owning("play()\n"), Baked(true)).has_value());
    CHECK(animation.root == before);
}
