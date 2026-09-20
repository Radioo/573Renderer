#include "editor_window.h"

#include "editor_popover.h"
#include "editor_selection_bar.h"
#include "editor_timeline.h"

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/key_simplify.h"
#include "document/key_wiggle.h"
#include "support/expected.h"

#include <QAction>
#include <QKeySequence>
#include <QRandomGenerator>
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
    const uint32_t anchor = std::ranges::min(chosen, {}, &Document::KeyRef::frame).frame;
    popover_->Ask(PopoverAsk{.title = tr("Time-stretch %1 keyframes").arg(chosen.size()),
                             .apply = tr("Stretch"),
                             .fields = {PopoverField{.label = tr("Stretch to"),
                                                     .value = 100,
                                                     .lowest = 1,
                                                     .highest = 10000,
                                                     .suffix = tr(" %")}},
                             .describe = {},
                             .preview = {},
                             .run =
                                 [this, anchor](const PopoverValues& values) {
                                     StretchSelectedKeysBy(Document::KeyStretch{
                                         .anchor = anchor,
                                         .scale = static_cast<int64_t>(values.at(0)),
                                         .over = 100});
                                 },
                             .cancelled = {}},
                  BarAnchor("key.stretch"));
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

std::optional<std::size_t> Window::KeysAfterSimplify(const std::vector<Document::KeyRef>& chosen,
                                                     int64_t tolerance) const {
    if (!depth_) return std::nullopt;
    const std::optional<std::size_t> at = AuthoredIndexAt(static_cast<uint16_t>(*depth_), frame_);
    if (!at) return std::nullopt;
    Document::AuthoredDepth copy = authored_[*at];
    auto kept = Document::SimplifyKeys(copy, chosen, tolerance);
    if (!kept) return std::nullopt;
    return kept->size();
}

void Window::SimplifySelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    if (chosen.empty()) return;
    popover_->Ask(
        PopoverAsk{
            .title = tr("Simplify %1 keyframes").arg(chosen.size()),
            .apply = tr("Simplify"),
            .fields = {PopoverField{.label = tr("Largest change allowed"),
                                    .value = 0,
                                    .lowest = 0,
                                    .highest = 1000000}},
            .describe =
                [this, chosen](const PopoverValues& values) {
                    const std::optional<std::size_t> kept =
                        KeysAfterSimplify(chosen, static_cast<int64_t>(values.at(0)));
                    if (!kept) return QString();
                    return tr("%1 of %2 keyframes go")
                        .arg(chosen.size() - *kept)
                        .arg(chosen.size());
                },
            .preview =
                [this, chosen](const PopoverValues& values) {
                    PreviewAuthored([&chosen, &values](Document::AuthoredDepth& authored) {
                        using Changed = Support::Expected<void, std::string>;
                        auto kept = Document::SimplifyKeys(authored, chosen,
                                                           static_cast<int64_t>(values.at(0)));
                        if (!kept) return Changed(Support::Unexpected(kept.error()));
                        return Changed();
                    });
                },
            .run =
                [this, chosen](const PopoverValues& values) {
                    std::vector<Document::KeyRef> remaining;
                    if (!EditAuthored(
                            tr("Simplify %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                            [&chosen, &values, &remaining](Document::AuthoredDepth& authored) {
                                using Changed = Support::Expected<void, std::string>;
                                auto kept = Document::SimplifyKeys(
                                    authored, chosen, static_cast<int64_t>(values.at(0)));
                                if (!kept) return Changed(Support::Unexpected(kept.error()));
                                remaining = std::move(*kept);
                                return Changed();
                            })) {
                        return;
                    }
                    const std::size_t removed = chosen.size() - remaining.size();
                    ShowResult(tr("%n keyframe(s) removed", nullptr, static_cast<int>(removed)),
                               true);
                    ShowFrame();
                },
            .cancelled = [this, chosen] { CancelPreview(chosen); }},
        BarAnchor("key.simplify"));
}

namespace {

Document::Wiggle WiggleFrom(const PopoverValues& values, uint32_t seed) {
    return Document::Wiggle{.every = static_cast<uint32_t>(values.at(0)),
                            .magnitude = static_cast<int64_t>(values.at(1)),
                            .seed = seed};
}

}

void Window::WiggleSelectedKeys() {
    const std::vector<Document::KeyRef> chosen = timeline_->SelectedKeys();
    if (chosen.empty()) return;
    const uint32_t seed = QRandomGenerator::global()->generate();
    popover_->Ask(
        PopoverAsk{
            .title = tr("Wiggle %1 keyframes").arg(chosen.size()),
            .apply = tr("Wiggle"),
            .fields = {PopoverField{.label = tr("A keyframe every"),
                                    .value = 2,
                                    .lowest = 1,
                                    .highest = 10000,
                                    .suffix = tr(" frames")},
                       PopoverField{.label = tr("Largest change"),
                                    .value = 20,
                                    .lowest = 1,
                                    .highest = 1000000}},
            .describe = {},
            .preview =
                [this, chosen, seed](const PopoverValues& values) {
                    PreviewAuthored([&chosen, &values, seed](Document::AuthoredDepth& authored) {
                        using Changed = Support::Expected<void, std::string>;
                        auto placed =
                            Document::WiggleKeys(authored, chosen, WiggleFrom(values, seed));
                        if (!placed) return Changed(Support::Unexpected(placed.error()));
                        return Changed();
                    });
                },
            .run =
                [this, chosen, seed](const PopoverValues& values) {
                    std::vector<Document::KeyRef> wiggled;
                    if (!EditAuthored(
                            tr("Wiggle %n keyframe(s)", nullptr, static_cast<int>(chosen.size())),
                            [&chosen, &values, seed, &wiggled](Document::AuthoredDepth& authored) {
                                using Changed = Support::Expected<void, std::string>;
                                auto placed = Document::WiggleKeys(authored, chosen,
                                                                   WiggleFrom(values, seed));
                                if (!placed) return Changed(Support::Unexpected(placed.error()));
                                wiggled = std::move(*placed);
                                return Changed();
                            })) {
                        return;
                    }
                    timeline_->SelectKeys(std::move(wiggled));
                    ShowFrame();
                },
            .cancelled = [this, chosen] { CancelPreview(chosen); }},
        BarAnchor("key.wiggle"));
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
