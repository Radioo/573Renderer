#include <catch2/catch_test_macros.hpp>

#include "document/clip_edit.h"
#include "document/script_index.h"
#include "document/script_source.h"
#include "formats/afp_animation.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

AfpAnimation::Bytecode Compiled(AfpAnimation::Animation& animation, const std::string& source) {
    const auto code = Document::CompileScript(animation, source);
    REQUIRE(code.has_value());
    return *code;
}

AfpAnimation::Placement Owning(AfpAnimation::Animation& animation, uint16_t depth,
                               const std::string& source) {
    AfpAnimation::Placement placement;
    placement.depth = depth;
    placement.end_frame = 1;
    placement.character = uint16_t{5};
    AfpAnimation::ClipEvent event;
    event.triggers = 0x20000;
    event.bytecode = Compiled(animation, source);
    placement.clip_actions =
        AfpAnimation::ClipActions{.unread_value = 0, .unread_word = 0, .events = {event}};
    return placement;
}

AfpAnimation::Animation Peopled() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Action first;
    first.bytecode = Compiled(animation, "stop()\n");
    AfpAnimation::Action second;
    second.bytecode = Compiled(animation, "push(1)\ngoto_frame2(1)\n");
    AfpAnimation::Placement bare;
    bare.depth = 7;
    bare.end_frame = 1;
    bare.character = uint16_t{5};
    animation.root.tags = {AfpAnimation::Tag{Owning(animation, 4, "gotoAndPlay(\"loop\")\n")},
                           AfpAnimation::Tag{first}, AfpAnimation::Tag{bare},
                           AfpAnimation::Tag{second}};
    animation.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 3},
                             AfpAnimation::Frame{.first_tag = 3, .tag_count = 1}};
    return animation;
}

}

TEST_CASE("A script that binds a register is not marked as a run of calls") {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Action calls;
    calls.bytecode = Compiled(animation, "stop()\n");
    AfpAnimation::Action registers;
    registers.bytecode =
        Compiled(animation, "let r1 = getInstanceAtDepth(-16382)\nr1.frameOffset = 9\n");
    animation.root.tags = {AfpAnimation::Tag{calls}, AfpAnimation::Tag{registers}};
    animation.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 1},
                             AfpAnimation::Frame{.first_tag = 1, .tag_count = 1}};

    const std::vector<Document::ScriptEntry> found = Document::ScriptsIn(animation);
    REQUIRE(found.size() == 2);
    CHECK(found[0].shape == Document::ScriptShape::Call);
    CHECK(found[1].shape == Document::ScriptShape::Instructions);
    CHECK(found[1].preview == "let r1 = getInstanceAtDepth(-16382)");
}

TEST_CASE("The calls an animation names are the ones its own scripts call") {
    AfpAnimation::Animation animation = Peopled();
    AfpAnimation::Action third;
    third.bytecode = Compiled(animation, "let r1 = getInstanceAtDepth(-16382)\n"
                                         "keep r1.gotoAndStop(3)\n"
                                         "r1.frameOffset = 9\n");
    animation.root.tags.push_back(AfpAnimation::Tag{third});
    animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 4, .tag_count = 1});

    const std::vector<std::string> calls = Document::CallsIn(animation);
    CHECK(calls ==
          std::vector<std::string>{"getInstanceAtDepth", "gotoAndPlay", "gotoAndStop", "stop"});
}

TEST_CASE("The script index finds a frame script and a placement's own script") {
    const AfpAnimation::Animation animation = Peopled();
    const std::vector<Document::ScriptEntry> found = Document::ScriptsIn(animation);
    REQUIRE(found.size() == 3);

    CHECK(found[0].place.frame == 0);
    CHECK_FALSE(found[0].place.depth.has_value());
    CHECK(found[0].preview == "stop()");
    CHECK(found[0].shape == Document::ScriptShape::Call);

    CHECK(found[1].place.frame == 1);
    CHECK(found[1].preview == "push(1)");
    CHECK(found[1].shape == Document::ScriptShape::Instructions);

    REQUIRE(found[2].place.depth.has_value());
    CHECK(*found[2].place.depth == 4);
    CHECK(found[2].preview == "gotoAndPlay(\"loop\")");
}

TEST_CASE("The script index names the root clip rather than leaving it blank") {
    const AfpAnimation::Animation animation = Peopled();
    for (const Document::ScriptEntry& one : Document::ScriptsIn(animation))
        CHECK(one.clip_name == std::string("Root"));
}

TEST_CASE("A script fetched back by its place is the one the index listed") {
    const AfpAnimation::Animation animation = Peopled();
    for (const Document::ScriptEntry& one : Document::ScriptsIn(animation)) {
        const std::optional<AfpAnimation::Bytecode> code = Document::ScriptAt(animation, one.place);
        REQUIRE(code.has_value());
        const std::string source =
            Document::ScriptSourceText(animation, *code).value_or(std::string());
        CHECK(source.starts_with(one.preview));
    }
}

TEST_CASE("A place no script sits on fetches nothing") {
    const AfpAnimation::Animation animation = Peopled();
    CHECK_FALSE(Document::ScriptAt(
                    animation, Document::ScriptPlace{.clip = {}, .frame = 9, .depth = std::nullopt})
                    .has_value());
    CHECK_FALSE(Document::ScriptAt(
                    animation, Document::ScriptPlace{.clip = {}, .frame = 0, .depth = uint16_t{77}})
                    .has_value());
}

TEST_CASE("A placement's own script is compiled back into its clip actions") {
    AfpAnimation::Animation animation = Peopled();
    REQUIRE(Document::WritePlacementScript(animation, {}, 4, 0, "stop()\n").has_value());

    const std::optional<AfpAnimation::Bytecode> code = Document::ScriptAt(
        animation, Document::ScriptPlace{.clip = {}, .frame = 0, .depth = uint16_t{4}});
    REQUIRE(code.has_value());
    CHECK(Document::ScriptSourceText(animation, *code).value_or(std::string()) ==
          std::string("stop()\n"));
}

TEST_CASE("A placement with no script of its own refuses one") {
    AfpAnimation::Animation animation = Peopled();
    const auto refused = Document::WritePlacementScript(animation, {}, 7, 0, "stop()\n");
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().find("no script") != std::string::npos);
}

TEST_CASE("A placement script that will not compile leaves the clip actions alone") {
    AfpAnimation::Animation animation = Peopled();
    const AfpAnimation::Container before = animation.root;
    CHECK_FALSE(Document::WritePlacementScript(animation, {}, 4, 0, "sprocket()\n").has_value());
    CHECK(animation.root == before);
}
