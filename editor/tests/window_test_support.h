#pragma once

#include <catch2/catch_test_macros.hpp>

#include "editor_timeline.h"
#include "editor_viewport.h"
#include "editor_window.h"
#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "document/frame_edit.h"
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

inline Script::Step AnswerNumber(int number) {
    return [number] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr) return false;
        dialog->setIntValue(number);
        dialog->accept();
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

inline QString WritePackage(const QTemporaryDir& dir, bool with_image = false) {
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

struct Opened {
    QTemporaryDir dir;
    Editor::Window window;
    QTreeWidget* tree = nullptr;
    QTableWidget* inspector = nullptr;
};

inline void Open(Opened& opened, bool with_image = false) {
    REQUIRE(opened.dir.isValid());
    opened.window.OpenDocument(WritePackage(opened.dir, with_image));
    opened.tree = opened.window.findChild<QTreeWidget*>("package");
    opened.inspector = opened.window.findChild<QTableWidget*>();
    REQUIRE(opened.tree != nullptr);
    REQUIRE(opened.inspector != nullptr);
}

inline bool Settle(const std::function<bool()>& done) {
    QElapsedTimer waited;
    waited.start();
    while (!done() && waited.elapsed() < kLongestWaitMs)
        QApplication::processEvents();
    return done();
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
