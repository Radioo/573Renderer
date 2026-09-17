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
#include "formats/ifs_archive.h"

#include <QAction>
#include <QImage>
#include <QPixmap>
#include <QtGlobal>
#include <QApplication>
#include <QByteArray>
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
#include <vector>

namespace {

constexpr int kStepMs = 5;
constexpr qint64 kLongestWaitMs = 10000;
constexpr uint32_t kPreviewFrame = 400;
constexpr uint32_t kNoDepth = 65535;
constexpr uint32_t kFixedRate = 0x2;
constexpr uint32_t kUpdateMatrix = 0x5;
const QString kAnimationKind = "animation";

class Script {
public:
    using Step = std::function<bool()>;

    explicit Script(std::vector<Step> steps) : steps_(steps.begin(), steps.end()) {
        running_.start();
        timer_.setInterval(kStepMs);
        QObject::connect(&timer_, &QTimer::timeout, [this] { Run(); });
        timer_.start();
    }

    [[nodiscard]] bool Finished() const { return steps_.empty(); }
    [[nodiscard]] const QStringList& Problems() const { return problems_; }

private:
    void Run() {
        if (running_.elapsed() > kLongestWaitMs && GiveUp()) return;
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            problems_.append(box->text());
            box->reject();
            return;
        }
        if (steps_.empty()) return;
        if (steps_.front()()) steps_.pop_front();
    }

    bool GiveUp() {
        QWidget* open = QApplication::activeModalWidget();
        if (open == nullptr) open = QApplication::activePopupWidget();
        if (open == nullptr) return false;
        problems_.append(QString("timed out with %1 open").arg(open->metaObject()->className()));
        steps_.clear();
        open->close();
        return true;
    }

    std::deque<Step> steps_;
    QStringList problems_;
    QElapsedTimer running_;
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

Script::Step Look(const QString& text, bool& found) {
    return [text, &found] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr) return false;
        const QList<QAction*> actions = menu->actions();
        found = std::ranges::any_of(
            actions, [&text](const QAction* action) { return action->text() == text; });
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

Script::Step AcceptNumber() {
    return [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->accept();
        return true;
    };
}

QString WritePackage(const QTemporaryDir& dir, bool with_image = false) {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    const std::string path = "afp/" + SamplePackage::HashPath("intro");
    auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    animation->name = Document::InternString(*animation, "intro");
    animation->flags |= kFixedRate;
    REQUIRE(Document::AddDepth(*animation, {}, 1, 7, 0, 2).has_value());
    REQUIRE(file->WriteAnimation(path, *animation).has_value());
    if (with_image) {
        const Document::DepthSpan span{.clip = {}, .depth = 2, .first_frame = 0, .last_frame = 2};
        REQUIRE(file->AddImage("dot", 4, 3, std::vector<uint8_t>(48, 0x40)).has_value());
        REQUIRE(Document::PlaceImage(*file, path, "dot", span).has_value());
        auto placed = file->ReadAnimation(path);
        REQUIRE(placed.has_value());
        AfpAnimation::Placement moved;
        moved.flags = kUpdateMatrix;
        moved.depth = 2;
        moved.translation = std::array<int32_t, 2>{40, 0};
        Document::InsertTag(placed->root, 2, AfpAnimation::Tag{moved});
        REQUIRE(file->WriteAnimation(path, *placed).has_value());
    }
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

void Open(Opened& opened, bool with_image = false) {
    REQUIRE(opened.dir.isValid());
    opened.window.OpenDocument(WritePackage(opened.dir, with_image));
    opened.tree = opened.window.findChild<QTreeWidget*>();
    opened.inspector = opened.window.findChild<QTableWidget*>();
    REQUIRE(opened.tree != nullptr);
    REQUIRE(opened.inspector != nullptr);
}

bool Settle(const std::function<bool()>& done) {
    QElapsedTimer waited;
    waited.start();
    while (!done() && waited.elapsed() < kLongestWaitMs)
        QApplication::processEvents();
    return done();
}

void RunMenu(Script& script, QTreeWidget& tree) {
    emit tree.customContextMenuRequested(QPoint(4, 4));
    REQUIRE(Settle([&script] { return script.Finished(); }));
}

std::optional<uint16_t> WidestTitleDepth(const QString& title) {
    QFile read(title);
    REQUIRE(read.open(QIODevice::ReadOnly));
    const QByteArray bytes = read.readAll();
    const auto file = Document::File::Open(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.constData()),
                                 static_cast<std::size_t>(bytes.size())));
    REQUIRE(file.has_value());
    if (!file) return std::nullopt;
    std::string path;
    for (const Document::Node& node : file->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation && child.name == "title") path = child.path;
        }
    }
    const auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return std::nullopt;
    const auto outlines =
        Document::StageOutlines(*animation, {}, kPreviewFrame, file->ShapeBounds(path));
    REQUIRE(!outlines.empty());
    const auto widest = std::ranges::max_element(outlines, {}, [](const auto& outline) {
        return std::abs(outline.corners[1][0] - outline.corners[0][0]);
    });
    return widest->depth;
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

TEST_CASE("A stage drag previews through the host before it is committed") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    const QString title = game + "/data/graphic/1/title.ifs";
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(title);
    REQUIRE(opening.Problems().isEmpty());

    const std::optional<uint16_t> widest = WidestTitleDepth(title);
    REQUIRE(widest.has_value());
    if (!widest) return;

    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* undo = ShortcutAction(window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    emit timeline->FrameChosen(kPreviewFrame);
    emit timeline->DepthChosen(*widest);
    QApplication::processEvents();
    const QImage before = viewport->grab().toImage();

    emit viewport->Dragged(*widest, 300, 150, false);
    QApplication::processEvents();
    QApplication::processEvents();
    const QImage during = viewport->grab().toImage();
    CHECK(during != before);
    CHECK_FALSE(undo->isEnabled());

    emit viewport->Dragged(*widest, 0, 0, false);
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(viewport->grab().toImage() == before);
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("Hiding a depth in the view takes it out of the rendered frame until it is shown") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    const QString title = game + "/data/graphic/1/title.ifs";
    const std::optional<uint16_t> widest = WidestTitleDepth(title);
    REQUIRE(widest.has_value());
    if (!widest) return;
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(title);
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    const auto grab = [&] {
        emit timeline->DepthChosen(kNoDepth);
        QApplication::processEvents();
        return viewport->grab().toImage();
    };
    emit timeline->FrameChosen(kPreviewFrame);
    const QImage before = grab();
    emit timeline->DepthChosen(*widest);
    const QString depth = QString::number(*widest);
    {
        Script hidden({Choose("Hide depth " + depth + " in the view")});
        emit timeline->MenuRequested(QPoint(4, 4), kPreviewFrame, QString());
        REQUIRE(Settle([&hidden] { return hidden.Finished(); }));
        CHECK(hidden.Problems().isEmpty());
    }
    CHECK(grab() != before);
    emit timeline->DepthChosen(*widest);
    {
        Script shown({Choose("Show every hidden depth")});
        emit timeline->MenuRequested(QPoint(4, 4), kPreviewFrame, QString());
        REQUIRE(Settle([&shown] { return shown.Finished(); }));
        CHECK(shown.Problems().isEmpty());
    }
    CHECK(grab() == before);
    CHECK(opening.Problems().isEmpty());
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
