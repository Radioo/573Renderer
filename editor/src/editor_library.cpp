#include "editor_window.h"

#include "editor_filter.h"
#include "editor_icons.h"
#include "editor_panel_tabs.h"
#include "editor_rows.h"
#include "editor_theme.h"
#include "editor_mime.h"

#include "document/characters.h"
#include "document/clip.h"
#include "document/clip_edit.h"
#include "document/document.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/span_tags.h"
#include "document/stage_move.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QAction>
#include <QAbstractItemView>
#include <QBrush>
#include <QByteArray>
#include <QList>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QToolButton>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QKeyEvent>
#include <QSize>
#include <QLineEdit>
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
#include <limits>
#include <tuple>
#include <functional>
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

    std::function<void(const QTreeWidgetItem*)> on_return;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        const bool entered = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
        if (!entered || currentItem() == nullptr || !on_return) {
            QTreeWidget::keyPressEvent(event);
            return;
        }
        on_return(currentItem());
    }
};

constexpr int kTileSide = 32;
constexpr int kChipHeight = 22;
constexpr int kPanelIcon = 16;
constexpr int kPanelButton = 26;

std::optional<uint16_t> SpriteOf(const QTreeWidgetItem* item) {
    if (item == nullptr) return std::nullopt;
    const auto kind = static_cast<Document::CharacterKind>(item->data(0, kKindRole).toInt());
    if (kind != Document::CharacterKind::Sprite) return std::nullopt;
    return static_cast<uint16_t>(item->data(0, kIdRole).toUInt());
}

}

QWidget* Window::BuildLibraryPanel() {
    QTreeWidget* library = BuildLibrary();
    library_kinds_ = {Document::CharacterKind::Sprite, Document::CharacterKind::Image,
                      Document::CharacterKind::Shape, Document::CharacterKind::Imported};
    auto* kinds = new QWidget;
    auto* row = new QHBoxLayout(kinds);
    row->setContentsMargins(8, 6, 8, 2);
    row->setSpacing(4);
    for (const auto& [name, text, kind] :
         {std::tuple{QStringLiteral("library_sprites"), tr("Sprites"),
                     Document::CharacterKind::Sprite},
          std::tuple{QStringLiteral("library_images"), tr("Images"),
                     Document::CharacterKind::Image},
          std::tuple{QStringLiteral("library_shapes"), tr("Shapes"),
                     Document::CharacterKind::Shape},
          std::tuple{QStringLiteral("library_imported"), tr("Imported"),
                     Document::CharacterKind::Imported}}) {
        auto* button = new QToolButton;
        button->setObjectName(name);
        button->setProperty("chip", true);
        button->setText(text);
        button->setFixedHeight(kChipHeight);
        button->setCheckable(true);
        button->setChecked(true);
        connect(button, &QToolButton::toggled, this, [this, kind](bool on) {
            std::erase(library_kinds_, kind);
            if (on) library_kinds_.push_back(kind);
            ShowLibraryKinds();
        });
        row->addWidget(button);
    }
    row->addStretch();

    auto* inside = new QWidget;
    auto* layout = new QVBoxLayout(inside);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(kinds);
    layout->addWidget(WithFilter(library, library_filter_ = new QLineEdit, tr("Filter characters")),
                      1);

    auto* corner = new QWidget;
    auto* beside = new QHBoxLayout(corner);
    beside->setContentsMargins(0, 0, 4, 0);
    beside->setSpacing(6);
    library_of_ = new QLabel;
    library_of_->setObjectName("library_of");
    beside->addWidget(library_of_);
    auto* fresh = new QToolButton;
    fresh->setObjectName("library_add");
    fresh->setProperty("panel_icon", true);
    fresh->setIcon(Icons::Of(Icons::Glyph::Plus, Theme::kSoft, kPanelIcon));
    fresh->setIconSize(QSize(kPanelIcon, kPanelIcon));
    fresh->setFixedSize(kPanelButton, kPanelButton);
    fresh->setToolTip(tr("New empty sprite"));
    connect(fresh, &QToolButton::clicked, this, [this] { NewEmptySprite(); });
    beside->addWidget(fresh);

    auto* panel = new PanelTabs;
    panel->setObjectName("library_tabs");
    panel->addTab(inside, tr("Library"));
    panel->setCornerWidget(corner, Qt::TopRightCorner);
    return panel;
}

QTreeWidget* Window::BuildLibrary() {
    auto* tree = new LibraryTree;
    library_ = tree;
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
    tree->on_return = [this](const QTreeWidgetItem* item) {
        PlaceLibraryCharacter(static_cast<uint16_t>(item->data(0, kIdRole).toUInt()));
    };
    library_->setHeaderHidden(true);
    library_->setColumnHidden(1, true);
    library_->setMouseTracking(true);
    library_->setItemDelegate(new Rows::Delegate(library_));
    library_->setIconSize(QSize(Rows::kThumbWidth, Rows::kThumbHeight));
    return library_;
}

void Window::PlaceLibraryCharacter(uint16_t character) {
    if (!file_ || animation_path_.empty()) return;
    const uint32_t frames = ClipFrameCount();
    if (frames == 0) return;
    const std::optional<uint16_t> depth = NextFreeDepth(0);
    if (!depth) return;
    AddCharacterDepth(*depth, character, frame_, frames - 1);
}

void Window::FillLibrary(const std::vector<LibraryRow>& characters) {
    library_->clear();
    const QBrush unused = library_->palette().brush(QPalette::Disabled, QPalette::Text);
    for (const LibraryRow& one : characters) {
        auto* item = new QTreeWidgetItem(library_, {one.label, QString::number(one.uses)});
        item->setData(0, kIdRole, one.id);
        item->setData(0, kKindRole, static_cast<int>(one.kind));
        if (!one.tile.isNull()) item->setIcon(0, QIcon(QPixmap::fromImage(one.tile)));
        const QString says = one.uses == 0   ? tr("unused")
                             : one.uses == 1 ? tr("used once")
                                             : tr("used %1 times").arg(one.uses);
        item->setData(0, Rows::kDetailRole, says);
        if (one.uses > 0) continue;
        item->setForeground(0, unused);
        item->setForeground(1, unused);
    }
    const QString of = animation_name_.empty()
                           ? QString()
                           : tr("of %1").arg(QString::fromStdString(animation_name_));
    library_of_->setText(of);
    library_of_->setFixedWidth(library_of_->fontMetrics().horizontalAdvance(of));
    ShowLibraryKinds();
}

void Window::ShowLibraryKinds() {
    ApplyFilter(*library_, library_filter_->text());
    for (int at = 0; at < library_->topLevelItemCount(); at++) {
        QTreeWidgetItem* item = library_->topLevelItem(at);
        const auto kind = static_cast<Document::CharacterKind>(item->data(0, kKindRole).toInt());
        if (std::ranges::find(library_kinds_, kind) == library_kinds_.end()) item->setHidden(true);
    }
}

void Window::ShowLibrarySprite(uint16_t sprite) {
    ChooseClip(ClipIndexOf(Document::ClipId{.sprite = sprite}));
}

void Window::ShowLibraryMenu(const QPoint& where) {
    if (!file_ || animation_path_.empty()) return;
    const QTreeWidgetItem* item = library_->currentItem();
    const auto character =
        item != nullptr ? static_cast<uint16_t>(item->data(0, kIdRole).toUInt()) : uint16_t{0};
    const std::optional<uint16_t> sprite = item != nullptr ? SpriteOf(item) : std::nullopt;
    QMenu menu(this);
    QAction* place = item != nullptr
                         ? menu.addAction(tr("Place on a new depth from frame %1...").arg(frame_))
                         : nullptr;
    QAction* show = sprite ? menu.addAction(tr("Show this sprite on its own")) : nullptr;
    QAction* duplicate = sprite ? menu.addAction(tr("Duplicate this sprite")) : nullptr;
    QAction* use =
        item != nullptr && depth_
            ? menu.addAction(tr("Use on depth %1 from frame %2").arg(*depth_).arg(frame_))
            : nullptr;
    menu.addSeparator();
    QAction* fresh = menu.addAction(tr("New empty sprite"));
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;
    if (chosen == fresh) {
        NewEmptySprite();
        return;
    }
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

void Window::NewEmptySprite() {
    const int frames = std::max(1, static_cast<int>(ClipFrameCount()));
    std::optional<uint16_t> made;
    if (!EditAnimation(tr("New sprite of %n frame(s)", nullptr, frames),
                       [frames, &made](AfpAnimation::Animation& edited) {
                           using Made = Support::Expected<void, std::string>;
                           auto sprite = Document::NewSprite(edited, static_cast<uint32_t>(frames));
                           if (!sprite) return Made(Support::Unexpected(sprite.error()));
                           made = *sprite;
                           return Made();
                       }) ||
        !made) {
        return;
    }
    FillClips();
    ShowLibrarySprite(*made);
}

void Window::ReplaceCharacterOnDepth() {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    const std::vector<Document::CharacterSummary> characters =
        Document::Characters(*animation, file_->ShapeImages(animation_path_));
    if (characters.empty()) {
        ReportProblem(tr("This animation has nothing to place"));
        return;
    }
    QStringList labels;
    for (const Document::CharacterSummary& one : characters)
        labels.append(QString::fromStdString(one.label));
    bool answered = false;
    const QString picked = QInputDialog::getItem(this, tr("Replace the character"), tr("Place"),
                                                 labels, 0, false, &answered);
    const auto index = labels.indexOf(picked);
    if (!answered || index < 0) return;
    UseCharacterOnDepth(characters[static_cast<std::size_t>(index)].id,
                        static_cast<uint16_t>(*depth_));
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

void Window::DropCharacterOnTimeline(uint16_t character, uint32_t frame,
                                     std::optional<uint16_t> row) {
    if (!file_ || animation_path_.empty()) return;
    const uint32_t frames = ClipFrameCount();
    if (frames == 0) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    const AfpAnimation::Container* shown = Document::FindClip(*animation, clip_);
    const bool row_free =
        row && shown != nullptr && Document::CheckFree(*shown, *row, frame, frames - 1).has_value();
    const std::optional<uint16_t> depth = row_free ? row : NextFreeDepth(0);
    if (!depth) return;
    AddCharacterDepth(*depth, character, frame, frames - 1);
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
