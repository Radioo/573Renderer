#include "editor_window.h"

#include "editor_inspector.h"
#include "editor_selection_bar.h"
#include "editor_timeline.h"

#include "document/authored.h"
#include "document/characters.h"
#include "document/clip.h"
#include "document/inspector.h"
#include "document/clip_edit.h"
#include "document/inspector_view.h"
#include "document/keyframe_edit.h"
#include "document/span_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <QColor>
#include <QColorDialog>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Editor {

namespace {

std::optional<uint16_t> ShownCharacter(const AfpAnimation::Container& clip, uint16_t depth,
                                       uint32_t frame) {
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    const auto row = std::ranges::find(rows, depth, &Document::DepthRow::depth);
    if (row == rows.end()) return std::nullopt;
    std::optional<uint16_t> shown;
    for (const auto& [at, character] : row->shows) {
        if (at <= frame) shown = character;
    }
    return shown;
}

}

void Window::ShowInspectorSubject(const AfpAnimation::Animation& animation) {
    const AfpAnimation::Container* clip = Document::FindClip(animation, clip_);
    if (clip == nullptr || inspector_panel_ == nullptr) return;
    if (!depth_) {
        inspector_panel_->ShowSubject(
            InspectorSubject{.title = QString::fromStdString(animation_name_),
                             .detail = tr("No depth chosen"),
                             .depth_chosen = false,
                             .owned = false});
        inspector_panel_->ShowView(std::nullopt, frame_);
        inspector_panel_->ShowExtras(std::nullopt);
        return;
    }
    const auto depth = static_cast<uint16_t>(*depth_);
    const Document::AuthoredDepth* owned = AuthoredAt(depth, frame_);
    QString detail;
    QString character_name;
    if (const std::optional<uint16_t> character = ShownCharacter(*clip, depth, frame_)) {
        for (const Document::CharacterSummary& one :
             Document::Characters(animation, file_->ShapeImages(animation_path_))) {
            if (one.id == *character) character_name = QString::fromStdString(one.label);
        }
        detail = character_name;
    }
    if (const std::optional<Document::Span> span = Document::SpanOfDepth(*clip, depth, frame_)) {
        const QString frames = tr("frames %1 to %2").arg(span->first_frame).arg(span->last_frame);
        detail = detail.isEmpty() ? frames : detail + ", " + frames;
    }
    inspector_panel_->ShowSubject(InspectorSubject{.title = tr("Depth %1").arg(depth),
                                                   .detail = detail,
                                                   .depth_chosen = true,
                                                   .owned = owned != nullptr});
    inspector_panel_->ShowView(Document::ViewPlacement(*clip, depth, frame_, owned), frame_);
    ShowInspectorExtras(animation, depth, character_name);
}

QWidget* Window::BarAnchor(const QString& id) const {
    if (selection_bar_ == nullptr) return nullptr;
    return selection_bar_->findChild<QWidget*>("bar_" + id);
}

void Window::RefreshSelectionBar() {
    if (selection_bar_ == nullptr) return;
    const std::vector<uint16_t> chosen = SelectedDepths();
    const std::size_t keys = timeline_->SelectedKeys().size();
    if (keys > 0) {
        const QString property =
            key_property_.isEmpty() ? tr("keyframes") : key_property_.toLower();
        selection_bar_->Show(keys == 1 ? tr("1 keyframe, %1").arg(property)
                                       : tr("%1 keyframes, %2").arg(keys).arg(property),
                             tr("frame %1").arg(frame_),
                             {"key.hold", "key.ease", "key.reverse", "key.stretch", "key.wiggle",
                              "key.simplify", "edit.copy", "edit.delete"});
        return;
    }
    if (chosen.size() > 1) {
        selection_bar_->Show(tr("%1 depths chosen").arg(chosen.size()), tr("frame %1").arg(frame_),
                             {"depth.align_left", "depth.align_centre", "depth.align_top",
                              "depth.spread_across", "depth.sequence", "depth.remove"});
        return;
    }
    if (chosen.empty()) {
        selection_bar_->Show(tr("Nothing chosen"), QString(), {});
        return;
    }
    const bool owned = AuthoredAt(chosen.front(), frame_) != nullptr;
    selection_bar_->Show(
        tr("Depth %1").arg(chosen.front()),
        owned ? tr("owned, frame %1").arg(frame_) : tr("baked, frame %1").arg(frame_),
        {"depth.fit", "depth.centre_anchor", "depth.flip_across", "depth.flip_over",
         "depth.forward", "depth.backward", "depth.split", "depth.duplicate", "depth.group",
         owned ? QString("depth.detach") : QString("depth.own"), "depth.remove"});
}

void Window::ShowInspectorExtras(const AfpAnimation::Animation& animation, uint16_t depth,
                                 const QString& character) {
    const Document::Selection selection{.depth = depth,
                                        .frame = frame_,
                                        .owned = AuthoredAt(depth, frame_),
                                        .key_property = {},
                                        .key_frame = std::nullopt,
                                        .clip = clip_};
    PlacementExtras extras{.character = character,
                           .blend = 0,
                           .clip_depth = 0,
                           .filters = {},
                           .owned = selection.owned != nullptr};
    for (const Document::InspectedRow& row : Document::InspectFrame(animation, selection)) {
        const QString name = QString::fromStdString(row.field.name);
        const QString value = QString::fromStdString(row.field.value);
        if (name == "Blend") extras.blend = value.toInt();
        if (name == "Clip depth") extras.clip_depth = value.toInt();
        if (name.startsWith("Filter ") && name.count(' ') == 1)
            extras.filters.append(name + ": " + value);
    }
    inspector_panel_->ShowExtras(extras);
}

void Window::EditPlacementFieldOnDepth(const QString& field, const QString& value) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    const std::string named = field.toStdString();
    const std::string wanted = value.toStdString();
    EditAnimation(tr("%1 on depth %2").arg(field).arg(depth), [clip, depth, frame, &named, &wanted](
                                                                  AfpAnimation::Animation& edited) {
        return Document::EditPlacementField(edited, clip, depth, frame, named, wanted);
    });
}

void Window::ApplyViewEdit(const QString& label, const std::vector<double>& values) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    const std::string wanted = label.toStdString();
    const QString name = tr("%1 of depth %2").arg(label).arg(depth);
    if (AuthoredAt(depth, frame) != nullptr) {
        EditOwned(name, [&wanted, frame, &values](Document::AuthoredDepth& authored,
                                                  const Document::BakedDepth& baked) {
            return Document::SetViewedOwned(authored, baked, frame, wanted, values);
        });
        return;
    }
    EditAnimation(name, [clip, depth, frame, &wanted, &values](AfpAnimation::Animation& edited) {
        return Document::SetViewedBaked(edited, clip, depth, frame, wanted, values);
    });
}

void Window::ToggleViewKeying(const QString& label, bool animated) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const Document::AuthoredDepth* owned = AuthoredAt(depth, frame_);
    if (owned == nullptr) return;
    const std::optional<std::string> track = Document::ViewTrack(*owned, label.toStdString());
    if (!track) return;
    const std::string property = *track;
    const uint32_t frame = frame_;
    if (!animated) {
        EditOwned(
            tr("Animate %1 on depth %2").arg(label).arg(depth),
            [&property](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
                return Document::AddTrack(authored, baked, property);
            });
        return;
    }
    const bool keyed = Document::KeyAt(*owned, property, frame).has_value();
    const QString name = keyed ? tr("Remove the %1 keyframe at frame %2").arg(label).arg(frame)
                               : tr("Add a %1 keyframe at frame %2").arg(label).arg(frame);
    EditAuthored(name, [&property, frame, keyed](Document::AuthoredDepth& authored) {
        return keyed ? Document::RemoveKeyAt(authored, property, frame)
                     : Document::AddKeyAt(authored, property, frame);
    });
}

void Window::PickViewColour(const QString& label) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;
    const AfpAnimation::Container* clip = Document::FindClip(*animation, clip_);
    if (clip == nullptr) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const auto view = Document::ViewPlacement(*clip, depth, frame_, AuthoredAt(depth, frame_));
    if (!view) return;
    const auto row =
        std::ranges::find(view->colours, label.toStdString(), &Document::ViewRow::label);
    if (row == view->colours.end() || row->values.size() != 4) return;
    const QColor current(static_cast<int>(row->values[0]), static_cast<int>(row->values[1]),
                         static_cast<int>(row->values[2]), static_cast<int>(row->values[3]));
    const QColor chosen =
        QColorDialog::getColor(current, this, tr("Pick a colour"), QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    ApplyViewEdit(label, {static_cast<double>(chosen.red()), static_cast<double>(chosen.green()),
                          static_cast<double>(chosen.blue()), static_cast<double>(chosen.alpha())});
}

}
