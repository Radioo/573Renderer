#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/authored.h"
#include "document/library_call.h"
#include "document/script_source.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "formats/afp_animation_detail.h"
#include "formats/afp_layout.h"
#include "formats/afp_script.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <string>
#include <variant>
#include <vector>

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

TEST_CASE("The register chain arena.ifs ships compiles to the packing the game ships") {
    AfpAnimation::Animation animation = Animation();
    const std::string source = "let r1 = getInstanceAtDepth(-16382)\n"
                               "keep r1.gotoAndPlay(540)\n"
                               "r1.frameOffset = 539\n";
    const auto code = Document::CompileScript(animation, source);
    if (!code) FAIL(code.error());
    CHECK(Shape(*code) ==
          "PUSH(5) CALL_FUNCTION STORE_REGISTER PUSH(1) CALL_METHOD PUSH(3) SET_MEMBER END");

    const std::vector<std::string> listing = Document::ScriptListing(animation, *code);
    REQUIRE(listing.size() == 8);
    CHECK(listing[0] == "PUSH 540 1 -16382 1 getInstanceAtDepth");
    CHECK(listing[3] == "PUSH gotoAndPlay");
    CHECK(listing[5] == "PUSH r1 \"frameOffset\" 539");

    const std::optional<std::string> back = Document::ScriptSourceText(animation, *code);
    REQUIRE(back.has_value());
    CHECK(*back == source);
}

TEST_CASE("A method call on a bound register pops its result unless it is kept") {
    AfpAnimation::Animation animation = Animation();
    const std::string source = "let r2 = getInstanceAtDepth(-16382)\nr2.gotoAndStop(3)\n";
    const auto code = Document::CompileScript(animation, source);
    if (!code) FAIL(code.error());
    CHECK(Shape(*code) == "PUSH(5) CALL_FUNCTION STORE_REGISTER PUSH(1) CALL_METHOD POP END");
    const std::optional<std::string> back = Document::ScriptSourceText(animation, *code);
    REQUIRE(back.has_value());
    CHECK(*back == source);
}

TEST_CASE("A let with no method call after it stands on its own") {
    AfpAnimation::Animation animation = Animation();
    const std::string source = "let r1 = getInstanceAtDepth(-16382)\nstop()\n";
    const auto code = Document::CompileScript(animation, source);
    if (!code) FAIL(code.error());
    CHECK(Shape(*code) ==
          "PUSH(3) CALL_FUNCTION STORE_REGISTER PUSH(2) GET_VARIABLE PUSH(1) CALL_METHOD POP END");
    const std::optional<std::string> back = Document::ScriptSourceText(animation, *code);
    REQUIRE(back.has_value());
    CHECK(*back == source);
}

TEST_CASE("A member write names the object the game pushed") {
    AfpAnimation::Animation animation = Animation();
    const std::string source = "this.frameOffset = 12\nr3.loopCount = -1\n";
    const auto code = Document::CompileScript(animation, source);
    if (!code) FAIL(code.error());
    CHECK(Shape(*code) == "PUSH(3) SET_MEMBER PUSH(3) SET_MEMBER END");
    const std::optional<std::string> back = Document::ScriptSourceText(animation, *code);
    REQUIRE(back.has_value());
    CHECK(*back == source);
}

TEST_CASE("A register statement the language cannot place is refused") {
    AfpAnimation::Animation animation = Animation();
    CHECK_FALSE(Document::CompileScript(animation, "r1.gotoAndPlay(540)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "let x = getInstanceAtDepth(1)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "let r1 = 7").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "let r1 = getInstanceAtDepth(1)\nr1.sprocket()")
                    .has_value());
}

TEST_CASE("A call the game's own data never makes is refused") {
    AfpAnimation::Animation animation = Animation();
    const auto refused = Document::CompileScript(animation, "sprocket()");
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().find("sprocket") != std::string::npos);

    CHECK_FALSE(Document::CompileScript(animation, "aeplib.stop()").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "stop").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "stop(").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "keep push(0)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "keep sprocket()").has_value());
}

TEST_CASE("A script with nothing in it is the one instruction that ends it") {
    AfpAnimation::Animation animation = Animation();
    const auto code = Document::CompileScript(animation, "");
    REQUIRE(code.has_value());
    CHECK(Shape(*code) == "END");
    CHECK(Document::ScriptSourceText(animation, *code) == "");
    CHECK(Document::CompileScript(animation, "\n\n")->code == code->code);
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

TEST_CASE("The calls the editor knows are the builtins afp-core names") {
    const auto known = [](const std::string_view name) {
        return std::ranges::find(Document::ScriptCalls(), name) != Document::ScriptCalls().end();
    };
    for (const std::string_view name :
         {"stop", "gotoAndPlay", "gotoAndStop", "deepStop", "deepGotoAndPlay",
          "aep_set_frame_control", "aep_set_rect_mask", "aep_set_set_frame", "getInstanceAtDepth",
          "play", "_x", "blendMode"}) {
        CHECK(known(name));
    }
    for (const std::string_view reserved : {"this", "push", "pop", "sprocket"}) {
        CHECK_FALSE(known(reserved));
    }
    CHECK(Document::ScriptCalls().size() > 1000);
}

namespace {

AfpScript::Instruction Bare(uint8_t opcode) {
    return AfpScript::Instruction{.opcode = opcode,
                                  .items = {},
                                  .registers = {},
                                  .flags = 0,
                                  .frame_bias = 0,
                                  .has_frame_bias = false};
}

AfpScript::Instruction Pushing(std::vector<AfpScript::Item> items) {
    AfpScript::Instruction out = Bare(AfpScript::Op::kPush);
    out.items = std::move(items);
    return out;
}

AfpAnimation::Bytecode Assembled(std::vector<AfpScript::Instruction> instructions) {
    AfpScript::Script script;
    script.instructions = std::move(instructions);
    script.instructions.push_back(Bare(AfpScript::Op::kEnd));
    AfpAnimation::Bytecode out;
    out.code = *AfpScript::Write(script);
    return out;
}

void RoundTrips(AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& bytecode) {
    const std::string source =
        Document::ScriptSourceText(animation, bytecode).value_or(std::string());
    REQUIRE_FALSE(source.empty());
    const auto again = Document::CompileScript(animation, source);
    if (!again) FAIL(again.error() + " for: " + source);
    CHECK(again->code == bytecode.code);
}

AfpScript::Item Register(uint8_t number) {
    return AfpScript::Item{.type = AfpScript::PushType::kRegister, .operand = {number}};
}

}

TEST_CASE("Bytecode that cannot be read has no source") {
    const AfpAnimation::Animation animation = Animation();
    const AfpAnimation::Bytecode empty;
    CHECK_FALSE(Document::ScriptSourceText(animation, empty).has_value());

    AfpAnimation::Bytecode unfinished;
    unfinished.code = {AfpScript::Op::kGetVariable};
    CHECK_FALSE(Document::ScriptSourceText(animation, unfinished).has_value());
}

TEST_CASE("A string the compiler could not put back in the same slot has no source") {
    AfpAnimation::Animation animation = Animation();
    const AfpScript::Item spelled_wide{.type = AfpScript::PushType::kLongString, .operand = {0, 0}};
    AfpAnimation::Bytecode wide = Assembled({Pushing({spelled_wide})});
    wide.strings = std::vector<AfpAnimation::StringId>{Document::InternString(animation, "loop")};
    CHECK_FALSE(Document::ScriptSourceText(animation, wide).has_value());

    const AfpScript::Item past_the_end{.type = AfpScript::PushType::kShortString, .operand = {3}};
    AfpAnimation::Bytecode dangling = Assembled({Pushing({past_the_end})});
    dangling.strings = std::vector<AfpAnimation::StringId>{};
    CHECK_FALSE(Document::ScriptSourceText(animation, dangling).has_value());

    AfpAnimation::Bytecode quoted = Assembled(
        {Pushing({AfpScript::Item{.type = AfpScript::PushType::kShortString, .operand = {0}}})});
    quoted.strings = std::vector<AfpAnimation::StringId>{Document::InternString(animation, "a\"b")};
    CHECK_FALSE(Document::ScriptSourceText(animation, quoted).has_value());
}

TEST_CASE("A compiled script goes through the writer that stores it") {
    AfpAnimation::Animation animation = Animation();
    const auto code = Document::CompileScript(animation, "gotoAndPlay(\"loop\")\n");
    REQUIRE(code.has_value());
    REQUIRE(code->strings.has_value());
    CHECK((code->flags & AfpLayout::kBytecodeStrings) == 0);

    const std::size_t slots = animation.strings.size() +
                              code->strings.value_or(std::vector<AfpAnimation::StringId>{}).size();
    AfpAnimation::Detail::ByteWriter out(std::vector<uint32_t>(slots, 0));
    AfpAnimation::Detail::WriteBytecode(out, *code);
    if (out.Failed()) FAIL(out.Error());
}

TEST_CASE("A call the shipped data leaves on the stack reads back as keep") {
    AfpAnimation::Animation animation = Animation();
    const AfpAnimation::Bytecode bytecode =
        Assembled({Pushing({AfpScript::NumberItem(3), AfpScript::NumberItem(1),
                            *AfpScript::BuiltinItem(0x390)}),
                   Bare(AfpScript::Op::kGetVariable), Pushing({*AfpScript::BuiltinItem(0x443)}),
                   Bare(AfpScript::Op::kCallMethod)});
    CHECK(Document::ScriptSourceText(animation, bytecode).value_or(std::string()) ==
          "keep gotoAndStop(3)\n");
    RoundTrips(animation, bytecode);
}

TEST_CASE("A script that binds a register reads back as instructions") {
    AfpAnimation::Animation animation = Animation();
    AfpScript::Instruction store = Bare(AfpScript::Op::kStoreRegister);
    store.registers = {1};
    const AfpAnimation::Bytecode bytecode =
        Assembled({Pushing({AfpScript::NumberItem(540), AfpScript::NumberItem(1),
                            AfpScript::NumberItem(-16382), AfpScript::NumberItem(1),
                            *AfpScript::BuiltinItem(0x465)}),
                   Bare(AfpScript::Op::kCallFunction), store, Pushing({Register(1)}),
                   Bare(AfpScript::Op::kCallMethod), Bare(AfpScript::Op::kPop),
                   Pushing({AfpScript::NumberItem(2), AfpScript::NumberItem(0), Register(1)}),
                   Bare(AfpScript::Op::kSetMember)});
    const std::string text =
        Document::ScriptSourceText(animation, bytecode).value_or(std::string());
    REQUIRE_FALSE(text.empty());
    CHECK(text.find("call_function\n") != std::string::npos);
    CHECK(text.find("store r1\n") != std::string::npos);
    CHECK(text.find("push(r1)\n") != std::string::npos);
    CHECK(text.find("set_member\n") != std::string::npos);
    RoundTrips(animation, bytecode);
}

TEST_CASE("A jump reads back with its flags and its bias") {
    AfpAnimation::Animation animation = Animation();
    AfpScript::Instruction plain = Bare(AfpScript::Op::kGotoFrame2);
    plain.flags = 1;
    AfpScript::Instruction biased = Bare(AfpScript::Op::kGotoFrame2);
    biased.flags = 3;
    biased.frame_bias = 12;
    biased.has_frame_bias = true;

    const AfpAnimation::Bytecode one = Assembled({Pushing({AfpScript::NumberItem(0)}), plain});
    CHECK(Document::ScriptSourceText(animation, one) == "push(0)\ngoto_frame2(1)\n");
    RoundTrips(animation, one);

    const AfpAnimation::Bytecode two = Assembled({Pushing({AfpScript::NumberItem(0)}), biased});
    CHECK(Document::ScriptSourceText(animation, two) == "push(0)\ngoto_frame2(3, 12)\n");
    RoundTrips(animation, two);
}

TEST_CASE("A jump whose bias disagrees with its flags is refused") {
    AfpAnimation::Animation animation = Animation();
    CHECK_FALSE(Document::CompileScript(animation, "goto_frame2(3)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "goto_frame2(1, 5)").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "goto_frame2()").has_value());
    CHECK_FALSE(Document::CompileScript(animation, "goto_frame2").has_value());
    CHECK(Document::CompileScript(animation, "goto_frame2(1)").has_value());
    CHECK(Document::CompileScript(animation, "goto_frame2(3, 5)").has_value());
}

TEST_CASE("An item with no shorthand survives as its type and its bytes") {
    AfpAnimation::Animation animation = Animation();
    const AfpScript::Item odd{.type = AfpScript::PushType::kDouble,
                              .operand = {0x40, 0x59, 0, 0, 0, 0, 0, 0}};
    const AfpAnimation::Bytecode bytecode = Assembled({Pushing({odd})});
    CHECK(Document::ScriptSourceText(animation, bytecode) == "push(item(51, 4059000000000000))\n");
    RoundTrips(animation, bytecode);
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
                                .blank_frames = {},
                                .extra_controls = {},
                                .other_extended_frames = {},
                                .explicit_identities = {}};
}

Document::AuthoredDepth Owning(const std::string& source) {
    return Document::AuthoredDepth{.animation = "afp/a",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 0,
                                   .tracks = {},
                                   .script = source,
                                   .clip = {}};
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

    CHECK_FALSE(
        Document::WriteAuthored(animation, Owning("sprocket()\n"), Baked(true)).has_value());
    CHECK(animation.root == before);
}
