#include <catch2/catch_test_macros.hpp>

#include "editor_script_editor.h"
#include "editor_script_lexer.h"
#include "editor_theme.h"

#include <Qsci/qsciscintilla.h>

#include <QApplication>
#include <QLabel>
#include <QKeyEvent>
#include <QList>
#include "document/script_docs.h"
#include "document/script_source.h"
#include <QListWidget>
#include <QPushButton>
#include <QObject>
#include <QString>
#include <QStringList>

namespace {

QList<int> Styles(const QString& line) {
    return Editor::ScriptLexer::StylesOf(line);
}

int StyleOfWord(const QString& line, const QString& word) {
    const qsizetype at = line.indexOf(word);
    REQUIRE(at >= 0);
    return Styles(line).at(static_cast<int>(at));
}

}

TEST_CASE("The lexer paints a call, its text and its numbers apart") {
    const QString line = "aep_set_set_frame(this, 30)";
    CHECK(StyleOfWord(line, "aep_set_set_frame") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord(line, "this") == Editor::ScriptLexer::Word);
    CHECK(StyleOfWord(line, "30") == Editor::ScriptLexer::Number);
    CHECK(StyleOfWord(line, "(") == Editor::ScriptLexer::Punctuation);

    const QString quoted = "gotoAndPlay(\"loop\")";
    CHECK(StyleOfWord(quoted, "\"loop\"") == Editor::ScriptLexer::Text);
}

TEST_CASE("The lexer paints the instruction layer the way it paints a call") {
    CHECK(StyleOfWord("call_function", "call_function") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord("store r1", "store") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord("store r1", "r1") == Editor::ScriptLexer::Word);
    CHECK(StyleOfWord("push(540, builtin_0x465)", "builtin_0x465") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord("keep gotoAndStop(3)", "keep") == Editor::ScriptLexer::Word);
    CHECK(StyleOfWord("keep gotoAndStop(3)", "gotoAndStop") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord("push(item(51, ab))", "item") == Editor::ScriptLexer::Word);
}

TEST_CASE("The lexer paints a register statement the way the token sheet asks") {
    const QString bind = "let r1 = getInstanceAtDepth(-16382)";
    CHECK(StyleOfWord(bind, "let") == Editor::ScriptLexer::Word);
    CHECK(StyleOfWord(bind, "r1") == Editor::ScriptLexer::Word);
    CHECK(StyleOfWord(bind, "=") == Editor::ScriptLexer::Punctuation);
    CHECK(StyleOfWord(bind, "getInstanceAtDepth") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord(bind, "-16382") == Editor::ScriptLexer::Number);

    const QString method = "keep r1.gotoAndPlay(540)";
    CHECK(StyleOfWord(method, ".") == Editor::ScriptLexer::Punctuation);
    CHECK(StyleOfWord(method, "gotoAndPlay") == Editor::ScriptLexer::Call);
    CHECK(StyleOfWord(method, "540") == Editor::ScriptLexer::Number);

    const QString write = "r1.frameOffset = 539";
    CHECK(StyleOfWord(write, "frameOffset") == Editor::ScriptLexer::Text);
    CHECK(StyleOfWord(write, "539") == Editor::ScriptLexer::Number);
}

TEST_CASE("The lexer marks a word the compiler would refuse") {
    CHECK(StyleOfWord("sprocket()", "sprocket") == Editor::ScriptLexer::Wrong);
    CHECK(StyleOfWord("gotoAndPlay(loop)", "loop") == Editor::ScriptLexer::Wrong);
    CHECK(StyleOfWord("gotoAndPlay(loop)", "gotoAndPlay") == Editor::ScriptLexer::Call);
}

TEST_CASE("A name the open package does not hold is marked, once the names are known") {
    const QString line = "gotoAndPlay(\"loop\")";
    const QStringList held{"loop", "intro"};
    const QStringList other{"intro"};
    const qsizetype at = line.indexOf(QString('"'));
    REQUIRE(at >= 0);

    CHECK(Editor::ScriptLexer::StylesOf(line).at(static_cast<int>(at)) ==
          Editor::ScriptLexer::Text);
    CHECK(Editor::ScriptLexer::StylesOf(line, held).at(static_cast<int>(at)) ==
          Editor::ScriptLexer::Text);
    CHECK(Editor::ScriptLexer::StylesOf(line, other).at(static_cast<int>(at)) ==
          Editor::ScriptLexer::Wrong);
}

TEST_CASE("A line that is only spacing is painted plain") {
    const QList<int> styles = Styles("   ");
    CHECK(styles.size() == 3);
    for (const int style : styles)
        CHECK(style == Editor::ScriptLexer::Plain);
}

TEST_CASE("Every colour the script editor paints a word with is readable on the field") {
    const Editor::ScriptLexer lexer(13);
    QStringList faint;
    for (int style = 0; style < Editor::ScriptLexer::Styles; style++) {
        const double ratio =
            Editor::Theme::Contrast(lexer.defaultColor(style), lexer.defaultPaper(style));
        if (ratio >= Editor::Theme::kLeastContrast) continue;
        faint.append(QString("%1 is %2:1").arg(lexer.description(style)).arg(ratio, 0, 'f', 2));
    }
    CHECK(faint.join("; ").toStdString() == std::string());
}

TEST_CASE("The vocabulary the editor completes from is what the compiler accepts") {
    const QStringList words = Editor::ScriptLexer::Vocabulary();
    for (const QString& name :
         {"stop", "gotoAndPlay", "aep_set_set_frame", "getInstanceAtDepth", "push", "store",
          "goto_frame2", "set_member", "keep", "let", "this", "item"}) {
        CHECK(words.contains(name));
    }
    CHECK_FALSE(words.contains("sprocket"));
}

TEST_CASE("What the editor offers is the names that start with what was typed") {
    const QStringList names{"loop", "loop_counting", "in", "reward"};
    const QStringList offered = Editor::ScriptEditor::Matches(names, "loo");
    REQUIRE(offered.size() == 2);
    CHECK(offered.at(0) == QString("loop"));
    CHECK(offered.at(1) == QString("loop_counting"));

    CHECK(Editor::ScriptEditor::Matches(names, "loop").size() == 1);
    CHECK(Editor::ScriptEditor::Matches(names, "zz").isEmpty());
}

TEST_CASE("Typing one letter offers every word that starts with it") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    editor.resize(900, 400);
    auto* area = editor.findChild<QsciScintilla*>("script_text");
    auto* offers = editor.findChild<QWidget*>("script_offers");
    auto* rows = editor.findChild<QListWidget*>("script_offer_rows");
    auto* counted = editor.findChild<QLabel*>("script_offer_counted");
    auto* said = editor.findChild<QLabel*>("script_offer_said");
    auto* named = editor.findChild<QLabel*>("script_offer_named");
    REQUIRE(area != nullptr);
    REQUIRE(offers != nullptr);
    REQUIRE(rows != nullptr);
    REQUIRE(counted != nullptr);
    REQUIRE(said != nullptr);
    REQUIRE(named != nullptr);

    editor.ShowScript("Frame 1", "stop()\n");
    CHECK(offers->isHidden());

    area->setText("k");
    area->setCursorPosition(0, 1);
    QApplication::processEvents();

    CHECK_FALSE(offers->isHidden());
    CHECK(rows->count() > 0);
    CHECK(counted->text().contains("match k"));

    area->setText("kee");
    area->setCursorPosition(0, 3);
    QApplication::processEvents();
    CHECK_FALSE(offers->isHidden());
    CHECK(named->text() == QString("keep <call>"));
    CHECK(said->text().contains("Leaves the call's result on the stack"));

    area->setText("");
    area->setCursorPosition(0, 0);
    QApplication::processEvents();
    CHECK(offers->isHidden());
}

TEST_CASE("Every word of the language is documented, and every call at least named") {
    const auto told = [](std::string_view word) {
        const std::optional<Document::ScriptDoc> doc = Document::ScriptWordDoc(word);
        REQUIRE(doc.has_value());
        CHECK_FALSE(doc->signature.empty());
        CHECK_FALSE(doc->summary.empty());
        CHECK_FALSE(doc->returns.empty());
        CHECK(doc->measured);
        return *doc;
    };
    for (const std::string_view word : Document::ScriptWords()) {
        INFO(word);
        told(word);
    }
    for (const std::string_view word : Document::ScriptInstructions()) {
        INFO(word);
        told(word);
    }

    const std::optional<Document::ScriptDoc> call = Document::ScriptWordDoc("gotoAndPlay");
    REQUIRE(call.has_value());
    CHECK(call->call);
    CHECK(call->measured);
    CHECK(call->id == std::string("builtin 0x442"));
    CHECK(call->signature == std::string("gotoAndPlay(clip, frame)"));
    REQUIRE(call->params.size() == 2);
    CHECK(call->params.at(0).name == std::string("clip"));
    CHECK(call->params.at(1).name == std::string("frame"));
    CHECK(call->params.at(1).type == std::string("frame number from 1, or label text"));
    CHECK_FALSE(call->params.at(1).said.empty());

    const std::optional<Document::ScriptDoc> one = Document::ScriptWordDoc("stop");
    REQUIRE(one.has_value());
    CHECK(one->params.size() == 1);

    const std::optional<Document::ScriptDoc> deep = Document::ScriptWordDoc("getInstanceAtDepth");
    REQUIRE(deep.has_value());
    CHECK(deep->params.size() == 1);
    CHECK(deep->returns.find("instance at that depth") != std::string::npos);

    const std::optional<Document::ScriptDoc> unread = Document::ScriptWordDoc("Array");
    REQUIRE(unread.has_value());
    CHECK(unread->call);
    CHECK_FALSE(unread->measured);
    CHECK(unread->params.empty());

    CHECK_FALSE(Document::ScriptWordDoc("sprocket").has_value());
}

TEST_CASE("A name offered inside quotes carries where it lives, and Return takes it") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    editor.resize(900, 400);
    auto* area = editor.findChild<QsciScintilla*>("script_text");
    auto* rows = editor.findChild<QListWidget*>("script_offer_rows");
    auto* named = editor.findChild<QLabel*>("script_offer_named");
    auto* said = editor.findChild<QLabel*>("script_offer_said");
    auto* counted = editor.findChild<QLabel*>("script_offer_counted");
    REQUIRE(area != nullptr);
    REQUIRE(rows != nullptr);
    REQUIRE(named != nullptr);
    REQUIRE(said != nullptr);
    REQUIRE(counted != nullptr);
    editor.KnowNames(
        {Editor::ScriptName{.name = "loop_counting", .detail = "frame label, 89", .kind = "L"},
         Editor::ScriptName{.name = "reward", .detail = "frame label, 630", .kind = "L"}});

    editor.ShowScript("Frame 1", "gotoAndPlay(\"loo\")\n");
    area->setCursorPosition(0, 16);
    QApplication::processEvents();

    REQUIRE(rows->count() == 1);
    CHECK(named->text() == QString("loop_counting"));
    CHECK(said->text().contains("frame label, 89"));
    CHECK(counted->text() == QString("1 of 2 names in this package start with loo"));

    QKeyEvent took(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(area, &took);
    QApplication::processEvents();
    CHECK(area->text().contains("\"loop_counting\""));
}

TEST_CASE("The name under the caret is the one inside the quotes") {
    CHECK(Editor::ScriptEditor::QuotedAt("gotoAndPlay(\"loop\")", 13) == QString("loop"));
    CHECK(Editor::ScriptEditor::QuotedAt("gotoAndPlay(\"loop\")", 16) == QString("loop"));
    CHECK(Editor::ScriptEditor::QuotedAt("gotoAndPlay(\"loop\")", 12).isEmpty());
    CHECK(Editor::ScriptEditor::QuotedAt("gotoAndPlay(\"loop\")", 18).isEmpty());
    CHECK(Editor::ScriptEditor::QuotedAt("stop()", 3).isEmpty());
    CHECK(Editor::ScriptEditor::QuotedAt("", 0).isEmpty());
}

TEST_CASE("Taking a call writes its brackets and leaves the caret where the arguments go") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    editor.resize(900, 400);
    auto* area = editor.findChild<QsciScintilla*>("script_text");
    REQUIRE(area != nullptr);
    editor.ShowScript("Frame 1", "");

    area->setText("deepGotoAndPla");
    area->setCursorPosition(0, 14);
    QApplication::processEvents();

    QKeyEvent took(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(area, &took);
    QApplication::processEvents();

    CHECK(area->text() == QString("deepGotoAndPlay()"));
    int line = 0;
    int index = 0;
    area->getCursorPosition(&line, &index);
    CHECK(index == 16);
}

TEST_CASE("A word that is not a call is taken without brackets") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    editor.resize(900, 400);
    auto* area = editor.findChild<QsciScintilla*>("script_text");
    REQUIRE(area != nullptr);
    editor.ShowScript("Frame 1", "");

    area->setText("get_var");
    area->setCursorPosition(0, 7);
    QApplication::processEvents();

    QKeyEvent took(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(area, &took);
    QApplication::processEvents();

    CHECK(area->text() == QString("get_variable"));
}

TEST_CASE("The hint knows which call the caret is in and which argument it is on") {
    using Editor::ScriptEditor;
    CHECK(ScriptEditor::CalledAround("gotoAndPlay(", 12).name == QString("gotoAndPlay"));
    CHECK(ScriptEditor::CalledAround("gotoAndPlay(", 12).active == 0);
    CHECK(ScriptEditor::CalledAround("aep_set_rect_mask(this, 0, 1920", 31).name ==
          QString("aep_set_rect_mask"));
    CHECK(ScriptEditor::CalledAround("aep_set_rect_mask(this, 0, 1920", 31).active == 2);
    CHECK(ScriptEditor::CalledAround("gotoAndPlay(\"lo", 15).name == QString("gotoAndPlay"));
    CHECK(ScriptEditor::CalledAround("stop()", 6).name.isEmpty());
    CHECK(ScriptEditor::CalledAround("gotoAndPlay", 11).name.isEmpty());
    CHECK(ScriptEditor::CalledAround("", 0).name.isEmpty());
}

TEST_CASE("The script header counts calls when every line is one") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    auto* counted = editor.findChild<QLabel*>("script_counted");
    REQUIRE(counted != nullptr);

    editor.ShowScript("Frame 119", "aep_set_frame_control(this, 6, 181)\n"
                                   "gotoAndPlay(\"loop_counting\")\n"
                                   "stop()\n");
    CHECK(counted->text() == QString("3 calls"));

    editor.ShowScript("Frame 0", "push(1)\nget_variable\n");
    CHECK(counted->text() == QString("2 lines"));

    editor.ShowScript("Frame 0", "let r1 = getInstanceAtDepth(-16382)\nkeep r1.gotoAndPlay(540)\n");
    CHECK(counted->text() == QString("2 lines"));
}

TEST_CASE("Compiling is offered only once the script has been changed") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    auto* compile = editor.findChild<QPushButton*>("script_compile");
    auto* area = editor.findChild<QsciScintilla*>("script_text");
    auto* title = editor.findChild<QLabel*>("script_title");
    REQUIRE(compile != nullptr);
    REQUIRE(area != nullptr);
    REQUIRE(title != nullptr);

    editor.ShowScript("Frame 1", "stop()\n");
    CHECK_FALSE(compile->isEnabled());
    CHECK(title->text() == QString("Frame 1"));

    QStringList asked;
    QObject::connect(&editor, &Editor::ScriptEditor::Compiled, &editor,
                     [&asked](const QString& source) { asked.append(source); });
    compile->click();
    CHECK(asked.isEmpty());

    area->setText("gotoAndPlay(\"loop\")\n");
    QApplication::processEvents();
    CHECK(compile->isEnabled());
    CHECK(title->text().contains("edited"));

    compile->click();
    REQUIRE(asked.size() == 1);
    CHECK(asked.at(0) == QString("gotoAndPlay(\"loop\")\n"));
}

TEST_CASE("A frame with no script of its own says so and cannot be typed into") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    auto* area = editor.findChild<QsciScintilla*>("script_text");
    auto* problem = editor.findChild<QLabel*>("script_status");
    auto* compile = editor.findChild<QPushButton*>("script_compile");
    REQUIRE(area != nullptr);
    REQUIRE(problem != nullptr);
    REQUIRE(compile != nullptr);

    editor.ShowScript("Frame 1", "stop()\n");
    editor.ShowNothing("Frame 2", "this frame holds no script");
    CHECK(area->isReadOnly());
    CHECK_FALSE(problem->isHidden());
    CHECK(problem->text() == QString("this frame holds no script"));
    CHECK_FALSE(compile->isEnabled());

    editor.ShowScript("Frame 1", "stop()\n");
    CHECK_FALSE(area->isReadOnly());
    CHECK(problem->isHidden());
}

TEST_CASE("A refused compile is shown against the script it was refused for") {
    Editor::ScriptEditor editor(Editor::ScriptEditor::Place::Docked);
    auto* problem = editor.findChild<QLabel*>("script_status");
    REQUIRE(problem != nullptr);

    editor.ShowScript("Frame 1", "stop()\n");
    editor.ShowProblem("this is not a call or an instruction: play()");
    CHECK_FALSE(problem->isHidden());
    CHECK(problem->text().contains("play()"));

    editor.ShowProblem({});
    CHECK(problem->isHidden());
}
