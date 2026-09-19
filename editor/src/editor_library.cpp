#include "editor_window.h"

#include "editor_filter.h"
#include "editor_mime.h"

#include "document/characters.h"
#include "document/clip_edit.h"
#include "document/document.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/stage_move.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QAction>
#include <QAbstractItemView>
#include <QBrush>
#include <QByteArray>
#include <QList>
#include <QMimeData>
#include <QComboBox>
#include <QInputDialog>
#include <QMenu>
#include <QPalette>
#include <QPoint>
#include <QStatusBar>
#include <QString>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Editor {

namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kKindRole = Qt::UserRole + 1;

class LibraryTree : public QTreeWidget {
public:
    [[nodiscard]] QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        if (items.isEmpty()) return nullptr;
        auto* data = new QMimeData;
        data->setData(kCharacterMime, QByteArray::number(items.front()->data(0, kIdRole).toUInt()));
        return data;
    }
};

std::optional<uint16_t> SpriteOf(const QTreeWidgetItem* item) {
    if (item == nullptr) return std::nullopt;
    const auto kind = static_cast<Document::CharacterKind>(item->data(0, kKindRole).toInt());
    if (kind != Document::CharacterKind::Sprite) return std::nullopt;
    return static_cast<uint16_t>(item->data(0, kIdRole).toUInt());
}

}

QTreeWidget* Window::BuildLibrary() {
    library_ = new LibraryTree;
    library_->setObjectName("library");
    library_->setHeaderLabels({tr("Character"), tr("Uses")});
    library_->setRootIsDecorated(false);
    library_->setDragEnabled(true);
    library_->setDragDropMode(QAbstractItemView::DragOnly);
    library_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(library_, &QTreeWidget::itemDoubleClicked, this, [this](const QTreeWidgetItem* item) {
        const std::optional<uint16_t> sprite = SpriteOf(item);
        if (sprite) QTimer::singleShot(0, this, [this, id = *sprite] { ShowLibrarySprite(id); });
    });
    connect(library_, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& at) { ShowLibraryMenu(library_->mapToGlobal(at)); });
    return library_;
}

void Window::FillLibrary(const AfpAnimation::Animation& animation,
                         const std::vector<Document::CharacterSummary>& characters) {
    library_->clear();
    const std::map<uint16_t, std::size_t> uses = Document::CharacterUses(animation);
    const QBrush unused = library_->palette().brush(QPalette::Disabled, QPalette::Text);
    for (const Document::CharacterSummary& one : characters) {
        const auto found = uses.find(one.id);
        const std::size_t count = found == uses.end() ? 0 : found->second;
        auto* item = new QTreeWidgetItem(
            library_, {QString::fromStdString(one.label), QString::number(count)});
        item->setData(0, kIdRole, one.id);
        item->setData(0, kKindRole, static_cast<int>(one.kind));
        if (count == 0) {
            item->setForeground(0, unused);
            item->setForeground(1, unused);
        }
    }
    library_->resizeColumnToContents(0);
    ApplyFilter(*library_, library_filter_->text());
}

void Window::ShowLibrarySprite(uint16_t sprite) {
    const int index = clip_box_->findData(QVariant(static_cast<int>(sprite)));
    if (index >= 0) clip_box_->setCurrentIndex(index);
}

void Window::ShowLibraryMenu(const QPoint& where) {
    const QTreeWidgetItem* item = library_->currentItem();
    if (item == nullptr || !file_ || animation_path_.empty()) return;
    const auto character = static_cast<uint16_t>(item->data(0, kIdRole).toUInt());
    const std::optional<uint16_t> sprite = SpriteOf(item);
    QMenu menu(this);
    QAction* place = menu.addAction(tr("Place on a new depth from frame %1...").arg(frame_));
    QAction* show = sprite ? menu.addAction(tr("Show this sprite on its own")) : nullptr;
    QAction* duplicate = sprite ? menu.addAction(tr("Duplicate this sprite")) : nullptr;
    QAction* use =
        depth_ ? menu.addAction(tr("Use on depth %1 from frame %2").arg(*depth_).arg(frame_))
               : nullptr;
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;
    if (chosen == duplicate && sprite) {
        DuplicateLibrarySprite(*sprite);
        return;
    }
    if (chosen == use && depth_) {
        UseCharacterOnDepth(character, static_cast<uint16_t>(*depth_));
        return;
    }
    if (chosen == show && sprite) {
        ShowLibrarySprite(*sprite);
        return;
    }
    if (chosen != place) return;
    const std::optional<uint16_t> depth = AskForFreeDepth(tr("Place on a depth"), 0);
    if (!depth) return;
    const std::optional<uint32_t> last = AskForLastFrame(frame_);
    if (!last) return;
    AddCharacterDepth(*depth, character, frame_, *last);
}

std::optional<uint32_t> Window::AskForLastFrame(uint32_t first) {
    const auto clip_frames = static_cast<int>(ClipFrameCount());
    bool answered = false;
    const int last = QInputDialog::getInt(
        this, tr("Add a depth"), tr("Last frame"), static_cast<int>(first), static_cast<int>(first),
        std::max(clip_frames - 1, static_cast<int>(first)), 1, &answered);
    if (!answered) return std::nullopt;
    return static_cast<uint32_t>(last);
}

void Window::DuplicateLibrarySprite(uint16_t sprite) {
    std::optional<uint16_t> copy;
    if (!EditAnimation(tr("Duplicate sprite %1").arg(sprite),
                       [sprite, &copy](AfpAnimation::Animation& edited) {
                           using Duplicated = Support::Expected<void, std::string>;
                           auto made = Document::DuplicateSprite(edited, sprite);
                           if (!made) return Duplicated(Support::Unexpected(made.error()));
                           copy = *made;
                           return Duplicated();
                       })) {
        return;
    }
    RefillClipsKeepingChoice();
    if (copy)
        statusBar()->showMessage(tr("Sprite %1 is a copy of sprite %2").arg(*copy).arg(sprite));
}

void Window::UseCharacterOnDepth(uint16_t character, uint16_t depth) {
    const Document::ClipId clip = clip_;
    const uint32_t frame = frame_;
    const std::string value = std::to_string(character);
    EditAnimation(tr("Use character %1 on depth %2").arg(character).arg(depth),
                  [clip, depth, frame, value](AfpAnimation::Animation& edited) {
                      return Document::EditPlacementField(edited, clip, depth, frame, "Character",
                                                          value);
                  });
}

void Window::PlaceDroppedCharacter(uint16_t character, double x, double y) {
    if (!file_ || animation_path_.empty()) return;
    if (!OutlinesMatchView()) {
        ReportProblem(tr("The stage shows the whole animation while a sprite is being edited, so "
                         "a drop there has no place in the sprite. Show the sprite on its own "
                         "first."));
        return;
    }
    const std::optional<uint16_t> depth = NextFreeDepth(0);
    if (!depth) return;
    const uint32_t frames = ClipFrameCount();
    const uint32_t first = frame_;
    const uint32_t last = frames == 0 ? first : std::max(first, frames - 1);
    const Document::ClipId clip = clip_;
    const Document::StageOffset point{.x = x, .y = y};
    if (!EditAnimation(
            tr("Place character %1 on depth %2").arg(character).arg(*depth),
            [clip, depth, character, first, last, point](AfpAnimation::Animation& edited) {
                return Document::PlaceAtPoint(edited, clip, *depth, character, first, last, point);
            })) {
        return;
    }
    depth_ = *depth;
    ShowFrame();
}

void Window::AddCharacterDepth(uint16_t depth, uint16_t character, uint32_t first, uint32_t last) {
    const Document::ClipId clip = clip_;
    if (!EditAnimation(tr("Add depth %1").arg(depth),
                       [clip, depth, character, first, last](AfpAnimation::Animation& edited) {
                           return Document::AddDepth(edited, clip, depth, character, first, last);
                       })) {
        return;
    }
    depth_ = depth;
    ShowFrame();
}

}
