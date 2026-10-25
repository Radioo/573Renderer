#include <catch2/catch_test_macros.hpp>

#include "document/script_problems.h"

#include <string>
#include <vector>

namespace {

const std::vector<std::string> kNames{"loop_counting", "loop_class_in", "reward", "in"};

}

TEST_CASE("A name the animation holds raises no problem") {
    CHECK(Document::ScriptProblems("gotoAndPlay(\"loop_counting\")\n", kNames).empty());
    CHECK(Document::ScriptProblems("stop()\n", kNames).empty());
}

TEST_CASE("A mistyped name is named, with the nearest one the animation holds") {
    const std::vector<Document::ScriptProblem> found =
        Document::ScriptProblems("deepGotoAndPlay(\"loop_countng\", 2)\n", kNames);
    REQUIRE(found.size() == 1);
    CHECK(found.front().said.find("loop_countng") != std::string::npos);
    CHECK(found.front().said.find("Did you mean \"loop_counting\"?") != std::string::npos);
    CHECK(found.front().line == 1);
    CHECK(found.front().column == 18);
}

TEST_CASE("A name nothing resembles is named without a guess") {
    const std::vector<Document::ScriptProblem> found =
        Document::ScriptProblems("gotoAndPlay(\"zzzzzzzz\")\n", kNames);
    REQUIRE(found.size() == 1);
    CHECK(found.front().said.find("zzzzzzzz") != std::string::npos);
    CHECK(found.front().said.find("Did you mean") == std::string::npos);
}

TEST_CASE("Each line of a script is counted from one") {
    const std::vector<Document::ScriptProblem> found = Document::ScriptProblems(
        "stop()\ngotoAndPlay(\"loop_counting\")\ngotoAndStop(\"nope_at_all\")\n", kNames);
    REQUIRE(found.size() == 1);
    CHECK(found.front().line == 3);
}

TEST_CASE("With no names known nothing is marked") {
    CHECK(Document::ScriptProblems("gotoAndPlay(\"anything\")\n", {}).empty());
}
