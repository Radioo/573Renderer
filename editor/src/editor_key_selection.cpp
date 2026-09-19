#include "editor_window.h"

#include "editor_timeline.h"

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/key_simplify.h"
#include "support/expected.h"

#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QStatusBar>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

bool Window::EditOwned(const QString& name, const OwnedChange& change) {
    if (!file_ || animation_path_.empty()) return false;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return false;
    }
    return EditAuthored(name, [&animation, &change](Document::AuthoredDepth& authored) {
        using Changed = Support::Expected<void, std::string>;
        const auto baked = Document::BakedFor(*animation, authored);
        if (!baked) return Changed(Support::Unexpected(baked.error()));
        return change(authored, *baked);
    });
}

void Window::FocusKey(const QString& property, uint32_t frame) {
    key_property_ = property;
    key_frame_ = frame;
    ShowFrame();
}

void Window::AddKeyActions() {
    const auto add = [this](const QString& text, const QKeySequence& keys, auto slot) {
        auto* action = new QAction(text, timeline_);
        action->setShortcut(keys);
        action->setShortcutContext(Qt::WidgetShortcut);
        connect(action, &QAction::triggered, this, slot);
        timeline_->addAction(action);
    };
    add(tr("Copy"), QKeySequence::Copy, &Window::CopySelection);
    add(tr("Cut"), QKeySequence::Cut, &Window::CutSelection);
    add(tr("Paste"), QKeySequence::Paste, &Window::PasteClipboard);
    add(tr("Delete keyframes"), QKeySequence::Delete, &Window::RemoveSelectedKeys);
    add(tr("Select every keyframe"), QKeySequence::SelectAll, &Window::SelectAllKeys);
    add(tr("Toggle hold"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H),
        &Window::ToggleHoldSelectedKeys);
    add(tr("Easy ease"), QKeySequence(Qt::Key_F9),
        [this] { EasyEaseSelectedKeys(Document::EasySide::Both); });
    add(tr("Easy ease in"), QKeySequence(Qt::SHIFT | Qt::Key_F9),
        [this] { EasyEaseSelectedKeys(Document::EasySide::In); });
    add(tr("Easy ease out"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9),
        [this] { EasyEaseSelectedKeys(Document::EasySide::Out); });
}

bool Window::CopySelectedKeys() {
    if (!depth_) return false;
    const Document::AuthoredDepth* owned = AuthoredAt(static_cast<uint16_t>(*depth_), frame_);
    if (owned == nullptr) return false;
    const auto copied = Document::CopyKeys(*owned, timeline_->SelectedKeys());
    if (!copied) {
        ReportProblem(QString::fromStdString(copied.error()));
        return false;
    }
    copied_keys_ = *copied;
    keys_copied_last_ = true;
    statusBar()->showMessage(
        tr("Copied %n keyframe(s)", nullptr, static_cast<int>(timeline_->SelectedKeys().size())));
    return true;
}

void Window::CopySelection() {
    if (!timeline_->SelectedKeys().empty()) {
        CopySelectedKeys();
        return;
    }
    if (depth_) CopySpanAt(static_cast<uint16_t>(*depth_), frame_);
}

void Window::CutSelection() {
    if (!timeline_->SelectedKeys().empty()) {
        if (CopySelectedKeys()) RemoveSelectedKeys();
        return;
    }
    if (depth_ && CopySpanAt(static_cast<uint16_t>(*depth_), frame_)) RemoveChosenDepths(frame_);
}

void Window::PasteClipboard() {
    if (keys_copied_last_) {
        PasteCopiedKeys();
        return;
    }
    PasteSpanAt(frame_);
}

void Window::PasteCopiedKeys() {
    if (!copied_keys_ || !depth_) return;
    const Document::KeyClip copied = *copied_keys_;
    const uint32_t frame = frame_;
    std::vector<Document::KeyRef> pasted;
    if (!EditOwned(tr("Paste keyframes at frame %1").arg(frame),
                   [&copied, frame, &pasted](Document::AuthoredDepth& authored,
                                             const Document::BakedDepth& baked) {
                       using Changed = Support::Expected<void, std::string>;
                       auto placed = Document::PasteKeys(authored, baked, copied, frame);
                       if (!placed) return Changed(Support::Unexpected(placed.error()));
                       pasted = std::move(*placed);
                       return Changed();
                   })) {
        return;
    }
    timeline_->SelectKeys(std::move(pasted));
}

void Window::RemoveSelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    if (chosen.empty()) {
        RemoveChosenDepths(frame_);
        return;
    }
    if (!EditAuthored(tr("Delete %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                      [&chosen](Document::AuthoredDepth& authored) {
                          return Document::RemoveKeys(authored, chosen);
                      })) {
        return;
    }
    key_frame_.reset();
    timeline_->SelectKeys({});
    ShowFrame();
}

void Window::SelectAllKeys() {
    if (!depth_) return;
    const Document::AuthoredDepth* owned = AuthoredAt(static_cast<uint16_t>(*depth_), frame_);
    if (owned != nullptr) timeline_->SelectKeys(Document::AllKeys(*owned));
}

void Window::ToggleHoldSelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    EditAuthored(tr("Toggle hold on %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                 [&chosen](Document::AuthoredDepth& authored) {
                     return Document::ToggleHoldKeys(authored, chosen);
                 });
}

void Window::EasyEaseSelectedKeys(Document::EasySide side) {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    EditAuthored(tr("Easy ease %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                 [&chosen, side](Document::AuthoredDepth& authored) {
                     return Document::EasyEaseKeys(authored, chosen, side);
                 });
}

void Window::ReverseSelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    std::vector<Document::KeyRef> reversed;
    if (!EditAuthored(tr("Time-reverse %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                      [&chosen, &reversed](Document::AuthoredDepth& authored) {
                          using Changed = Support::Expected<void, std::string>;
                          auto flipped = Document::ReverseKeys(authored, chosen);
                          if (!flipped) return Changed(Support::Unexpected(flipped.error()));
                          reversed = std::move(*flipped);
                          return Changed();
                      })) {
        return;
    }
    SelectMovedKeys(chosen, std::move(reversed));
}

void Window::StretchSelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    if (chosen.empty()) return;
    bool accepted = false;
    const int percent = QInputDialog::getInt(this, tr("Time-stretch keyframes"),
                                             tr("Stretch factor (%)"), 100, 1, 10000, 1, &accepted);
    if (!accepted) return;
    StretchSelectedKeysBy(
        Document::KeyStretch{.anchor = std::ranges::min(chosen, {}, &Document::KeyRef::frame).frame,
                             .scale = percent,
                             .over = 100});
}

void Window::StretchSelectedKeysBy(const Document::KeyStretch& stretch) {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    std::vector<Document::KeyRef> stretched;
    if (!EditAuthored(tr("Time-stretch %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                      [&chosen, &stretch, &stretched](Document::AuthoredDepth& authored) {
                          using Changed = Support::Expected<void, std::string>;
                          auto placed = Document::StretchKeys(authored, chosen, stretch);
                          if (!placed) return Changed(Support::Unexpected(placed.error()));
                          stretched = std::move(*placed);
                          return Changed();
                      })) {
        return;
    }
    SelectMovedKeys(chosen, std::move(stretched));
}

void Window::SimplifySelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    bool accepted = false;
    const int tolerance =
        QInputDialog::getInt(this, tr("Simplify keyframes"),
                             tr("Largest change allowed on any frame, in the property's own units"),
                             0, 0, 1000000, 1, &accepted);
    if (!accepted) return;
    std::vector<Document::KeyRef> remaining;
    if (!EditAuthored(tr("Simplify %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                      [&chosen, tolerance, &remaining](Document::AuthoredDepth& authored) {
                          using Changed = Support::Expected<void, std::string>;
                          auto kept = Document::SimplifyKeys(authored, chosen, tolerance);
                          if (!kept) return Changed(Support::Unexpected(kept.error()));
                          remaining = std::move(*kept);
                          return Changed();
                      })) {
        return;
    }
    const std::size_t removed = chosen.size() - remaining.size();
    statusBar()->showMessage(tr("%n keyframe(s) removed", nullptr, static_cast<int>(removed)));
    ShowFrame();
}

void Window::SelectMovedKeys(const std::vector<Document::KeyRef>& chosen,
                             std::vector<Document::KeyRef> moved) {
    if (key_frame_) {
        const Document::KeyRef focused{.property = key_property_.toStdString(),
                                       .frame = *key_frame_};
        const auto at = std::ranges::find(chosen, focused);
        if (at != chosen.end())
            key_frame_ = moved.at(static_cast<std::size_t>(at - chosen.begin())).frame;
    }
    timeline_->SelectKeys(std::move(moved));
    ShowFrame();
}

void Window::ShiftSelectedKeys(int64_t by) {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    if (chosen.empty()) return;
    std::vector<Document::KeyRef> shifted;
    if (!EditAuthored(tr("Move %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                      [&chosen, by, &shifted](Document::AuthoredDepth& authored) {
                          using Changed = Support::Expected<void, std::string>;
                          auto moved = Document::ShiftKeys(authored, chosen, by);
                          if (!moved) return Changed(Support::Unexpected(moved.error()));
                          shifted = std::move(*moved);
                          return Changed();
                      })) {
        return;
    }
    const bool focused_moved =
        key_frame_ &&
        std::ranges::find(chosen, Document::KeyRef{.property = key_property_.toStdString(),
                                                   .frame = *key_frame_}) != chosen.end();
    if (focused_moved) key_frame_ = static_cast<uint32_t>(static_cast<int64_t>(*key_frame_) + by);
    timeline_->SelectKeys(std::move(shifted));
    ShowFrame();
}

}
