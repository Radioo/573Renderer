#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "editor_window.h"
#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "formats/ifs_archive.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QDialog>
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

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kStepMs = 5;
constexpr uint32_t kFixedRate = 0x2;
const QString kAnimationKind = "animation";

class Script {
public:
    using Step = std::function<bool()>;

    explicit Script(std::vector<Step> steps) : steps_(steps.begin(), steps.end()) {
        timer_.setInterval(kStepMs);
        QObject::connect(&timer_, &QTimer::timeout, [this] { Run(); });
        timer_.start();
    }

    [[nodiscard]] bool Finished() const { return steps_.empty(); }
    [[nodiscard]] const QStringList& Problems() const { return problems_; }

private:
    void Run() {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            problems_.append(box->text());
            box->reject();
            return;
        }
        if (steps_.empty()) return;
        if (steps_.front()()) steps_.pop_front();
    }

    std::deque<Step> steps_;
    QStringList problems_;
    QTimer timer_;
};

Script::Step Choose(const QString& text) {
    return [text] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr) return false;
        const QList<QAction*> actions = menu->actions();
        for (QAction* action : actions) {
            if (action->text() != text) continue;
            menu->setActiveAction(action);
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(menu, &press);
            return true;
        }
        menu->close();
        return true;
    };
}

Script::Step Answer(const QString& text) {
    return [text] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->setTextValue(text);
        dialog->accept();
        return true;
    };
}

Script::Step AnswerNumber(int number) {
    return [number] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->setIntValue(number);
        dialog->accept();
        return true;
    };
}

QString WritePackage(const QTemporaryDir& dir) {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    const std::string path = "afp/" + SamplePackage::HashPath("intro");
    auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    animation->name = Document::InternString(*animation, "intro");
    animation->flags |= kFixedRate;
    REQUIRE(file->WriteAnimation(path, *animation).has_value());
    const auto encoded = file->Encode();
    REQUIRE(encoded.has_value());
    const QString out = dir.filePath("sample.ifs");
    QFile written(out);
    REQUIRE(written.open(QIODevice::WriteOnly));
    written.write(QByteArray(reinterpret_cast<const char*>(encoded->data()),
                             static_cast<qsizetype>(encoded->size())));
    return out;
}

Script::Step PickFile(const QString& path) {
    return [path] {
        auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        auto* name = dialog->findChild<QLineEdit*>("fileNameEdit");
        if (name == nullptr) return false;
        name->setText(QDir::toNativeSeparators(path));
        static_cast<QDialog*>(dialog)->accept();
        return true;
    };
}

QString WriteImagesOnly(const QTemporaryDir& dir) {
    Ifs::Archive archive = SamplePackage::SampleArchive();
    std::erase_if(archive.entries, [](const Ifs::Entry& entry) { return entry.name == "afp"; });
    const auto encoded = Ifs::Write(archive);
    REQUIRE(encoded.has_value());
    const QString out = dir.filePath("images.ifs");
    QFile written(out);
    REQUIRE(written.open(QIODevice::WriteOnly));
    written.write(QByteArray(reinterpret_cast<const char*>(encoded->data()),
                             static_cast<qsizetype>(encoded->size())));
    return out;
}

QTreeWidgetItem* AnimationNamed(QTreeWidget& tree, const QString& name) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it) {
        if ((*it)->text(0) == name && (*it)->text(1) == kAnimationKind) return *it;
    }
    return nullptr;
}

std::string RowValue(QTableWidget& inspector, const QString& name) {
    for (int row = 0; row < inspector.rowCount(); row++) {
        const QTableWidgetItem* field = inspector.item(row, 0);
        if (field != nullptr && field->text() == name)
            return inspector.item(row, 1)->text().toStdString();
    }
    return "no " + name.toStdString() + " row";
}

QTableWidgetItem* ValueCell(QTableWidget& inspector, const QString& name) {
    for (int row = 0; row < inspector.rowCount(); row++) {
        const QTableWidgetItem* field = inspector.item(row, 0);
        if (field != nullptr && field->text() == name) return inspector.item(row, 1);
    }
    return nullptr;
}

QAction* ShortcutAction(QWidget& window, const QKeySequence& keys) {
    const QList<QAction*> actions = window.findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action->shortcut() == keys) return action;
    }
    return nullptr;
}

struct Opened {
    QTemporaryDir dir;
    Editor::Window window;
    QTreeWidget* tree = nullptr;
    QTableWidget* inspector = nullptr;
};

void Open(Opened& opened) {
    REQUIRE(opened.dir.isValid());
    opened.window.OpenDocument(WritePackage(opened.dir));
    opened.tree = opened.window.findChild<QTreeWidget*>();
    opened.inspector = opened.window.findChild<QTableWidget*>();
    REQUIRE(opened.tree != nullptr);
    REQUIRE(opened.inspector != nullptr);
}

void RunMenu(Script& script, QTreeWidget& tree) {
    emit tree.customContextMenuRequested(QPoint(4, 4));
    while (!script.Finished())
        QApplication::processEvents();
}

}

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

TEST_CASE("A taken name is refused with a message and adds nothing") {
    Opened opened;
    Open(opened);
    Script refused({Choose("New animation..."), Answer("intro"), AnswerNumber(3)});
    RunMenu(refused, *opened.tree);
    while (refused.Problems().isEmpty())
        QApplication::processEvents();
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
