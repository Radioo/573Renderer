#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/document.h"
#include "document/history.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using SamplePackage::HashPath;
using SamplePackage::SampleArchive;

std::vector<uint8_t> SampleBytes() {
    const auto bytes = Ifs::Write(SampleArchive());
    const std::string error = bytes.has_value() ? std::string() : bytes.error();
    INFO(error);
    REQUIRE(bytes.has_value());
    return *bytes;
}

std::string AnimationPath() {
    return "afp/" + HashPath("intro");
}

Document::File OpenSample() {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    return std::move(*file);
}

void AddLabel(Document::File& file, const std::string& name, uint16_t frame) {
    auto animation = file.ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    animation->strings.push_back(name);
    animation->root.labels.push_back(AfpAnimation::Label{
        .frame = frame, .name = static_cast<uint32_t>(animation->strings.size() - 1)});
    REQUIRE(file.WriteAnimation(AnimationPath(), *animation).has_value());
}

std::size_t LabelCount(const Document::File& file) {
    const auto details = file.Describe(AnimationPath());
    REQUIRE(details.has_value());
    return details->animation.value_or(Document::AnimationDetails{}).labels.size();
}

}

TEST_CASE("Undo and redo walk the document back and forward") {
    Document::History history;
    Document::File file = OpenSample();
    REQUIRE(LabelCount(file) == 1);

    history.Record("Add label one", file);
    AddLabel(file, "one", 1);
    CHECK(LabelCount(file) == 2);
    CHECK(history.CanUndo());
    CHECK(history.UndoName() == "Add label one");
    CHECK_FALSE(history.CanRedo());

    auto undone = history.Undo(file);
    REQUIRE(undone.has_value());
    file = std::move(*undone);
    CHECK(LabelCount(file) == 1);
    CHECK_FALSE(history.CanUndo());
    CHECK(history.CanRedo());
    CHECK(history.RedoName() == "Add label one");

    auto redone = history.Redo(file);
    REQUIRE(redone.has_value());
    file = std::move(*redone);
    CHECK(LabelCount(file) == 2);
    CHECK(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
}

TEST_CASE("An empty history undoes and redoes nothing") {
    Document::History history;
    const Document::File file = OpenSample();
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
    CHECK(history.UndoName().empty());
    CHECK(history.RedoName().empty());
    CHECK_FALSE(history.Undo(file).has_value());
    CHECK_FALSE(history.Redo(file).has_value());
}

TEST_CASE("Recording an edit drops the redo branch") {
    Document::History history;
    Document::File file = OpenSample();
    history.Record("first", file);
    AddLabel(file, "one", 1);
    auto undone = history.Undo(file);
    REQUIRE(undone.has_value());
    file = std::move(*undone);
    REQUIRE(history.CanRedo());

    history.Record("second", file);
    AddLabel(file, "two", 2);
    CHECK_FALSE(history.CanRedo());
    CHECK(history.UndoName() == "second");
}

TEST_CASE("The stack keeps only its last steps") {
    Document::History history(2);
    Document::File file = OpenSample();
    history.Record("one", file);
    AddLabel(file, "one", 1);
    history.Record("two", file);
    AddLabel(file, "two", 2);
    history.Record("three", file);
    AddLabel(file, "three", 3);
    CHECK(LabelCount(file) == 4);

    for (int step = 0; step < 3; step++) {
        auto undone = history.Undo(file);
        if (!undone) break;
        file = std::move(*undone);
    }
    CHECK(LabelCount(file) == 2);
    CHECK_FALSE(history.CanUndo());
}

TEST_CASE("A saved document is clean until it is edited again") {
    Document::History history;
    Document::File file = OpenSample();
    CHECK(history.Saved());

    history.Record("one", file);
    AddLabel(file, "one", 1);
    CHECK_FALSE(history.Saved());

    history.MarkSaved();
    CHECK(history.Saved());

    auto undone = history.Undo(file);
    REQUIRE(undone.has_value());
    file = std::move(*undone);
    CHECK_FALSE(history.Saved());

    auto redone = history.Redo(file);
    REQUIRE(redone.has_value());
    file = std::move(*redone);
    CHECK(history.Saved());
}

TEST_CASE("A saved point still reachable through the stack survives a dropped step") {
    Document::History history(1);
    Document::File file = OpenSample();
    history.Record("one", file);
    AddLabel(file, "one", 1);
    history.MarkSaved();
    CHECK(history.Saved());

    history.Record("two", file);
    AddLabel(file, "two", 2);
    CHECK_FALSE(history.Saved());

    auto undone = history.Undo(file);
    REQUIRE(undone.has_value());
    file = std::move(*undone);
    CHECK(LabelCount(file) == 2);
    CHECK(history.Saved());
}

TEST_CASE("An opened document that falls off the stack can no longer be reached") {
    Document::History history(1);
    Document::File file = OpenSample();
    CHECK(history.Saved());
    history.Record("one", file);
    AddLabel(file, "one", 1);
    history.Record("two", file);
    AddLabel(file, "two", 2);
    CHECK_FALSE(history.Saved());

    auto undone = history.Undo(file);
    REQUIRE(undone.has_value());
    file = std::move(*undone);
    CHECK(LabelCount(file) == 2);
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.Saved());
}

TEST_CASE("Clearing the history starts a clean document") {
    Document::History history;
    Document::File file = OpenSample();
    history.Record("one", file);
    AddLabel(file, "one", 1);
    CHECK_FALSE(history.Saved());
    history.Clear();
    CHECK(history.Saved());
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
}
