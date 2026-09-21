#pragma once

#include <catch2/catch_test_macros.hpp>

#include <DockManager.h>
#include <DockWidget.h>

#include "editor_commands.h"
#include "editor_popover.h"
#include "editor_timeline.h"
#include "editor_timeline_metrics.h"
#include "editor_viewport.h"
#include "editor_window.h"
#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/place_image.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"

#include <QAction>
#include <QImage>
#include <QPixmap>
#include <QtGlobal>
#include <QApplication>
#include <QScrollArea>
#include <QByteArray>
#include <QColorDialog>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
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
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
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
#include <variant>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace WindowTest {

inline constexpr int kStepMs = 5;
inline constexpr qint64 kLongestWaitMs = 60000;
inline constexpr uint32_t kPreviewFrame = 400;
inline constexpr qint64 kPlayForMs = 700;
inline constexpr std::array<uint8_t, 74> kTinyPng{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x08, 0x06, 0x00, 0x00, 0x00, 0xb9,
    0xea, 0xde, 0x81, 0x00, 0x00, 0x00, 0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8,
    0xcf, 0xc0, 0xf0, 0x1f, 0x84, 0x19, 0x30, 0x18, 0x00, 0xa1, 0x79, 0x0b, 0xf5, 0x4d, 0xc4,
    0x9a, 0x07, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
inline constexpr uint32_t kNoDepth = 65535;
inline constexpr uint32_t kFixedRate = 0x2;
inline constexpr uint32_t kUpdateMatrix = 0x5;
inline const QString kAnimationKind = "animation";

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

inline Script::Step Choose(const QString& text) {
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

inline Script::Step Look(const QString& text, bool& found) {
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

inline Script::Step PickColourStep(const QColor& colour) {
    return [colour] {
        auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->setCurrentColor(colour);
        dialog->accept();
        return true;
    };
}

inline Script::Step Answer(const QString& text) {
    return [text] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->setTextValue(text);
        dialog->accept();
        return true;
    };
}

inline Script::Step AnswerMatching(const QString& part) {
    return [part] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        auto* items = dialog->findChild<QComboBox*>();
        if (items == nullptr) return false;
        for (int at = 0; at < items->count(); at++) {
            if (!items->itemText(at).contains(part)) continue;
            items->setCurrentIndex(at);
            dialog->accept();
            return true;
        }
        return false;
    };
}

inline Script::Step AnswerNumber(int number) {
    return [number] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->setIntValue(number);
        dialog->accept();
        return true;
    };
}

inline Script::Step RejectInput() {
    return [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->reject();
        return true;
    };
}

inline Script::Step AcceptInput() {
    return [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->accept();
        return true;
    };
}

inline QString WritePackage(const QTemporaryDir& dir, bool with_image = false,
                            const QString& named_depth = QString()) {
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
    if (!named_depth.isEmpty()) {
        auto shown = file->ReadAnimation(path);
        REQUIRE(shown.has_value());
        AfpAnimation::Placement told;
        told.flags = kUpdateMatrix;
        told.depth = 1;
        told.name = Document::InternString(*shown, named_depth.toStdString());
        Document::InsertTag(shown->root, 0, AfpAnimation::Tag{told});
        REQUIRE(file->WriteAnimation(path, *shown).has_value());
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

inline QString WriteDigitPlaces(const QTemporaryDir& dir, const QString& lone = QString()) {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    const std::string path = "afp/" + SamplePackage::HashPath("intro");
    for (uint32_t digit = 0; digit < 10; digit++) {
        REQUIRE(file->AddImage("num" + std::to_string(digit) + "_flat", 4, 3,
                               std::vector<uint8_t>(48, 0x40))
                    .has_value());
    }
    auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    animation->name = Document::InternString(*animation, "intro");
    REQUIRE(file->WriteAnimation(path, *animation).has_value());
    const std::array<std::string, 4> named{"score_0001", "score_0010", "score_0100", "score_1000"};
    for (uint16_t place = 0; place < named.size(); place++) {
        const Document::DepthSpan span{.clip = {},
                                       .depth = static_cast<uint16_t>(2 + place),
                                       .first_frame = 0,
                                       .last_frame = 2};
        REQUIRE(Document::PlaceImage(*file, path, "num0_flat", span).has_value());
        auto placed = file->ReadAnimation(path);
        REQUIRE(placed.has_value());
        for (AfpAnimation::Tag& tag : placed->root.tags) {
            auto* one = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (one == nullptr || one->depth != span.depth || !one->character) continue;
            one->name = Document::InternString(*placed, named[place]);
        }
        REQUIRE(file->WriteAnimation(path, *placed).has_value());
    }
    if (!lone.isEmpty()) {
        const Document::DepthSpan span{.clip = {}, .depth = 6, .first_frame = 0, .last_frame = 2};
        REQUIRE(Document::PlaceImage(*file, path, "num0_flat", span).has_value());
        auto placed = file->ReadAnimation(path);
        REQUIRE(placed.has_value());
        for (AfpAnimation::Tag& tag : placed->root.tags) {
            auto* one = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (one == nullptr || one->depth != span.depth || !one->character) continue;
            one->name = Document::InternString(*placed, lone.toStdString());
        }
        REQUIRE(file->WriteAnimation(path, *placed).has_value());
    }
    const auto encoded = file->Encode();
    REQUIRE(encoded.has_value());
    const QString out = dir.filePath("digits.ifs");
    QFile written(out);
    REQUIRE(written.open(QIODevice::WriteOnly));
    written.write(QByteArray(reinterpret_cast<const char*>(encoded->data()),
                             static_cast<qsizetype>(encoded->size())));
    return out;
}

inline QString WriteNamedInSprite(const QTemporaryDir& dir, const QString& name) {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    const std::string path = "afp/" + SamplePackage::HashPath("intro");
    auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    animation->name = Document::InternString(*animation, "intro");
    REQUIRE(Document::AddDepth(*animation, {}, 1, 7, 0, 2).has_value());
    const auto grouped = Document::GroupIntoSprite(
        *animation,
        Document::GroupRange{
            .clip = {}, .first_depth = 1, .last_depth = 1, .first_frame = 0, .last_frame = 2});
    REQUIRE(grouped.has_value());
    AfpAnimation::Container* sprite =
        Document::FindClip(*animation, Document::ClipId{.sprite = *grouped});
    REQUIRE(sprite != nullptr);
    bool marked = false;
    for (AfpAnimation::Tag& tag : sprite->tags) {
        auto* one = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (one == nullptr || !one->character || marked) continue;
        one->name = Document::InternString(*animation, name.toStdString());
        marked = true;
    }
    REQUIRE(marked);
    REQUIRE(file->WriteAnimation(path, *animation).has_value());
    const auto encoded = file->Encode();
    REQUIRE(encoded.has_value());
    const QString out = dir.filePath("nested.ifs");
    QFile written(out);
    REQUIRE(written.open(QIODevice::WriteOnly));
    written.write(QByteArray(reinterpret_cast<const char*>(encoded->data()),
                             static_cast<qsizetype>(encoded->size())));
    return out;
}

inline QTreeWidgetItem* InputNamed(QTreeWidget& inputs, const QString& name) {
    for (QTreeWidgetItemIterator it(&inputs); *it != nullptr; ++it) {
        if ((*it)->text(0) == name) return *it;
    }
    return nullptr;
}

inline Script::Step PickFile(const QString& path) {
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

inline QString WriteImagesOnly(const QTemporaryDir& dir) {
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

inline QTreeWidgetItem* AnimationNamed(QTreeWidget& tree, const QString& name) {
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it) {
        if ((*it)->text(0) == name && (*it)->text(1) == kAnimationKind) return *it;
    }
    return nullptr;
}

inline std::string RowValue(QTableWidget& inspector, const QString& name) {
    for (int row = 0; row < inspector.rowCount(); row++) {
        const QTableWidgetItem* field = inspector.item(row, 0);
        if (field != nullptr && field->text() == name)
            return inspector.item(row, 1)->text().toStdString();
    }
    return "no " + name.toStdString() + " row";
}

inline QTableWidgetItem* ValueCell(QTableWidget& inspector, const QString& name) {
    for (int row = 0; row < inspector.rowCount(); row++) {
        const QTableWidgetItem* field = inspector.item(row, 0);
        if (field != nullptr && field->text() == name) return inspector.item(row, 1);
    }
    return nullptr;
}

inline QAction* ShortcutAction(QWidget& window, const QKeySequence& keys) {
    const QList<QAction*> actions = window.findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action->shortcut() == keys) return action;
    }
    return nullptr;
}

inline bool Settle(const std::function<bool()>& done) {
    QElapsedTimer waited;
    waited.start();
    while (!done() && waited.elapsed() < kLongestWaitMs)
        QApplication::processEvents();
    return done();
}

inline QStringList CrumbTexts(QWidget& window) {
    QStringList texts;
    for (QToolButton* button : window.findChildren<QToolButton*>()) {
        if (button->objectName().startsWith("crumb_")) texts.append(button->text());
    }
    return texts;
}

inline QString OpenClip(QWidget& window) {
    const QStringList crumbs = CrumbTexts(window);
    return crumbs.size() > 2 ? crumbs.back() : QString();
}

inline bool EnterFirstSprite(QWidget& window) {
    auto* library = window.findChild<QTreeWidget*>("library");
    if (library == nullptr) return false;
    for (QTreeWidgetItemIterator it(library); *it != nullptr; ++it) {
        if (!(*it)->text(0).contains("Sprite")) continue;
        emit library->itemDoubleClicked(*it, 0);
        return Settle([&window] { return !OpenClip(window).isEmpty(); });
    }
    return false;
}

inline QImage Picture(QWidget& widget) {
    QImage shot = widget.grab().toImage();
    for (int pass = 0; pass < 8; pass++) {
        const QSize settled = widget.size();
        QApplication::processEvents();
        shot = widget.grab().toImage();
        if (widget.size() == settled) break;
    }
    return shot;
}

inline ads::CDockWidget* Panel(QWidget& window, const QString& title) {
    auto* docks = window.findChild<ads::CDockManager*>();
    REQUIRE(docks != nullptr);
    return docks->findDockWidget(title);
}

inline Editor::Popover& PopoverOf(QWidget& window) {
    auto* popover = window.findChild<Editor::Popover*>("popover");
    REQUIRE(popover != nullptr);
    return *popover;
}

inline void SetPopoverValue(QWidget& window, const QString& label, double value) {
    auto* box = PopoverOf(window).findChild<QDoubleSpinBox*>("popover_" + label);
    REQUIRE(box != nullptr);
    box->setValue(value);
    QApplication::processEvents();
}

inline QString PopoverDetail(QWidget& window) {
    auto* detail = PopoverOf(window).findChild<QLabel*>("popover_detail");
    REQUIRE(detail != nullptr);
    return detail->text();
}

inline void PressPopover(QWidget& window, const QString& button) {
    auto* pressed = PopoverOf(window).findChild<QPushButton*>("popover_" + button);
    REQUIRE(pressed != nullptr);
    pressed->click();
    QApplication::processEvents();
}

inline QStringList NoticeTexts(QWidget& window) {
    QStringList said;
    for (const QLabel* text : window.findChildren<QLabel*>("notice_text"))
        said.append(text->text());
    return said;
}

inline QString LastNotice(QWidget& window) {
    const QStringList said = NoticeTexts(window);
    return said.isEmpty() ? QString() : said.back();
}

inline QString FrameStatus(QWidget& window) {
    auto* status = window.findChild<QLabel*>("frame_status");
    REQUIRE(status != nullptr);
    return status->text();
}

inline Editor::Commands& CommandsOf(QWidget& window) {
    auto* commands = window.findChild<Editor::Commands*>("commands");
    REQUIRE(commands != nullptr);
    return *commands;
}

inline QString CommandFor(QWidget& window, const QKeySequence& keys) {
    Editor::Commands& commands = CommandsOf(window);
    for (const QString& id : commands.Ids()) {
        if (commands.Action(id)->shortcut() == keys) return id;
    }
    return {};
}

inline QStringList RunCommand(QWidget& window, const QString& id) {
    Editor::Commands& commands = CommandsOf(window);
    if (const std::optional<QString> refused = commands.Refusal(id)) {
        CHECK_FALSE(commands.Run(id));
        return {*refused};
    }
    Script run({});
    commands.Run(id);
    QApplication::processEvents();
    return run.Problems();
}

inline QString RefusalOf(QWidget& window, const QString& id) {
    Editor::Commands& commands = CommandsOf(window);
    if (const std::optional<QString> refused = commands.Refusal(id)) {
        CHECK_FALSE(commands.Run(id));
        return *refused;
    }
    Script refused({});
    commands.Run(id);
    REQUIRE(Settle([&refused] { return !refused.Problems().isEmpty(); }));
    return refused.Problems().front();
}

inline constexpr int kWindowWidth = 1600;
inline constexpr int kWindowHeight = 1000;
inline constexpr int kFrameRoom = 600;
inline constexpr int kTimelineRoom = 200;

struct WithoutHost {
    WithoutHost() { QSettings().remove("game/directory"); }
};

struct Opened : WithoutHost {
    QTemporaryDir dir;
    Editor::Window window;
    QTreeWidget* tree = nullptr;
    QTableWidget* inspector = nullptr;
};

inline void WidenTimeline(Editor::Timeline& timeline) {
    const int wide = Editor::kGutterWidth + kFrameRoom;
    QWidget* held = timeline.parentWidget();
    QWidget* area = held != nullptr ? held->parentWidget() : nullptr;
    if (auto* scrolled = qobject_cast<QScrollArea*>(area)) scrolled->resize(wide, kTimelineRoom);
    QApplication::processEvents();
    if (timeline.width() < wide) {
        if (held != nullptr) held->resize(wide, kTimelineRoom);
        timeline.resize(wide, kTimelineRoom);
        QApplication::processEvents();
    }
}

inline constexpr int kOffScreen = -32000;

inline void ShowOffScreen(Editor::Window& window) {
    window.setWindowFlag(Qt::Tool);
    window.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    window.setAttribute(Qt::WA_ShowWithoutActivating);
    window.resize(kWindowWidth, kWindowHeight);
    window.move(kOffScreen, kOffScreen);
    window.show();
    window.move(kOffScreen, kOffScreen);
    QApplication::processEvents();
}

inline void WaitForOpen(Editor::Window& window) {
    REQUIRE(Settle([&window] { return !window.Loading(); }));
    QApplication::processEvents();
}

inline void Open(Opened& opened, bool with_image = false) {
    REQUIRE(opened.dir.isValid());
    opened.window.resize(kWindowWidth, kWindowHeight);
    opened.window.OpenDocument(WritePackage(opened.dir, with_image));
    WaitForOpen(opened.window);
    opened.tree = opened.window.findChild<QTreeWidget*>("package");
    opened.inspector = opened.window.findChild<QTableWidget*>();
    REQUIRE(opened.tree != nullptr);
    REQUIRE(opened.inspector != nullptr);
}

inline void RunMenu(Script& script, QTreeWidget& tree) {
    emit tree.customContextMenuRequested(QPoint(4, 4));
    REQUIRE(Settle([&script] { return script.Finished(); }));
}

struct TitleFile {
    Document::File file;
    std::string path;
};

inline std::optional<TitleFile> ReadTitle(const QString& title) {
    QFile read(title);
    REQUIRE(read.open(QIODevice::ReadOnly));
    const QByteArray bytes = read.readAll();
    auto file = Document::File::Open(
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
    return TitleFile{.file = std::move(*file), .path = path};
}

inline QString OwnDroppedDot(Opened& opened) {
    auto* timeline = opened.window.findChild<Editor::Timeline*>();
    auto* viewport = opened.window.findChild<Editor::Viewport*>();
    auto* library = opened.window.findChild<QTreeWidget*>("library");
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(library != nullptr);
    std::optional<uint16_t> dot;
    for (int at = 0; at < library->topLevelItemCount(); at++) {
        if (library->topLevelItem(at)->text(0).contains("dot"))
            dot = static_cast<uint16_t>(library->topLevelItem(at)->data(0, Qt::UserRole).toUInt());
    }
    REQUIRE(dot.has_value());
    emit timeline->FrameChosen(0);
    {
        Script placed({});
        emit viewport->CharacterDropped(dot.value_or(0), 500, 300);
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
        CHECK(owned.Problems().isEmpty());
    }
    return folder;
}

inline std::optional<uint16_t> WidestTitleDepth(const QString& title) {
    const std::optional<TitleFile> read = ReadTitle(title);
    if (!read) return std::nullopt;
    const auto animation = read->file.ReadAnimation(read->path);
    REQUIRE(animation.has_value());
    if (!animation) return std::nullopt;
    const auto outlines =
        Document::StageOutlines(*animation, {}, kPreviewFrame, read->file.ShapeBounds(read->path));
    REQUIRE(!outlines.empty());
    const auto widest = std::ranges::max_element(outlines, {}, [](const auto& outline) {
        return std::abs(outline.corners[1][0] - outline.corners[0][0]);
    });
    return widest->depth;
}

inline std::optional<uint16_t> FirstSpriteDepth(const QString& title) {
    const std::optional<TitleFile> read = ReadTitle(title);
    if (!read) return std::nullopt;
    const auto animation = read->file.ReadAnimation(read->path);
    REQUIRE(animation.has_value());
    if (!animation) return std::nullopt;
    const std::vector<Document::ClipSummary> clips = Document::Clips(*animation);
    REQUIRE(clips.size() > 1);
    const AfpAnimation::Container* sprite = Document::FindClip(*animation, clips.at(1).id);
    REQUIRE(sprite != nullptr);
    for (const Document::DepthRow& row : Document::DepthRows(*sprite)) {
        if (!row.spans.empty() && row.spans.front().first_frame == 0) return row.depth;
    }
    return std::nullopt;
}

}
