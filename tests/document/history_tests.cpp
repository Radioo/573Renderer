#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/history.h"
#include "document/keyframes.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"

#include <cstddef>
#include <cstdint>
#include <optional>
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

Document::Snapshot Of(const Document::File& file) {
    return Document::Snapshot{.file = file, .authored = {}};
}

Document::AuthoredDepth Owned(uint32_t last) {
    Document::Track track{.property = "Translation", .keys = {}};
    track.keys.push_back(Document::Keyframe{
        .frame = 0, .value = {0, 0}, .ease = Document::Ease::Hold, .bezier = {}});
    return Document::AuthoredDepth{.animation = AnimationPath(),
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = last,
                                   .tracks = {track},
                                   .script = std::nullopt,
                                   .clip = {}};
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

    history.Record("Add label one", Of(file));
    AddLabel(file, "one", 1);
    CHECK(LabelCount(file) == 2);
    CHECK(history.CanUndo());
    CHECK(history.UndoName() == "Add label one");
    CHECK_FALSE(history.CanRedo());

    auto undone = history.Undo(Of(file));
    REQUIRE(undone.has_value());
    if (!undone) return;
    file = std::move(undone->file);
    CHECK(LabelCount(file) == 1);
    CHECK_FALSE(history.CanUndo());
    CHECK(history.CanRedo());
    CHECK(history.RedoName() == "Add label one");

    auto redone = history.Redo(Of(file));
    REQUIRE(redone.has_value());
    if (!redone) return;
    file = std::move(redone->file);
    CHECK(LabelCount(file) == 2);
    CHECK(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
}

TEST_CASE("Jumping to a step undoes or redoes everything between here and there") {
    Document::History history;
    Document::File file = OpenSample();
    for (const std::string name : {"a", "b", "c"}) {
        history.Record("Add label " + name, Of(file));
        AddLabel(file, name, 1);
    }
    const std::vector<std::string> names{"Add label a", "Add label b", "Add label c"};
    CHECK(history.Names() == names);
    CHECK(history.Position() == 3);

    auto back = history.Jump(1, Of(file));
    REQUIRE(back.has_value());
    if (!back) return;
    file = std::move(back->file);
    CHECK(LabelCount(file) == 2);
    CHECK(history.Position() == 1);
    CHECK(history.Names() == names);
    CHECK(history.UndoName() == "Add label a");
    CHECK(history.RedoName() == "Add label b");

    auto forward = history.Jump(3, Of(file));
    REQUIRE(forward.has_value());
    if (!forward) return;
    file = std::move(forward->file);
    CHECK(LabelCount(file) == 4);
    CHECK(history.Position() == 3);

    auto start = history.Jump(0, Of(file));
    REQUIRE(start.has_value());
    if (!start) return;
    file = std::move(start->file);
    CHECK(LabelCount(file) == 1);
    CHECK_FALSE(history.CanUndo());

    CHECK_FALSE(history.Jump(4, Of(file)).has_value());
    CHECK(history.Position() == 0);
    CHECK(history.Names() == names);
}

TEST_CASE("An empty history undoes and redoes nothing") {
    Document::History history;
    const Document::File file = OpenSample();
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
    CHECK(history.UndoName().empty());
    CHECK(history.RedoName().empty());
    CHECK_FALSE(history.Undo(Of(file)).has_value());
    CHECK_FALSE(history.Redo(Of(file)).has_value());
}

TEST_CASE("Recording an edit drops the redo branch") {
    Document::History history;
    Document::File file = OpenSample();
    history.Record("first", Of(file));
    AddLabel(file, "one", 1);
    auto undone = history.Undo(Of(file));
    REQUIRE(undone.has_value());
    if (!undone) return;
    file = std::move(undone->file);
    REQUIRE(history.CanRedo());

    history.Record("second", Of(file));
    AddLabel(file, "two", 2);
    CHECK_FALSE(history.CanRedo());
    CHECK(history.UndoName() == "second");
}

TEST_CASE("The stack keeps only its last steps") {
    Document::History history(2);
    Document::File file = OpenSample();
    history.Record("one", Of(file));
    AddLabel(file, "one", 1);
    history.Record("two", Of(file));
    AddLabel(file, "two", 2);
    history.Record("three", Of(file));
    AddLabel(file, "three", 3);
    CHECK(LabelCount(file) == 4);

    for (int step = 0; step < 3; step++) {
        auto undone = history.Undo(Of(file));
        if (!undone) break;
        file = std::move(undone->file);
    }
    CHECK(LabelCount(file) == 2);
    CHECK_FALSE(history.CanUndo());
}

TEST_CASE("A saved document is clean until it is edited again") {
    Document::History history;
    Document::File file = OpenSample();
    CHECK(history.Saved());

    history.Record("one", Of(file));
    AddLabel(file, "one", 1);
    CHECK_FALSE(history.Saved());

    history.MarkSaved();
    CHECK(history.Saved());

    auto undone = history.Undo(Of(file));
    REQUIRE(undone.has_value());
    if (!undone) return;
    file = std::move(undone->file);
    CHECK_FALSE(history.Saved());

    auto redone = history.Redo(Of(file));
    REQUIRE(redone.has_value());
    if (!redone) return;
    file = std::move(redone->file);
    CHECK(history.Saved());
}

TEST_CASE("A saved point still reachable through the stack survives a dropped step") {
    Document::History history(1);
    Document::File file = OpenSample();
    history.Record("one", Of(file));
    AddLabel(file, "one", 1);
    history.MarkSaved();
    CHECK(history.Saved());

    history.Record("two", Of(file));
    AddLabel(file, "two", 2);
    CHECK_FALSE(history.Saved());

    auto undone = history.Undo(Of(file));
    REQUIRE(undone.has_value());
    if (!undone) return;
    file = std::move(undone->file);
    CHECK(LabelCount(file) == 2);
    CHECK(history.Saved());
}

TEST_CASE("An opened document that falls off the stack can no longer be reached") {
    Document::History history(1);
    Document::File file = OpenSample();
    CHECK(history.Saved());
    history.Record("one", Of(file));
    AddLabel(file, "one", 1);
    history.Record("two", Of(file));
    AddLabel(file, "two", 2);
    CHECK_FALSE(history.Saved());

    auto undone = history.Undo(Of(file));
    REQUIRE(undone.has_value());
    if (!undone) return;
    file = std::move(undone->file);
    CHECK(LabelCount(file) == 2);
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.Saved());
}

TEST_CASE("Clearing the history starts a clean document") {
    Document::History history;
    Document::File file = OpenSample();
    history.Record("one", Of(file));
    AddLabel(file, "one", 1);
    CHECK_FALSE(history.Saved());
    history.Clear();
    CHECK(history.Saved());
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.CanRedo());
}

TEST_CASE("Undo brings the authored content back together with the file") {
    Document::History history;
    Document::File file = OpenSample();
    std::vector<Document::AuthoredDepth> authored{Owned(2)};

    history.Record("move a keyframe", Document::Snapshot{.file = file, .authored = authored});
    AddLabel(file, "moved", 1);
    authored.front().tracks.front().keys.front().value = {500, 0};

    auto undone = history.Undo(Document::Snapshot{.file = file, .authored = authored});
    REQUIRE(undone.has_value());
    if (!undone) return;
    CHECK(LabelCount(undone->file) == 1);
    REQUIRE(undone->authored.size() == 1);
    CHECK(undone->authored.front().tracks.front().keys.front().value == std::vector<int64_t>{0, 0});

    auto redone = history.Redo(*undone);
    REQUIRE(redone.has_value());
    if (!redone) return;
    CHECK(LabelCount(redone->file) == 2);
    REQUIRE(redone->authored.size() == 1);
    CHECK(redone->authored.front().tracks.front().keys.front().value ==
          std::vector<int64_t>{500, 0});
}

TEST_CASE("A step that only changes authored content is undone like any other") {
    Document::History history;
    const Document::File file = OpenSample();
    history.Record("own depth 1", Document::Snapshot{.file = file, .authored = {}});
    const std::vector<Document::AuthoredDepth> owned{Owned(4)};

    auto undone = history.Undo(Document::Snapshot{.file = file, .authored = owned});
    REQUIRE(undone.has_value());
    if (!undone) return;
    CHECK(undone->authored.empty());
    CHECK(undone->file.Encode() == file.Encode());
    CHECK(history.RedoName() == "own depth 1");
}
