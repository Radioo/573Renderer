#include "editor_window.h"

#include "editor_timeline.h"

#include "document/authored.h"
#include "document/key_selection.h"
#include "support/expected.h"

#include <QAction>
#include <QKeySequence>
#include <QStatusBar>
#include <QString>

#include <algorithm>
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
    add(tr("Copy keyframes"), QKeySequence::Copy, &Window::CopySelectedKeys);
    add(tr("Paste keyframes"), QKeySequence::Paste, &Window::PasteCopiedKeys);
    add(tr("Delete keyframes"), QKeySequence::Delete, &Window::RemoveSelectedKeys);
    add(tr("Select every keyframe"), QKeySequence::SelectAll, &Window::SelectAllKeys);
}

void Window::CopySelectedKeys() {
    if (!depth_) return;
    const Document::AuthoredDepth* owned = AuthoredAt(static_cast<uint16_t>(*depth_), frame_);
    if (owned == nullptr) return;
    const auto copied = Document::CopyKeys(*owned, timeline_->SelectedKeys());
    if (!copied) {
        ReportProblem(QString::fromStdString(copied.error()));
        return;
    }
    copied_keys_ = *copied;
    statusBar()->showMessage(
        tr("Copied %n keyframe(s)", nullptr, static_cast<int>(timeline_->SelectedKeys().size())));
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
