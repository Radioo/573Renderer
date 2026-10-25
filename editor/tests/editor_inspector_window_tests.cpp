#include <catch2/catch_test_macros.hpp>

#include "editor_inspector.h"
#include "sample_package.h"
#include "editor_script_editor.h"
#include "editor_theme.h"
#include "editor_timeline.h"
#include "editor_window.h"

#include "document/keyframe_edit.h"

#include <Qsci/qsciscintilla.h>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QIODevice>
#include <QFile>
#include <QByteArray>
#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QWidget>
#include <QKeySequence>
#include <QComboBox>
#include <QAction>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMouseEvent>
#include <QLineEdit>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QToolButton>

#include <string>

#include "window_test_support.h"

using namespace WindowTest;

namespace {

QDoubleSpinBox& Box(QWidget& window, const QString& label, int at) {
    auto* box = window.findChild<QDoubleSpinBox*>(QString("value_%1_%2").arg(label).arg(at));
    REQUIRE(box != nullptr);
    return *box;
}

QString MarkTip(QWidget& window, const QString& label) {
    auto* mark = window.findChild<QLabel*>("set_on_" + label);
    REQUIRE(mark != nullptr);
    return mark->toolTip();
}

void Commit(QDoubleSpinBox& box, double value) {
    box.setValue(value);
    emit box.editingFinished();
    QApplication::processEvents();
}

}

TEST_CASE("The inspector shows the placement in pixels, percent and degrees") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(2);
    emit timeline->DepthChosen(2);
    CHECK(RowValue(*opened.inspector, "Translation") == "40, 0");
    CHECK(Box(opened.window, "Position", 0).value() == 2.0);
    CHECK(Box(opened.window, "Position", 1).value() == 0.0);
    CHECK(Box(opened.window, "Scale", 0).value() == 100.0);
    CHECK(Box(opened.window, "Rotation", 0).value() == 0.0);
    auto* title = opened.window.findChild<QLabel*>("inspector_title");
    auto* badge = opened.window.findChild<QLabel*>("inspector_badge");
    REQUIRE(title != nullptr);
    REQUIRE(badge != nullptr);
    CHECK(title->text() == "Depth 2");
    CHECK(badge->text() == "BAKED");
    CHECK(MarkTip(opened.window, "Position") == "Set on this frame");
    emit timeline->FrameChosen(1);
    CHECK(MarkTip(opened.window, "Position") == "Set on frame 0");
}

TEST_CASE("Typing a position into the inspector moves the depth by the difference") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(2);
    emit timeline->DepthChosen(2);
    Commit(Box(opened.window, "Position", 0), 10.0);
    CHECK(RowValue(*opened.inspector, "Translation") == "200, 0");
    CHECK(Box(opened.window, "Position", 0).value() == 10.0);
    CHECK(CommandsOf(opened.window).Action("edit.undo")->text() ==
          QString("&Undo Position of depth 2"));
    REQUIRE(RunCommand(opened.window, "edit.undo").isEmpty());
    CHECK(RowValue(*opened.inspector, "Translation") == "40, 0");
}

TEST_CASE("Turning a depth from the inspector keeps its scale and writes the matrix") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(2);
    emit timeline->DepthChosen(2);
    Commit(Box(opened.window, "Scale", 0), 50.0);
    CHECK(Box(opened.window, "Scale", 0).value() == 50.0);
    Commit(Box(opened.window, "Rotation", 0), 90.0);
    CHECK(Box(opened.window, "Rotation", 0).value() == 90.0);
    CHECK(Box(opened.window, "Scale", 0).value() == 50.0);
    CHECK(RowValue(*opened.inspector, "Rotate skew") != "no Rotate skew row");
}

TEST_CASE("An owned depth's inspector keys the value it is given and the diamonds follow") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    OwnDroppedDot(opened);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    auto* badge = opened.window.findChild<QLabel*>("inspector_badge");
    REQUIRE(badge != nullptr);
    CHECK(badge->text().startsWith("KEYED"));

    auto* keying = opened.window.findChild<QToolButton*>("keying_Rotation");
    REQUIRE(keying != nullptr);
    CHECK(keying->text() == "+");
    keying->click();
    QApplication::processEvents();
    auto* animated = opened.window.findChild<QToolButton*>("keying_Rotation");
    REQUIRE(animated != nullptr);
    CHECK(animated->text() == "K");

    Commit(Box(opened.window, "Position", 0), 30.0);
    CHECK(Box(opened.window, "Position", 0).value() == 30.0);
    CHECK(CommandsOf(opened.window).Action("edit.undo")->text() ==
          QString("&Undo Position of depth 3"));
}

TEST_CASE("The inspector keeps every raw placement field editable") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(2);
    emit timeline->DepthChosen(2);
    REQUIRE(opened.inspector->objectName() == "raw_fields");
    QTableWidgetItem* cell = ValueCell(*opened.inspector, "Translation");
    REQUIRE(cell != nullptr);
    {
        Script edited({});
        cell->setText("60, 20");
        QApplication::processEvents();
        CHECK(edited.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Translation") == "60, 20");
    CHECK(Box(opened.window, "Position", 0).value() == 3.0);
}

TEST_CASE("The inspector's Content section names the character and edits the blend and the mask") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    auto* content = opened.window.findChild<QWidget*>("content_character");
    auto* blend = opened.window.findChild<QComboBox*>("content_blend");
    auto* mask = opened.window.findChild<QSpinBox*>("content_clip_depth");
    REQUIRE(content != nullptr);
    REQUIRE(blend != nullptr);
    REQUIRE(mask != nullptr);
    CHECK_FALSE(blend->isVisibleTo(&opened.window));

    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(blend->isVisibleTo(&opened.window));
    CHECK(qobject_cast<QLabel*>(content)->text().contains("dot"));
    CHECK(blend->currentText() == "Normal");
    CHECK(mask->value() == 0);

    {
        Script chosen({});
        blend->setCurrentIndex(blend->findData(4));
        emit blend->activated(blend->currentIndex());
        QApplication::processEvents();
        CHECK(chosen.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Blend") == "4");
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(blend->currentText() == "Additive");

    {
        Script masked({});
        mask->setValue(3);
        emit mask->editingFinished();
        QApplication::processEvents();
        CHECK(masked.Problems().isEmpty());
    }
    CHECK(RowValue(*opened.inspector, "Clip depth") == "3");

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    undo->trigger();
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(blend->currentText() == "Normal");
    CHECK(mask->value() == 0);
}

TEST_CASE("Replace on the Content section puts another character on the depth") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    auto* replace = opened.window.findChild<QToolButton*>("content_replace");
    auto* shown = opened.window.findChild<QLabel*>("content_character");
    REQUIRE(replace != nullptr);
    REQUIRE(shown != nullptr);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    const QString before = shown->text();
    const std::string character = RowValue(*opened.inspector, "Character");
    const QString png = opened.dir.filePath("leaf.png");
    QFile written(png);
    REQUIRE(written.open(QIODevice::WriteOnly));
    written.write(QByteArray(reinterpret_cast<const char*>(kTinyPng.data()),
                             static_cast<qsizetype>(kTinyPng.size())));
    written.close();
    {
        Script added({Choose("Add an image from a file..."), PickFile(png)});
        RunMenu(added, *opened.tree);
        CHECK(added.Problems().isEmpty());
    }
    {
        Script placed({AnswerMatching("leaf")});
        RunCommand(opened.window, "depth.add");
        REQUIRE(Settle([&placed] { return placed.Finished(); }));
        INFO(placed.Problems().join("|").toStdString());
        CHECK(placed.Problems().isEmpty());
    }
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    REQUIRE(library != nullptr);
    QString other;
    for (QTreeWidgetItemIterator it(library); *it != nullptr; ++it) {
        if ((*it)->text(0) != before && !(*it)->text(0).isEmpty()) other = (*it)->text(0);
    }
    REQUIRE_FALSE(other.isEmpty());
    {
        Script picked({AnswerMatching(other)});
        replace->click();
        REQUIRE(Settle([&picked] { return picked.Finished(); }));
        INFO(picked.Problems().join("|").toStdString());
        CHECK(picked.Problems().isEmpty());
    }
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK_FALSE(shown->text().isEmpty());
    CHECK(RowValue(*opened.inspector, "Character") != character);
    CHECK(shown->text() != before);
}

TEST_CASE("Filters are only added and removed on a depth the project owns") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    auto* add = opened.window.findChild<QToolButton*>("filter_add_matrix");
    REQUIRE(add != nullptr);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK_FALSE(add->isVisibleTo(&opened.window));

    OwnDroppedDot(opened);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    QApplication::processEvents();
    CHECK(add->isVisibleTo(&opened.window));
    Script refused({});
    add->click();
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    CHECK(refused.Problems().front().contains("Filters"));
}

TEST_CASE("A matrix field typed into the raw table changes what the stage draws") {
    Opened opened;
    Open(opened, true);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    REQUIRE(Box(opened.window, "Scale", 0).value() == 100.0);

    QTableWidgetItem* raw = ValueCell(*opened.inspector, "Scale");
    REQUIRE(raw != nullptr);
    {
        Script scaled({});
        raw->setText("2048, 2048");
        QApplication::processEvents();
        INFO(scaled.Problems().join("|").toStdString());
        CHECK(scaled.Problems().isEmpty());
    }
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(RowValue(*opened.inspector, "Scale") == "2048, 2048");
    CHECK(Box(opened.window, "Scale", 0).value() == 200.0);
    CHECK(Box(opened.window, "Scale", 1).value() == 200.0);

    QAction* undo = ShortcutAction(opened.window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    undo->trigger();
    emit timeline->DepthChosen(2);
    QApplication::processEvents();
    CHECK(Box(opened.window, "Scale", 0).value() == 100.0);
}

TEST_CASE("The script on a frame opens in the editor and compiles back into the animation") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* section = opened.window.findChild<QWidget*>("section_script");
    auto* title = opened.window.findChild<QLabel*>("script_title");
    auto* area = opened.window.findChild<QsciScintilla*>("script_text");
    auto* compile = opened.window.findChild<QPushButton*>("script_compile");
    REQUIRE(timeline != nullptr);
    REQUIRE(section != nullptr);
    REQUIRE(title != nullptr);
    REQUIRE(area != nullptr);
    REQUIRE(compile != nullptr);
    CHECK(section->parentWidget()->isHidden());

    emit timeline->FrameChosen(1);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();
    REQUIRE_FALSE(section->parentWidget()->isHidden());
    CHECK(title->text().contains("Frame 1"));
    CHECK(area->text().contains("intro"));
    CHECK_FALSE(compile->isEnabled());

    area->setText("gotoAndPlay(\"outro\")\n");
    QApplication::processEvents();
    REQUIRE(compile->isEnabled());
    {
        Script watching({});
        compile->click();
        REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));
        INFO(watching.Problems().join("|").toStdString());
        CHECK(watching.Problems().isEmpty());
    }

    auto* again = opened.window.findChild<QsciScintilla*>("script_text");
    REQUIRE(again != nullptr);
    CHECK(again->text().contains("outro"));
    CHECK_FALSE(again->text().contains("intro"));
    CHECK_FALSE(compile->isEnabled());
}

TEST_CASE("A script the compiler refuses is kept in the editor with the reason") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();

    auto* area = opened.window.findChild<QsciScintilla*>("script_text");
    auto* compile = opened.window.findChild<QPushButton*>("script_compile");
    auto* problem = opened.window.findChild<QLabel*>("script_status");
    REQUIRE(area != nullptr);
    REQUIRE(compile != nullptr);
    REQUIRE(problem != nullptr);

    area->setText("sprocket()\n");
    QApplication::processEvents();
    REQUIRE(compile->isEnabled());
    compile->click();
    REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));

    CHECK(area->text().contains("sprocket()"));
    CHECK_FALSE(problem->isHidden());
    CHECK(problem->text().contains("sprocket()"));
}

TEST_CASE("Ctrl clicking a name the package holds goes to it in the timeline") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "loop"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();

    auto* area = opened.window.findChild<QsciScintilla*>("script_text");
    auto* seeker = opened.window.findChild<QSpinBox*>("timeline_frame");
    REQUIRE(area != nullptr);
    REQUIRE(seeker != nullptr);
    REQUIRE(area->text().contains("\"loop\""));
    REQUIRE(seeker->value() == 1);

    const qsizetype quote = area->text(0).indexOf(QChar('"'));
    REQUIRE(quote >= 0);
    const int at = area->positionFromLineIndex(0, static_cast<int>(quote) + 1);
    const int x = static_cast<int>(
        area->SendScintilla(QsciScintillaBase::SCI_POINTXFROMPOSITION, 0UL, static_cast<long>(at)));
    const int y = static_cast<int>(
        area->SendScintilla(QsciScintillaBase::SCI_POINTYFROMPOSITION, 0UL, static_cast<long>(at)));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(x + 1, y + 2), QPointF(), Qt::LeftButton,
                      Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(area->viewport(), &press);
    QApplication::processEvents();

    CHECK(Settle([seeker] { return seeker->value() == 2; }));
}

TEST_CASE("The script list marks the one being edited") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();
    auto* open = opened.window.findChild<QPushButton*>("script_place");
    REQUIRE(open != nullptr);
    open->click();
    QApplication::processEvents();

    const auto amber = [&opened] {
        int many = 0;
        for (const QLabel* dot : opened.window.findChildren<QLabel*>("ide_dot")) {
            if (dot->styleSheet().contains(Editor::Theme::kAmber.name())) many++;
        }
        return many;
    };
    REQUIRE(!opened.window.findChildren<QLabel*>("ide_dot").isEmpty());
    CHECK(amber() == 0);

    auto* wide = opened.window.findChild<QsciScintilla*>("script_ide_text");
    REQUIRE(wide != nullptr);
    wide->setText("stop()\n");
    QApplication::processEvents();
    CHECK(amber() == 1);
}

TEST_CASE("Open full replaces the whole view with the script IDE and closes back") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* centre = opened.window.findChild<QStackedWidget*>("centre_stack");
    auto* ide = opened.window.findChild<QWidget*>("script_ide");
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(centre != nullptr);
    REQUIRE(ide != nullptr);
    REQUIRE(timeline != nullptr);
    CHECK(centre->currentWidget() != ide);

    emit timeline->FrameChosen(1);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();

    auto* open = opened.window.findChild<QPushButton*>("script_place");
    REQUIRE(open != nullptr);
    open->click();
    QApplication::processEvents();

    CHECK(centre->currentWidget() == ide);
    auto* crumb = opened.window.findChild<QLabel*>("ide_crumb");
    auto* wide = opened.window.findChild<QsciScintilla*>("script_ide_text");
    auto* scripts = opened.window.findChild<QTreeWidget*>("ide_scripts");
    auto* names = opened.window.findChild<QTreeWidget*>("ide_names");
    REQUIRE(crumb != nullptr);
    REQUIRE(wide != nullptr);
    REQUIRE(scripts != nullptr);
    REQUIRE(names != nullptr);
    CHECK(crumb->text().contains("frame 1"));
    CHECK(wide->text().contains("intro"));
    CHECK(scripts->topLevelItemCount() > 0);
    CHECK(names->topLevelItemCount() > 0);

    auto* close = opened.window.findChild<QPushButton*>("ide_close");
    REQUIRE(close != nullptr);
    close->click();
    QApplication::processEvents();
    CHECK(centre->currentWidget() != ide);
}

TEST_CASE("The IDE compiles the script it is showing back into the animation") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(1);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();
    auto* open = opened.window.findChild<QPushButton*>("script_place");
    REQUIRE(open != nullptr);
    open->click();
    QApplication::processEvents();

    auto* wide = opened.window.findChild<QsciScintilla*>("script_ide_text");
    auto* compile = opened.window.findChild<QPushButton*>("ide_compile");
    REQUIRE(wide != nullptr);
    REQUIRE(compile != nullptr);
    CHECK_FALSE(compile->isEnabled());

    wide->setText("gotoAndPlay(\"outro\")\n");
    QApplication::processEvents();
    REQUIRE(compile->isEnabled());
    {
        Script watching({});
        compile->click();
        REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));
        INFO(watching.Problems().join("|").toStdString());
        CHECK(watching.Problems().isEmpty());
    }

    auto* again = opened.window.findChild<QsciScintilla*>("script_ide_text");
    REQUIRE(again != nullptr);
    CHECK(again->text().contains("outro"));
    CHECK_FALSE(again->text().contains("intro"));
}

TEST_CASE("A placement's own script opens in the IDE and compiles back") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WritePlacementScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(0);
    emit timeline->DepthsChosen({});
    QApplication::processEvents();

    auto* open = opened.window.findChild<QPushButton*>("script_place");
    REQUIRE(open != nullptr);
    open->click();
    QApplication::processEvents();

    auto* scripts = opened.window.findChild<QTreeWidget*>("ide_scripts");
    REQUIRE(scripts != nullptr);
    QTreeWidgetItem* owned = nullptr;
    for (QTreeWidgetItemIterator it(scripts); *it != nullptr; ++it) {
        if ((*it)->text(1).contains("intro")) owned = *it;
    }
    REQUIRE(owned != nullptr);
    scripts->setCurrentItem(owned);
    QApplication::processEvents();

    auto* wide = opened.window.findChild<QsciScintilla*>("script_ide_text");
    auto* compile = opened.window.findChild<QPushButton*>("ide_compile");
    REQUIRE(wide != nullptr);
    REQUIRE(compile != nullptr);
    CHECK(wide->text().contains("intro"));

    wide->setText("gotoAndPlay(\"outro\")\n");
    QApplication::processEvents();
    REQUIRE(compile->isEnabled());
    {
        Script watching({});
        compile->click();
        REQUIRE(Settle([&opened] { return !opened.window.Loading(); }));
        INFO(watching.Problems().join("|").toStdString());
        CHECK(watching.Problems().isEmpty());
    }
    CHECK(wide->text().contains("outro"));
}

TEST_CASE("An owned depth's script is written in the real editor, not a bare text box") {
    Opened opened;
    Open(opened, true);
    OwnDroppedDot(opened);
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->FrameChosen(0);
    emit timeline->DepthChosen(3);
    QApplication::processEvents();

    QStringList seen;
    Script typing({[&opened, &seen] {
        auto* asking = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (asking == nullptr || asking->objectName() != QString("owned_script_dialog"))
            return false;
        auto* area = asking->findChild<QsciScintilla*>("script_ide_text");
        auto* answers = asking->findChild<QDialogButtonBox*>("owned_script_answers");
        if (area == nullptr || answers == nullptr) return false;
        seen.append("editor");
        seen.append(area->lexer() == nullptr ? "plain" : "highlighted");
        answers->button(QDialogButtonBox::Cancel)->click();
        return true;
    }});
    REQUIRE(RunCommand(opened.window, "depth.edit_script").isEmpty());
    REQUIRE(Settle([&typing] { return typing.Finished(); }));
    INFO(typing.Problems().join("|").toStdString());
    CHECK(seen.contains("editor"));
    CHECK(seen.contains("highlighted"));
    CHECK(typing.Problems().isEmpty());
}

TEST_CASE("Clicking a script mark brings the inspector forward with that frame's script") {
    Opened opened;
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WriteFrameScript(opened.dir, "intro"));
    WaitForOpen(opened.window);
    ShowOffScreen(opened.window);
    ads::CDockWidget* inputs = Panel(opened.window, "Inputs");
    REQUIRE(inputs != nullptr);
    inputs->setAsCurrentTab();
    QApplication::processEvents();
    REQUIRE(inputs->isCurrentTab());

    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    emit timeline->ScriptChosen(1);
    QApplication::processEvents();

    ads::CDockWidget* inspector = Panel(opened.window, "Inspector");
    REQUIRE(inspector != nullptr);
    CHECK(inspector->isCurrentTab());
    auto* title = opened.window.findChild<QLabel*>("script_title");
    REQUIRE(title != nullptr);
    CHECK(title->text().contains("Frame 1"));
}
