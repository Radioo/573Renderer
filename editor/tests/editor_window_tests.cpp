#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "editor_timeline.h"
#include "editor_viewport.h"
#include "editor_window.h"
#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "document/frame_edit.h"
#include "document/place_image.h"
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
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QSettings>
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
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "window_test_support.h"

using namespace WindowTest;

TEST_CASE("An animation opens for editing without a game install") {
    Opened opened;
    Open(opened);
    QTreeWidgetItem* intro = AnimationNamed(*opened.tree, "intro");
    REQUIRE(intro != nullptr);
    CHECK(opened.tree->currentItem() == intro);
    CHECK(RowValue(*opened.inspector, "Frame rate") == "60");
    CHECK(RowValue(*opened.inspector, "Stage size") == "1920, 1080");
}

TEST_CASE("An animation setting edited in the inspector can be undone") {
    Opened opened;
    Open(opened);
    QTableWidgetItem* rate = ValueCell(*opened.inspector, "Frame rate");
    REQUIRE(rate != nullptr);
    Script script({});
    rate->setText("30");
    QApplication::processEvents();
    CHECK(script.Problems().isEmpty());
    CHECK(RowValue(*opened.inspector, "Frame rate") == "30");

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    CHECK(RowValue(*opened.inspector, "Frame rate") == "60");
}

TEST_CASE("A new animation made from the package menu opens, and removing it closes it") {
    Opened opened;
    Open(opened);
    Script added({Choose("New animation..."), Answer("fresh"), AnswerNumber(12)});
    RunMenu(added, *opened.tree);
    CHECK(added.Problems().isEmpty());
    QTreeWidgetItem* fresh = AnimationNamed(*opened.tree, "fresh");
    REQUIRE(fresh != nullptr);
    CHECK(opened.tree->currentItem() == fresh);
    CHECK(RowValue(*opened.inspector, "Frame rate") == "60");

    Script removed({Choose("Remove fresh")});
    RunMenu(removed, *opened.tree);
    CHECK(removed.Problems().isEmpty());
    CHECK(AnimationNamed(*opened.tree, "fresh") == nullptr);
}

TEST_CASE("PNG and JPEG images can be added from the package menu") {
    Opened opened;
    Open(opened);
    const auto add = [&](const QString& path) {
        Script added({Choose("Add an image from a file..."), PickFile(path)});
        RunMenu(added, *opened.tree);
        INFO(added.Problems().join("|").toStdString());
        CHECK(added.Problems().isEmpty());
    };
    const QString png = opened.dir.filePath("leaf.png");
    QFile written(png);
    REQUIRE(written.open(QIODevice::WriteOnly));
    written.write(QByteArray(reinterpret_cast<const char*>(kTinyPng.data()),
                             static_cast<qsizetype>(kTinyPng.size())));
    written.close();
    add(png);
    const QString jpeg = opened.dir.filePath("stone.jpg");
    QImage picture(3, 2, QImage::Format_ARGB32);
    picture.fill(QColor(20, 200, 90));
    REQUIRE(picture.save(jpeg, "JPEG"));
    add(jpeg);
    const auto listed = [&](const QString& name) {
        for (QTreeWidgetItemIterator it(opened.tree); *it != nullptr; ++it) {
            if ((*it)->text(0) == name) return true;
        }
        return false;
    };
    CHECK(listed("leaf"));
    CHECK(listed("stone"));
}

TEST_CASE("An image saved from the package menu has the pixels it was added with") {
    Opened opened;
    Open(opened);
    QImage picture(3, 2, QImage::Format_ARGB32);
    const std::array<QColor, 6> colours{QColor(255, 0, 0),       QColor(0, 255, 0),
                                        QColor(0, 0, 255, 128),  QColor(10, 20, 30, 40),
                                        QColor(200, 100, 50, 0), QColor(255, 255, 255)};
    for (int at = 0; at < 6; at++)
        picture.setPixelColor(at % 3, at / 3, colours.at(static_cast<std::size_t>(at)));
    const QString source = opened.dir.filePath("tint.png");
    REQUIRE(picture.save(source, "PNG"));
    {
        Script added({Choose("Add an image from a file..."), PickFile(source)});
        RunMenu(added, *opened.tree);
        CHECK(added.Problems().isEmpty());
    }
    QTreeWidgetItem* tint = nullptr;
    for (QTreeWidgetItemIterator it(opened.tree); *it != nullptr; ++it) {
        if ((*it)->text(0) == "tint") tint = *it;
    }
    REQUIRE(tint != nullptr);
    opened.tree->setCurrentItem(tint);
    const QString saved = opened.dir.filePath("saved.png");
    {
        Script saving({Choose("Save tint as PNG..."), PickFile(saved)});
        RunMenu(saving, *opened.tree);
        INFO(saving.Problems().join("|").toStdString());
        CHECK(saving.Problems().isEmpty());
    }
    const QImage read(saved);
    REQUIRE_FALSE(read.isNull());
    CHECK(read.convertToFormat(QImage::Format_ARGB32) == picture);
}

TEST_CASE("An animation renamed from the package menu stays open under its new name") {
    Opened opened;
    Open(opened);
    REQUIRE(AnimationNamed(*opened.tree, "intro") != nullptr);
    opened.tree->setCurrentItem(AnimationNamed(*opened.tree, "intro"));
    {
        Script renamed({Choose("Rename intro..."), Answer("opening")});
        RunMenu(renamed, *opened.tree);
        INFO(renamed.Problems().join("|").toStdString());
        CHECK(renamed.Problems().isEmpty());
    }
    CHECK(AnimationNamed(*opened.tree, "intro") == nullptr);
    QTreeWidgetItem* opening = AnimationNamed(*opened.tree, "opening");
    REQUIRE(opening != nullptr);
    CHECK(opened.tree->currentItem() == opening);
    CHECK(RowValue(*opened.inspector, "Frame rate") == "60");

    Script refused({Choose("Rename opening..."), Answer("a/b")});
    RunMenu(refused, *opened.tree);
    REQUIRE_FALSE(refused.Problems().isEmpty());
    CHECK(refused.Problems().front().contains("slashes"));
    CHECK(AnimationNamed(*opened.tree, "opening") != nullptr);
}

TEST_CASE("A package with no animation takes its first from another IFS") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString like = WritePackage(dir);
    Editor::Window window;
    window.OpenDocument(WriteImagesOnly(dir));
    auto* tree = window.findChild<QTreeWidget*>();
    auto* inspector = window.findChild<QTableWidget*>();
    REQUIRE(tree != nullptr);
    REQUIRE(inspector != nullptr);
    CHECK(AnimationNamed(*tree, "intro") == nullptr);
    Script added({Choose("New animation..."), PickFile(like), Answer("intro"), Answer("first"),
                  AnswerNumber(8)});
    RunMenu(added, *tree);
    CHECK(added.Problems().isEmpty());
    QTreeWidgetItem* first = AnimationNamed(*tree, "first");
    REQUIRE(first != nullptr);
    CHECK(tree->currentItem() == first);
    CHECK(RowValue(*inspector, "Frame rate") == "60");
}

TEST_CASE("A span duplicated from the timeline menu lands on the next free depth") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->DepthChosen(1);
    CHECK(RowValue(*opened.inspector, "Depth") == "1");
    {
        Script duplicated({Choose("Duplicate depth 1 here onto another depth..."), AcceptNumber()});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&duplicated] { return duplicated.Finished(); }));
        CHECK(duplicated.Problems().isEmpty());
        CHECK(RowValue(*opened.inspector, "Depth") == "2");
    }

    emit timeline->DepthChosen(1);
    Script refused({Choose("Duplicate depth 1 here onto another depth..."), AnswerNumber(2)});
    emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("depth 2"));
}

TEST_CASE("Depths grouped from the timeline menu become a sprite, and ungrouping undoes it") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* clips = opened.window.findChild<QComboBox*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(clips != nullptr);
    const int before = clips->count();
    emit timeline->DepthChosen(1);
    {
        Script grouped({Choose("Group depth 1 and up here into a sprite..."), AcceptNumber(),
                        AcceptNumber(), AcceptNumber()});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&grouped] { return grouped.Finished(); }));
        CHECK(grouped.Problems().isEmpty());
        CHECK(clips->count() == before + 1);
        CHECK(clips->currentIndex() == 0);
        CHECK(RowValue(*opened.inspector, "Depth") == "1");
    }
    {
        Script ungrouped({Choose("Ungroup the sprite on depth 1 here")});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&ungrouped] { return ungrouped.Finished(); }));
        CHECK(ungrouped.Problems().isEmpty());
        CHECK(clips->count() == before);
    }
    {
        Script regrouped({Choose("Group depth 1 and up here into a sprite..."), AcceptNumber(),
                          AcceptNumber(), AcceptNumber()});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&regrouped] { return regrouped.Finished(); }));
        CHECK(regrouped.Problems().isEmpty());
    }
    clips->setCurrentIndex(1);
    {
        Script named({Choose("Name the export of this sprite..."), Answer("banner")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&named] { return named.Finished(); }));
        CHECK(named.Problems().isEmpty());
        CHECK(clips->currentIndex() == 1);
        CHECK(clips->currentText().startsWith("banner"));
    }
    {
        Script clash({Choose("Name the export of this sprite..."), Answer("intro")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&clash] { return !clash.Problems().isEmpty(); }));
        CHECK(clips->currentText().startsWith("banner"));
    }
    clips->setCurrentIndex(0);
    emit timeline->DepthChosen(1);
    {
        Script ungroup_again({Choose("Ungroup the sprite on depth 1 here")});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&ungroup_again] { return ungroup_again.Finished(); }));
        CHECK(ungroup_again.Problems().isEmpty());
    }
    Script refused({Choose("Ungroup the sprite on depth 1 here")});
    emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("sprite"));
}

TEST_CASE("A depth copied from the timeline pastes onto a free depth at the playhead") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->DepthChosen(1);
    {
        Script copied({Choose("Copy depth 1 here")});
        emit timeline->MenuRequested(QPoint(4, 4), 1, QString());
        REQUIRE(Settle([&copied] { return copied.Finished(); }));
        CHECK(copied.Problems().isEmpty());
    }
    {
        Script pasted({Choose("Paste the copied depth here..."), AcceptNumber()});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&pasted] { return pasted.Finished(); }));
        CHECK(pasted.Problems().isEmpty());
        CHECK(RowValue(*opened.inspector, "Depth") == "2");
        CHECK(RowValue(*opened.inspector, "Character") == "7");
    }
    Script refused({Choose("Paste the copied depth here..."), AnswerNumber(1)});
    emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("depth 1"));
}

TEST_CASE("A depth copied from one animation pastes into another with its shape") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const auto copy = [&](uint16_t depth) {
        emit timeline->DepthChosen(depth);
        Script copied({Choose(QString("Copy depth %1 here").arg(depth))});
        emit timeline->MenuRequested(QPoint(4, 4), depth, QString());
        REQUIRE(Settle([&copied] { return copied.Finished(); }));
        CHECK(copied.Problems().isEmpty());
    };
    copy(1);
    {
        Script added({Choose("New animation..."), Answer("fresh"), AnswerNumber(12)});
        RunMenu(added, *opened.tree);
        CHECK(added.Problems().isEmpty());
    }
    {
        Script refused({Choose("Paste the copied depth here..."), AcceptNumber()});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
        CHECK(refused.Problems().front().contains("not defined"));
    }
    opened.tree->setCurrentItem(AnimationNamed(*opened.tree, "intro"));
    copy(2);
    opened.tree->setCurrentItem(AnimationNamed(*opened.tree, "fresh"));
    {
        Script pasted({Choose("Paste the copied depth here..."), AcceptNumber()});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&pasted] { return pasted.Finished(); }));
        INFO(pasted.Problems().join("|").toStdString());
        CHECK(pasted.Problems().isEmpty());
    }
    QAction* save = ShortcutAction(opened.window, QKeySequence(QKeySequence::Save));
    REQUIRE(save != nullptr);
    save->trigger();

    QFile read(opened.dir.filePath("sample.ifs"));
    REQUIRE(read.open(QIODevice::ReadOnly));
    const QByteArray bytes = read.readAll();
    const auto file = Document::File::Open(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.constData()),
                                 static_cast<std::size_t>(bytes.size())));
    REQUIRE(file.has_value());
    std::string fresh;
    for (const Document::Node& node : file->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == "fresh")
                fresh = child.path;
        }
    }
    const auto animation = file->ReadAnimation(fresh);
    REQUIRE(animation.has_value());
    std::optional<uint16_t> placed;
    for (const AfpAnimation::Tag& tag : animation->root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->character) placed = placement->character;
    }
    REQUIRE(placed.has_value());
    const bool shaped =
        std::ranges::any_of(animation->root.tags, [&](const AfpAnimation::Tag& tag) {
            const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body);
            return shape != nullptr && shape->id == *placed;
        });
    CHECK(shaped);
    CHECK(file->ShapeFile(fresh, *placed).has_value());

    const auto offers_paste = [&] {
        bool found = false;
        Script looked({Look("Paste the copied depth here...", found)});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&looked] { return looked.Finished(); }));
        return found;
    };
    CHECK(offers_paste());
    opened.window.OpenDocument(opened.dir.filePath("sample.ifs"));
    CHECK_FALSE(offers_paste());
}

TEST_CASE("A colour row takes its value from the colour picker") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->DepthChosen(1);
    QTableWidgetItem* cell = ValueCell(*opened.inspector, "Multiply colour");
    REQUIRE(cell != nullptr);
    const QPoint at = opened.inspector->visualItemRect(cell).center();
    Script picked({Choose("Pick a colour..."), PickColourStep(QColor(10, 20, 30, 200))});
    emit opened.inspector->customContextMenuRequested(at);
    REQUIRE(Settle([&picked] { return picked.Finished(); }));
    CHECK(picked.Problems().isEmpty());
    CHECK(RowValue(*opened.inspector, "Multiply colour") == "10, 20, 30, 200");

    QTableWidgetItem* moved = ValueCell(*opened.inspector, "Translation");
    REQUIRE(moved != nullptr);
    REQUIRE((moved->flags() & Qt::ItemIsEditable) != 0);
    bool offered = false;
    Script looked({Look("Pick a colour...", offered)});
    emit opened.inspector->customContextMenuRequested(
        opened.inspector->visualItemRect(moved).center());
    QApplication::processEvents();
    CHECK_FALSE(offered);
}

TEST_CASE("A stage drag only changes the document when it ends") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    emit timeline->DepthChosen(1);
    const std::string before = RowValue(*opened.inspector, "Translation");
    CHECK_FALSE(undo->isEnabled());

    Script script({});
    emit viewport->Dragged(1, 5, 0, false);
    emit viewport->Dragged(1, 10, 0, false);
    QApplication::processEvents();
    CHECK(RowValue(*opened.inspector, "Translation") == before);
    CHECK_FALSE(undo->isEnabled());

    emit viewport->Dragged(1, 10, 0, true);
    QApplication::processEvents();
    CHECK(script.Problems().isEmpty());
    CHECK(RowValue(*opened.inspector, "Translation") == "200, 0");
    CHECK(undo->isEnabled());
    undo->trigger();
    CHECK(RowValue(*opened.inspector, "Translation") == before);
}

TEST_CASE("An arrow key on the stage nudges the selected depth as one undo step") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    emit timeline->DepthChosen(2);
    const std::string before = RowValue(*opened.inspector, "Translation");

    Script script({});
    QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
    QApplication::sendEvent(viewport, &right);
    QApplication::processEvents();
    CHECK(script.Problems().isEmpty());
    CHECK(RowValue(*opened.inspector, "Translation") == "200, 0");
    undo->trigger();
    CHECK(RowValue(*opened.inspector, "Translation") == before);
}

TEST_CASE("A depth hidden in the view cannot be picked and leaves the document alone") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    const auto pick = [&] {
        emit timeline->DepthChosen(1);
        emit viewport->Picked(1, 1);
        return RowValue(*opened.inspector, "Depth");
    };
    CHECK(pick() == "2");
    {
        Script hidden({Choose("Hide depth 2 in the view")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&hidden] { return hidden.Finished(); }));
        CHECK(hidden.Problems().isEmpty());
    }
    CHECK_FALSE(undo->isEnabled());
    CHECK(pick() != "2");
    emit timeline->DepthChosen(2);
    {
        Script shown({Choose("Show depth 2 in the view")});
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&shown] { return shown.Finished(); }));
        CHECK(shown.Problems().isEmpty());
    }
    CHECK(pick() == "2");
    CHECK_FALSE(undo->isEnabled());
}

TEST_CASE("Solo hides every other depth, and a locked depth cannot be picked on stage") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    const auto run = [&](std::vector<Script::Step> steps) {
        Script script(std::move(steps));
        emit timeline->MenuRequested(QPoint(4, 4), 0, QString());
        REQUIRE(Settle([&script] { return script.Finished(); }));
        CHECK(script.Problems().isEmpty());
    };
    const auto pick = [&] {
        emit timeline->DepthChosen(1);
        emit viewport->Picked(1, 1);
        return RowValue(*opened.inspector, "Depth");
    };

    emit timeline->DepthChosen(2);
    run({Choose("Solo depth 2 in the view")});
    emit timeline->DepthChosen(1);
    bool one_hidden = false;
    run({Look("Show depth 1 in the view", one_hidden)});
    CHECK(one_hidden);
    emit timeline->DepthChosen(2);
    bool two_hidden = true;
    run({Look("Show depth 2 in the view", two_hidden)});
    CHECK_FALSE(two_hidden);
    CHECK(pick() == "2");
    run({Choose("Show every hidden depth")});

    emit timeline->DepthChosen(2);
    run({Choose("Lock depth 2 on stage")});
    CHECK(pick() != "2");
    emit timeline->DepthChosen(2);
    run({Choose("Unlock depth 2 on stage")});
    CHECK(pick() == "2");
}

TEST_CASE("Frame keys step through the clip and jump between a depth's changes") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->DepthChosen(2);
    Script script({});

    const auto at = [&](uint32_t frame) {
        emit timeline->FrameChosen(frame);
        QApplication::processEvents();
        return timeline->grab().toImage();
    };
    const std::vector<QImage> frames{at(0), at(1), at(2)};
    REQUIRE(frames[0] != frames[2]);
    at(0);
    const auto press = [&](Qt::Key key) {
        QAction* action = ShortcutAction(opened.window, QKeySequence(key));
        REQUIRE(action != nullptr);
        action->trigger();
        QApplication::processEvents();
        return timeline->grab().toImage();
    };
    CHECK(press(Qt::Key_K) == frames[2]);
    CHECK(press(Qt::Key_K) == frames[2]);
    CHECK(press(Qt::Key_J) == frames[0]);
    CHECK(press(Qt::Key_J) == frames[0]);
    CHECK(press(Qt::Key_PageDown) == frames[1]);
    CHECK(press(Qt::Key_End) == frames[2]);
    CHECK(press(Qt::Key_PageDown) == frames[2]);
    CHECK(press(Qt::Key_PageUp) == frames[1]);
    CHECK(press(Qt::Key_Home) == frames[0]);
    CHECK(press(Qt::Key_PageUp) == frames[0]);
    CHECK(script.Problems().isEmpty());
}

TEST_CASE("B and N set the work area on the ruler and it can be cleared") {
    Opened opened;
    Open(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const auto picture = [&] {
        QApplication::processEvents();
        return timeline->grab().toImage();
    };
    emit timeline->FrameChosen(1);
    const QImage plain = picture();
    QAction* start = ShortcutAction(opened.window, QKeySequence(Qt::Key_B));
    QAction* end = ShortcutAction(opened.window, QKeySequence(Qt::Key_N));
    REQUIRE(start != nullptr);
    REQUIRE(end != nullptr);
    end->trigger();
    const QImage ended = picture();
    CHECK(ended != plain);
    start->trigger();
    CHECK(picture() != ended);
    const QList<QAction*> actions = opened.window.findChildren<QAction*>();
    const auto clear = std::ranges::find_if(
        actions, [](const QAction* action) { return action->text() == "Clear the work area"; });
    REQUIRE(clear != actions.end());
    (*clear)->trigger();
    CHECK(picture() == plain);
}

TEST_CASE("Saving a frame without the preview says why") {
    Opened opened;
    Open(opened);
    QAction* save = ShortcutAction(opened.window, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    REQUIRE(save != nullptr);
    Script script({});
    save->trigger();
    REQUIRE(Settle([&script] { return !script.Problems().isEmpty(); }));
    CHECK(script.Problems().front().contains("preview"));
}

TEST_CASE("A taken name is refused with a message and adds nothing") {
    Opened opened;
    Open(opened);
    Script refused({Choose("New animation..."), Answer("intro"), AnswerNumber(3)});
    RunMenu(refused, *opened.tree);
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("already"));
    int animations = 0;
    for (QTreeWidgetItemIterator it(opened.tree); *it != nullptr; ++it)
        animations += (*it)->text(1) == kAnimationKind ? 1 : 0;
    CHECK(animations == 1);
}

TEST_CASE("The background option is remembered without a preview host") {
    Opened opened;
    Open(opened);
    QAction* background = nullptr;
    const QList<QAction*> actions = opened.window.findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action->text() == "Draw the &background colour") background = action;
    }
    REQUIRE(background != nullptr);
    CHECK_FALSE(background->isChecked());
    Script script({});
    background->setChecked(true);
    QApplication::processEvents();
    CHECK(script.Problems().isEmpty());
    CHECK(QSettings().value("preview/background").toBool());
    background->setChecked(false);
    CHECK_FALSE(QSettings().value("preview/background").toBool());
}

TEST_CASE("Stage snapping is on until it is turned off, and the choice is kept") {
    Opened opened;
    Open(opened);
    QAction* snap = nullptr;
    const QList<QAction*> actions = opened.window.findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action->text() == "&Snap while moving on stage") snap = action;
    }
    REQUIRE(snap != nullptr);
    CHECK(snap->isChecked());
    snap->setChecked(false);
    CHECK_FALSE(QSettings().value("stage/snap", true).toBool());
    Editor::Window reopened;
    const QList<QAction*> again = reopened.findChildren<QAction*>();
    const auto kept =
        std::ranges::find(again, QString("&Snap while moving on stage"), &QAction::text);
    REQUIRE(kept != again.end());
    CHECK_FALSE((*kept)->isChecked());
    snap->setChecked(true);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "minimal");
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("573RendererWindowTests");
    QApplication::setApplicationName("IFS Editor window tests");
    return Catch::Session().run(argc, argv);
}
