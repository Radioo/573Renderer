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

TEST_CASE("A character dropped on the timeline starts a depth on the dropped frame") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    REQUIRE(timeline != nullptr);
    REQUIRE(library != nullptr);
    std::optional<uint16_t> dot;
    for (int at = 0; at < library->topLevelItemCount(); at++) {
        if (library->topLevelItem(at)->text(0).contains("dot"))
            dot = static_cast<uint16_t>(library->topLevelItem(at)->data(0, Qt::UserRole).toUInt());
    }
    REQUIRE(dot.has_value());
    const auto shows = [&](uint16_t depth, uint32_t frame) {
        emit timeline->FrameChosen(frame);
        emit timeline->DepthChosen(depth);
        return RowValue(*opened.inspector, "Character");
    };
    const std::string dot_text = std::to_string(dot.value_or(0));
    {
        Script dropped({});
        emit timeline->CharacterDropped(dot.value_or(0), 1, uint16_t{2});
        QApplication::processEvents();
        CHECK(dropped.Problems().isEmpty());
    }
    CHECK(shows(3, 1) == dot_text);
    CHECK(shows(3, 2) == dot_text);
    CHECK(shows(3, 0) == "no Character row");
    {
        Script trimmed({});
        emit timeline->SpanTrimmed(1, 0, 0, 0);
        QApplication::processEvents();
        CHECK(trimmed.Problems().isEmpty());
    }
    {
        Script dropped({});
        emit timeline->CharacterDropped(dot.value_or(0), 2, uint16_t{1});
        QApplication::processEvents();
        CHECK(dropped.Problems().isEmpty());
    }
    CHECK(shows(1, 2) == dot_text);
    CHECK(shows(1, 0) == "7");
    CHECK(shows(4, 2) == "no Character row");
}

TEST_CASE("A new empty sprite from the library opens in the clip box ready to fill") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    auto* clips = opened.window.findChild<QComboBox*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(library != nullptr);
    REQUIRE(clips != nullptr);
    const int before = clips->count();
    library->setCurrentItem(nullptr);
    {
        Script made({Choose("New empty sprite..."), AnswerNumber(5)});
        emit library->customContextMenuRequested(QPoint(4, 4));
        REQUIRE(Settle([&made] { return made.Finished(); }));
        CHECK(made.Problems().isEmpty());
    }
    REQUIRE(clips->count() == before + 1);
    REQUIRE(clips->currentIndex() > 0);
    const int sprite = clips->currentData().toInt();
    bool listed = false;
    for (QTreeWidgetItemIterator it(library); *it != nullptr; ++it)
        listed = listed || (*it)->data(0, Qt::UserRole).toInt() == sprite;
    CHECK(listed);
    {
        Script dropped({});
        emit timeline->CharacterDropped(uint16_t{7}, 4, std::nullopt);
        QApplication::processEvents();
        CHECK(dropped.Problems().isEmpty());
    }
    emit timeline->FrameChosen(4);
    CHECK(RowValue(*opened.inspector, "Character") == "7");
    CHECK(clips->currentData().toInt() == sprite);

    library->setCurrentItem(library->topLevelItem(0));
    bool offered = false;
    {
        Script looked({Look("New empty sprite...", offered)});
        emit library->customContextMenuRequested(QPoint(4, 4));
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
    }
    CHECK(offered);
}

TEST_CASE("A marquee on the stage chooses the depths it touches") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    emit viewport->DepthsBanded({1, 2});
    CHECK(RowValue(*opened.inspector, "Depth") == "2");
    bool offered = false;
    {
        Script looked({Look("Remove 2 depths here", offered)});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
    }
    CHECK(offered);
}
