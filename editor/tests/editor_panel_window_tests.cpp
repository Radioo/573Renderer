#include <catch2/catch_test_macros.hpp>

#include <DockWidget.h>

#include "editor_files.h"
#include "editor_mime.h"
#include "editor_timeline.h"
#include "editor_viewport.h"
#include "editor_window.h"
#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "document/frame_edit.h"
#include "document/key_selection.h"
#include "document/place_image.h"
#include "document/project.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"

#include <QAction>
#include <QImage>
#include <QPixmap>
#include <QtGlobal>
#include <QApplication>
#include <QByteArray>
#include <QColorDialog>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QModelIndex>
#include <QMessageBox>
#include <QPoint>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "window_test_support.h"

using namespace WindowTest;

TEST_CASE("The library counts uses, places a character and shows a sprite on its own") {
    Opened opened;
    Open(opened, true);
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* clips = opened.window.findChild<QComboBox*>();
    REQUIRE(library != nullptr);
    REQUIRE(timeline != nullptr);
    REQUIRE(clips != nullptr);
    const auto row = [&](const QString& part) -> QTreeWidgetItem* {
        for (QTreeWidgetItemIterator it(library); *it != nullptr; ++it) {
            if ((*it)->text(0).contains(part)) return *it;
        }
        return nullptr;
    };
    REQUIRE(row("dot") != nullptr);
    CHECK(row("dot")->text(1) == "1");

    library->setCurrentItem(row("dot"));
    {
        Script placed(
            {Choose("Place on a new depth from frame 0..."), AcceptInput(), AcceptInput()});
        emit library->customContextMenuRequested(QPoint(4, 4));
        REQUIRE(Settle([&placed] { return placed.Finished(); }));
        INFO(placed.Problems().join("|").toStdString());
        CHECK(placed.Problems().isEmpty());
    }
    REQUIRE(row("dot") != nullptr);
    CHECK(row("dot")->text(1) == "2");
    CHECK(RowValue(*opened.inspector, "Depth") == "3");

    emit timeline->DepthChosen(3);
    {
        Script grouped({Choose("Group depth 3 and up here into a sprite..."), AcceptInput(),
                        AcceptInput(), AcceptInput()});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&grouped] { return grouped.Finished(); }));
        INFO(grouped.Problems().join("|").toStdString());
        CHECK(grouped.Problems().isEmpty());
    }
    QTreeWidgetItem* sprite = row("Sprite");
    REQUIRE(sprite != nullptr);
    CHECK(sprite->text(1) == "1");
    CHECK(clips->currentIndex() == 0);
    const QVariant id = sprite->data(0, Qt::UserRole);
    emit library->itemDoubleClicked(sprite, 0);
    REQUIRE(Settle([&] { return clips->currentIndex() != 0; }));
    CHECK(clips->currentData().toInt() == id.toInt());
}

TEST_CASE("The timeline names what each depth places") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const int middle = timeline->width() / 2;
    CHECK(timeline->SpanNameAt(QPoint(middle, 34)).isEmpty());
    CHECK(timeline->SpanNameAt(QPoint(middle, 50)).contains("dot"));
}

TEST_CASE("A character dragged from the library lands on a new depth where it is dropped") {
    Opened opened;
    Open(opened, true);
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(library != nullptr);
    REQUIRE(viewport != nullptr);
    int dot = -1;
    for (int at = 0; at < library->topLevelItemCount(); at++) {
        if (library->topLevelItem(at)->text(0).contains("dot")) dot = at;
    }
    REQUIRE(dot >= 0);
    const QModelIndex index = library->model()->index(dot, 0);
    const std::unique_ptr<QMimeData> data(library->model()->mimeData({index}));
    REQUIRE(data != nullptr);
    REQUIRE(data->hasFormat(Editor::kCharacterMime));
    const auto character = static_cast<uint16_t>(data->data(Editor::kCharacterMime).toUInt());
    CHECK(character == library->topLevelItem(dot)->data(0, Qt::UserRole).toUInt());

    {
        Script placed({});
        emit viewport->CharacterDropped(character, 100, 50);
        QApplication::processEvents();
        CHECK(placed.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Depth") == "3");
    CHECK(RowValue(*opened.inspector, "Translation") == "2000, 1000");
    CHECK(library->topLevelItem(dot)->text(1) == "2");

    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* clips = opened.window.findChild<QComboBox*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(clips != nullptr);
    emit timeline->DepthChosen(1);
    {
        Script grouped({Choose("Group depth 1 and up here into a sprite..."), AcceptInput(),
                        AcceptInput(), AcceptInput()});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&grouped] { return grouped.Finished(); }));
        CHECK(grouped.Problems().isEmpty());
    }
    clips->setCurrentIndex(1);
    Script refused({});
    emit viewport->CharacterDropped(character, 100, 50);
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("on its own"));
}

TEST_CASE("A sprite duplicated in the library can be put on a depth in place of the original") {
    Opened opened;
    Open(opened, true);
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* clips = opened.window.findChild<QComboBox*>();
    REQUIRE(library != nullptr);
    REQUIRE(timeline != nullptr);
    REQUIRE(clips != nullptr);
    emit timeline->DepthChosen(2);
    {
        Script grouped({Choose("Group depth 2 and up here into a sprite..."), AcceptInput(),
                        AcceptInput(), AcceptInput()});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&grouped] { return grouped.Finished(); }));
        CHECK(grouped.Problems().isEmpty());
    }
    const auto sprites = [&] {
        std::vector<QTreeWidgetItem*> found;
        for (int at = 0; at < library->topLevelItemCount(); at++) {
            if (library->topLevelItem(at)->text(0).startsWith("Sprite"))
                found.push_back(library->topLevelItem(at));
        }
        return found;
    };
    REQUIRE(sprites().size() == 1);
    const QString original = sprites()[0]->data(0, Qt::UserRole).toString();
    const int clip_count = clips->count();
    library->setCurrentItem(sprites()[0]);
    {
        Script duplicated({Choose("Duplicate this sprite")});
        emit library->customContextMenuRequested(QPoint(4, 4));
        REQUIRE(Settle([&duplicated] { return duplicated.Finished(); }));
        CHECK(duplicated.Problems().isEmpty());
    }
    REQUIRE(sprites().size() == 2);
    CHECK(clips->count() == clip_count + 1);
    QTreeWidgetItem* copy =
        sprites()[0]->data(0, Qt::UserRole).toString() == original ? sprites()[1] : sprites()[0];
    const QString copied = copy->data(0, Qt::UserRole).toString();
    CHECK(copy->text(1) == "0");

    emit timeline->DepthChosen(2);
    library->setCurrentItem(copy);
    {
        Script used({Choose("Use on depth 2 from frame 0")});
        emit library->customContextMenuRequested(QPoint(4, 4));
        REQUIRE(Settle([&used] { return used.Finished(); }));
        INFO(used.Problems().join("|").toStdString());
        CHECK(used.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Character") == copied.toStdString());
    for (QTreeWidgetItem* one : sprites()) {
        const bool is_copy = one->data(0, Qt::UserRole).toString() == copied;
        CHECK(one->text(1) == (is_copy ? "1" : "0"));
    }
}

TEST_CASE("Clicking a step in the history panel undoes or redoes up to it") {
    Opened opened;
    Open(opened);
    auto* history = opened.window.findChild<QListWidget*>("history");
    REQUIRE(history != nullptr);
    REQUIRE(history->count() == 1);
    CHECK(history->item(0)->text() == "Start");
    const auto set_rate = [&](const QString& rate) {
        Script script({});
        QTableWidgetItem* cell = ValueCell(*opened.inspector, "Frame rate");
        REQUIRE(cell != nullptr);
        cell->setText(rate);
        QApplication::processEvents();
        CHECK(script.Problems().isEmpty());
    };
    set_rate("30");
    set_rate("24");
    REQUIRE(history->count() == 3);
    CHECK(history->currentRow() == 2);
    CHECK(RowValue(*opened.inspector, "Frame rate") == "24");

    emit history->itemClicked(history->item(0));
    REQUIRE(Settle([&] { return RowValue(*opened.inspector, "Frame rate") == "60"; }));
    REQUIRE(history->count() == 3);
    CHECK(history->currentRow() == 0);
    CHECK(history->item(2)->foreground() != history->item(0)->foreground());

    emit history->itemClicked(history->item(2));
    REQUIRE(Settle([&] { return RowValue(*opened.inspector, "Frame rate") == "24"; }));
    CHECK(history->currentRow() == 2);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(RowValue(*opened.inspector, "Frame rate") == "30");
    CHECK(history->currentRow() == 1);
}

TEST_CASE("Depths chosen together move together, owned or not, as one undo step") {
    Opened opened;
    Open(opened, true);
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(library != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(timeline != nullptr);
    std::optional<uint16_t> dot;
    for (int at = 0; at < library->topLevelItemCount(); at++) {
        if (library->topLevelItem(at)->text(0).contains("dot"))
            dot = static_cast<uint16_t>(library->topLevelItem(at)->data(0, Qt::UserRole).toUInt());
    }
    REQUIRE(dot.has_value());
    if (!dot) return;
    {
        Script placed({});
        emit viewport->CharacterDropped(*dot, 500, 300);
        QApplication::processEvents();
        CHECK(placed.Problems().isEmpty());
    }
    QAction* project = nullptr;
    for (QAction* action : opened.window.findChildren<QAction*>()) {
        if (action->text() == "&New project...") project = action;
    }
    REQUIRE(project != nullptr);
    const QString folder = opened.dir.filePath("project");
    REQUIRE(QDir().mkpath(folder));
    {
        Script made({PickFile(folder)});
        project->trigger();
        REQUIRE(Settle([&made] { return made.Finished(); }));
        CHECK(made.Problems().isEmpty());
    }
    emit timeline->DepthChosen(3);
    {
        Script owned({Choose("Let the project own depth 3 from here")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&owned] { return owned.Finished(); }));
        INFO(owned.Problems().join("|").toStdString());
        CHECK(owned.Problems().isEmpty());
    }
    const auto x_of = [&](uint16_t depth) {
        emit timeline->DepthChosen(depth);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    const int baked_before = x_of(2);
    CHECK(x_of(3) == 10000);

    emit timeline->DepthsChosen({2, 3});
    {
        Script moved({});
        emit viewport->Picked(501, 301);
        emit viewport->Dragged(2, 10, 0, true);
        QApplication::processEvents();
        INFO(moved.Problems().join("|").toStdString());
        CHECK(moved.Problems().isEmpty());
    }
    CHECK(x_of(3) == 10200);
    CHECK(x_of(2) == baked_before + 200);

    emit timeline->DepthChosen(3);
    {
        Script nudged({});
        emit viewport->Dragged(3, 1, 0, true);
        QApplication::processEvents();
        CHECK(nudged.Problems().isEmpty());
    }
    CHECK(x_of(3) == 10220);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    undo->trigger();
    CHECK(x_of(3) == 10000);
    CHECK(x_of(2) == baked_before);
}

TEST_CASE("The Align menu lines chosen depths up as one undo step") {
    Opened opened;
    Open(opened, true);
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(library != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(timeline != nullptr);
    std::optional<uint16_t> dot;
    for (int at = 0; at < library->topLevelItemCount(); at++) {
        if (library->topLevelItem(at)->text(0).contains("dot"))
            dot = static_cast<uint16_t>(library->topLevelItem(at)->data(0, Qt::UserRole).toUInt());
    }
    REQUIRE(dot.has_value());
    if (!dot) return;
    {
        Script placed({});
        emit viewport->CharacterDropped(*dot, 500, 300);
        QApplication::processEvents();
        CHECK(placed.Problems().isEmpty());
    }
    const auto action = [&](const QString& text) {
        for (QAction* one : opened.window.findChildren<QAction*>()) {
            if (one->text() == text) return one;
        }
        return static_cast<QAction*>(nullptr);
    };
    QAction* left = action("&Left edges");
    QAction* spread = action("Spread centres &across");
    REQUIRE(left != nullptr);
    REQUIRE(spread != nullptr);
    const auto x_of = [&](uint16_t depth) {
        emit timeline->DepthChosen(depth);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    const int first = x_of(2);
    CHECK(x_of(3) == 10000);
    REQUIRE(first != 10000);

    emit timeline->DepthChosen(3);
    {
        Script refused({});
        left->trigger();
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        CHECK(refused.Problems().front().contains("at least 2"));
    }
    emit timeline->DepthsChosen({2, 3});
    {
        Script refused({});
        spread->trigger();
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        CHECK(refused.Problems().front().contains("at least 3"));
    }
    emit timeline->DepthsChosen({2, 3});
    {
        Script aligned({});
        left->trigger();
        QApplication::processEvents();
        CHECK(aligned.Problems().isEmpty());
    }
    CHECK(x_of(3) == x_of(2));
    CHECK(x_of(2) == first);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(x_of(3) == 10000);
    CHECK(x_of(2) == first);

    QAction* centres = action("&Horizontal centres");
    REQUIRE(centres != nullptr);
    emit timeline->DepthsChosen({2, 3});
    {
        Script aligned({});
        centres->trigger();
        QApplication::processEvents();
        CHECK(aligned.Problems().isEmpty());
    }
    CHECK(x_of(3) == x_of(2));
    CHECK(x_of(2) != first);
    CHECK(x_of(3) != 10000);
}

TEST_CASE("Delete removes the chosen depths here as one undo step, but not owned ones") {
    Opened opened;
    Open(opened, true);
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(library != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(timeline != nullptr);
    const auto dot_row = [&]() -> QTreeWidgetItem* {
        for (int at = 0; at < library->topLevelItemCount(); at++) {
            if (library->topLevelItem(at)->text(0).contains("dot"))
                return library->topLevelItem(at);
        }
        return nullptr;
    };
    REQUIRE(dot_row() != nullptr);
    const auto dot = static_cast<uint16_t>(dot_row()->data(0, Qt::UserRole).toUInt());
    {
        Script placed({});
        emit viewport->CharacterDropped(dot, 500, 300);
        QApplication::processEvents();
        CHECK(placed.Problems().isEmpty());
    }
    CHECK(dot_row()->text(1) == "2");
    emit timeline->DepthsChosen({2, 3});
    {
        bool offered = false;
        Script looked({Look("Remove 2 depths here", offered)});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
        CHECK(offered);
    }
    QAction* remove = ShortcutAction(opened.window, QKeySequence(QKeySequence::Delete));
    REQUIRE(remove != nullptr);
    {
        Script removed({});
        remove->trigger();
        QApplication::processEvents();
        CHECK(removed.Problems().isEmpty());
    }
    CHECK(dot_row()->text(1) == "0");
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(dot_row()->text(1) == "2");

    QAction* project = nullptr;
    for (QAction* action : opened.window.findChildren<QAction*>()) {
        if (action->text() == "&New project...") project = action;
    }
    REQUIRE(project != nullptr);
    const QString folder = opened.dir.filePath("project");
    REQUIRE(QDir().mkpath(folder));
    {
        Script made({PickFile(folder)});
        project->trigger();
        REQUIRE(Settle([&made] { return made.Finished(); }));
        CHECK(made.Problems().isEmpty());
    }
    emit timeline->DepthChosen(3);
    {
        Script owned({Choose("Let the project own depth 3 from here")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&owned] { return owned.Finished(); }));
        CHECK(owned.Problems().isEmpty());
    }
    emit timeline->DepthsChosen({2, 3});
    Script refused({});
    remove->trigger();
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("owns depth 3"));
    CHECK(dot_row()->text(1) == "2");
}

TEST_CASE("Arranging swaps depths in the stacking order and carries what the project owns") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const QString folder = OwnDroppedDot(opened);
    const auto x_of = [&](uint16_t depth) {
        emit timeline->DepthChosen(depth);
        const QString text = QString::fromStdString(RowValue(*opened.inspector, "Translation"));
        return text.section(',', 0, 0).trimmed().toInt();
    };
    const auto owns = [&](uint16_t depth) {
        emit timeline->DepthChosen(depth);
        bool offered = false;
        Script looked({Look(QString("Detach depth %1 back to baked data").arg(depth), offered)});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
        return offered;
    };
    const auto saved_depth = [&folder]() -> std::optional<uint16_t> {
        const auto project = Document::ReadProject(Editor::ReadFileBytes(
            QString::fromStdString(Document::ProjectManifestPath(folder.toStdString()))));
        if (!project || project->content.size() != 1) return std::nullopt;
        return project->content.front().depth;
    };
    const int baked = x_of(2);
    CHECK(x_of(3) == 10000);
    CHECK(owns(3));
    CHECK(saved_depth() == uint16_t{3});

    QAction* backward = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    QAction* front =
        ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
    QAction* forward = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    REQUIRE(backward != nullptr);
    REQUIRE(front != nullptr);
    REQUIRE(forward != nullptr);
    emit timeline->DepthChosen(3);
    {
        Script sent({});
        backward->trigger();
        QApplication::processEvents();
        CHECK(sent.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Depth") == "2");
    CHECK(x_of(2) == 10000);
    CHECK(x_of(3) == baked);
    CHECK(owns(2));
    CHECK_FALSE(owns(3));
    CHECK(saved_depth() == uint16_t{2});

    emit timeline->DepthChosen(1);
    {
        Script brought({});
        front->trigger();
        QApplication::processEvents();
        CHECK(brought.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Depth") == "3");
    CHECK(RowValue(*opened.inspector, "Character") == "7");
    CHECK(x_of(1) == 10000);
    CHECK(x_of(2) == baked);
    CHECK(owns(1));

    emit timeline->DepthChosen(3);
    {
        Script refused({});
        forward->trigger();
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        CHECK(refused.Problems().front().contains("already at the front"));
    }

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    undo->trigger();
    CHECK(x_of(3) == 10000);
    CHECK(x_of(2) == baked);
    CHECK(owns(3));
}

TEST_CASE("Splitting a depth at the playhead keeps what it shows and refuses what it cannot") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    QAction* split = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(split != nullptr);
    REQUIRE(undo != nullptr);
    const auto choose = [&](uint32_t frame, uint16_t depth) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(depth);
    };
    const auto translation_on = [&](uint32_t frame) {
        choose(frame, 2);
        return RowValue(*opened.inspector, "Translation");
    };
    const auto refusal = [&] {
        Script refused({});
        split->trigger();
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        return refused.Problems().front();
    };
    const std::string at_one = translation_on(1);
    const std::string at_two = translation_on(2);
    choose(1, 2);
    {
        Script done({});
        split->trigger();
        QApplication::processEvents();
        CHECK(done.Problems().isEmpty());
    }
    CHECK(translation_on(1) == at_one);
    CHECK(translation_on(2) == at_two);
    choose(1, 2);
    CHECK(refusal().contains("starts on frame 1"));
    choose(1, 1);
    CHECK(refusal().contains("start again"));
    undo->trigger();
    choose(1, 2);
    {
        Script again({Choose("Split depth 2 at frame 1")});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&again] { return again.Finished(); }));
        CHECK(again.Problems().isEmpty());
    }
    CHECK(refusal().contains("starts on frame 1"));
    undo->trigger();

    OwnDroppedDot(opened);
    choose(1, 3);
    CHECK(refusal().contains("owns depth 3"));
}

TEST_CASE("Trimming the clip to the work area keeps what those frames showed") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    QAction* trim = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X));
    QAction* start = ShortcutAction(opened.window, QKeySequence(Qt::Key_B));
    QAction* end = ShortcutAction(opened.window, QKeySequence(Qt::Key_N));
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(trim != nullptr);
    REQUIRE(start != nullptr);
    REQUIRE(end != nullptr);
    REQUIRE(undo != nullptr);
    const auto translation_on = [&](uint32_t frame) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(2);
        return RowValue(*opened.inspector, "Translation");
    };
    const auto mark = [&](uint32_t first, uint32_t last) {
        emit timeline->FrameChosen(first);
        start->trigger();
        emit timeline->FrameChosen(last);
        end->trigger();
    };
    const auto refusal = [&] {
        Script refused({});
        trim->trigger();
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        return refused.Problems().front();
    };
    CHECK(refusal().contains("work area first"));
    const std::string at_one = translation_on(1);
    const std::string at_two = translation_on(2);
    REQUIRE(at_one != at_two);
    mark(1, 2);
    emit timeline->FrameChosen(1);
    {
        Script trimmed({});
        trim->trigger();
        QApplication::processEvents();
        CHECK(trimmed.Problems().isEmpty());
    }
    CHECK(opened.window.statusBar()->currentMessage().contains("starts again"));
    CHECK(RowValue(*opened.inspector, "Translation") == at_one);
    CHECK(refusal().contains("work area first"));
    CHECK(translation_on(0) == at_one);
    CHECK(translation_on(1) == at_two);
    mark(0, 1);
    CHECK(refusal().contains("exactly those frames"));
    undo->trigger();
    CHECK(translation_on(2) == at_two);

    mark(0, 1);
    opened.window.statusBar()->clearMessage();
    {
        Script trimmed({});
        trim->trigger();
        QApplication::processEvents();
        CHECK(trimmed.Problems().isEmpty());
    }
    CHECK_FALSE(opened.window.statusBar()->currentMessage().contains("starts again"));
    undo->trigger();

    OwnDroppedDot(opened);
    mark(1, 2);
    CHECK(refusal().contains("owns depths"));
}

TEST_CASE("Chosen depths are sequenced one after another, owned records moving with them") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    for (int inserted = 0; inserted < 3; inserted++) {
        Script more({Choose("Insert a frame at 2")});
        emit timeline->MenuRequested(QPoint(4, 4), 2, QString());
        REQUIRE(Settle([&more] { return more.Finished(); }));
        CHECK(more.Problems().isEmpty());
    }
    const auto shows = [&](uint16_t depth, uint32_t frame) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(depth);
        return RowValue(*opened.inspector, "Character") != "no Character row";
    };
    const auto sequence = [&](uint32_t frame, const QString& entry) {
        Script sequenced({Choose(entry)});
        emit timeline->MenuRequested(QPoint(4, 4), frame, QString());
        REQUIRE(Settle([&sequenced] { return sequenced.Finished(); }));
        return sequenced.Problems();
    };
    {
        Script trimmed({});
        emit timeline->SpanTrimmed(1, 0, 0, 1);
        emit timeline->SpanTrimmed(2, 0, 0, 2);
        QApplication::processEvents();
        CHECK(trimmed.Problems().isEmpty());
    }
    CHECK(shows(2, 0));
    CHECK_FALSE(shows(2, 5));
    emit timeline->FrameChosen(0);
    emit timeline->DepthsChosen({1, 2});
    CHECK(sequence(0, "Sequence 2 depths one after another").isEmpty());
    CHECK(shows(1, 1));
    CHECK_FALSE(shows(2, 0));
    CHECK_FALSE(shows(2, 1));
    CHECK(shows(2, 2));
    CHECK(shows(2, 4));
    CHECK_FALSE(shows(2, 5));
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(shows(2, 0));

    const QString folder = OwnDroppedDot(opened);
    {
        Script trimmed({});
        emit timeline->SpanTrimmed(3, 0, 0, 1);
        QApplication::processEvents();
        CHECK(trimmed.Problems().isEmpty());
    }
    emit timeline->FrameChosen(0);
    emit timeline->DepthsChosen({2, 3});
    CHECK(sequence(0, "Sequence 2 depths one after another").isEmpty());
    CHECK(shows(3, 3));
    emit timeline->DepthChosen(3);
    bool owned_there = false;
    {
        Script looked({Look("Detach depth 3 back to baked data", owned_there)});
        emit timeline->MenuRequested(QPoint(4, 4), 3, QString());
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
    }
    CHECK(owned_there);
    const auto project = Document::ReadProject(Editor::ReadFileBytes(
        QString::fromStdString(Document::ProjectManifestPath(folder.toStdString()))));
    REQUIRE(project.has_value());
    REQUIRE(project->content.size() == 1);
    CHECK(project->content.front().first_frame == 3);
}

TEST_CASE("The package and library searches hide what does not match, across refills") {
    Opened opened;
    Open(opened, true);
    auto* package_filter = opened.window.findChild<QLineEdit*>("package_filter");
    auto* library_filter = opened.window.findChild<QLineEdit*>("library_filter");
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(package_filter != nullptr);
    REQUIRE(library_filter != nullptr);
    REQUIRE(library != nullptr);
    REQUIRE(viewport != nullptr);
    const auto visible = [](QTreeWidget& tree, const QString& text) {
        for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it) {
            if ((*it)->text(0) == text) return !(*it)->isHidden();
        }
        return false;
    };
    CHECK(visible(*opened.tree, "intro"));
    CHECK(visible(*opened.tree, "dot"));
    package_filter->setText("DOT");
    CHECK_FALSE(visible(*opened.tree, "intro"));
    CHECK(visible(*opened.tree, "dot"));
    package_filter->clear();
    CHECK(visible(*opened.tree, "intro"));

    REQUIRE(library->topLevelItemCount() > 0);
    QTreeWidgetItem* first = library->topLevelItem(0);
    const auto character = static_cast<uint16_t>(first->data(0, Qt::UserRole).toUInt());
    library_filter->setText("no such character");
    package_filter->setText("INTRO");
    CHECK(library->topLevelItem(0)->isHidden());
    {
        Script placed({});
        emit viewport->CharacterDropped(character, 100, 50);
        QApplication::processEvents();
        CHECK(placed.Problems().isEmpty());
    }
    REQUIRE(library->topLevelItemCount() > 0);
    CHECK(library->topLevelItem(0)->isHidden());
    opened.tree->setCurrentItem(AnimationNamed(*opened.tree, "intro"));
    {
        Script duplicated({Choose("Duplicate intro..."), AcceptInput()});
        RunMenu(duplicated, *opened.tree);
        CHECK(duplicated.Problems().isEmpty());
    }
    REQUIRE(AnimationNamed(*opened.tree, "intro_copy") != nullptr);
    CHECK(visible(*opened.tree, "intro_copy"));
    CHECK_FALSE(visible(*opened.tree, "dot"));
    library_filter->clear();
    CHECK_FALSE(library->topLevelItem(0)->isHidden());
}

TEST_CASE("A closed panel comes back from the View menu") {
    Opened opened;
    Open(opened);
    ads::CDockWidget* library = nullptr;
    for (ads::CDockWidget* dock : opened.window.findChildren<ads::CDockWidget*>()) {
        if (dock->windowTitle() == "Library") library = dock;
    }
    REQUIRE(library != nullptr);
    QAction* toggle = nullptr;
    for (const QMenu* menu : opened.window.findChildren<QMenu*>()) {
        if (menu->title() != "&Panels") continue;
        for (QAction* action : menu->actions()) {
            if (action->text() == "Library") toggle = action;
        }
    }
    REQUIRE(toggle != nullptr);
    bool graph_listed = false;
    for (const QMenu* menu : opened.window.findChildren<QMenu*>()) {
        if (menu->title() != "&Panels") continue;
        for (const QAction* action : menu->actions())
            graph_listed = graph_listed || action->text() == "Graph";
    }
    CHECK(graph_listed);
    CHECK_FALSE(library->isClosed());
    library->closeDockWidget();
    CHECK(library->isClosed());
    CHECK_FALSE(toggle->isChecked());
    toggle->trigger();
    CHECK_FALSE(library->isClosed());
}
