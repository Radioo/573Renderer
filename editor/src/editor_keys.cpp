#include "editor_window.h"

#include "editor_ease_dialog.h"
#include "editor_graph.h"
#include "editor_timeline.h"

#include "document/colour_pick.h"
#include "document/authored.h"
#include "document/filter_fields.h"
#include "document/key_selection.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QAction>
#include <QDialog>
#include <QInputDialog>
#include <QColor>
#include <QColorDialog>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

void Window::ShowKeysForDepth(const Document::AuthoredDepth* owned) {
    if (owned == nullptr) {
        timeline_->ShowKeys(std::nullopt, {});
        graph_->ShowTrack(std::nullopt, 0, 0, 0);
        return;
    }
    timeline_->ShowKeys(owned->depth, owned->tracks);
    graph_->ShowTrack(Document::GraphedTrack(*owned, key_property_.toStdString()),
                      owned->first_frame, owned->last_frame, frame_);
}

void Window::ApplyGraphValue(const QString& property, uint32_t frame,
                             const std::vector<int64_t>& value) {
    const std::string name = property.toStdString();
    EditAuthored(tr("%1 on frame %2").arg(property).arg(frame),
                 [&name, frame, &value](Document::AuthoredDepth& owned) {
                     return Document::SetKeyValuesAt(owned, name, frame, value);
                 });
}

std::optional<std::size_t> Window::AuthoredIndexAt(uint16_t depth, uint32_t frame) const {
    for (std::size_t i = 0; i < authored_.size(); i++) {
        const Document::AuthoredDepth& owned = authored_[i];
        if (owned.animation == animation_path_ && owned.clip == clip_ && owned.depth == depth &&
            frame >= owned.first_frame && frame <= owned.last_frame) {
            return i;
        }
    }
    return std::nullopt;
}

bool Window::EditAuthored(const QString& name, const AuthoredChange& change) {
    if (!file_ || !depth_ || animation_path_.empty()) return false;
    const std::optional<std::size_t> at = AuthoredIndexAt(static_cast<uint16_t>(*depth_), frame_);
    if (!at) return false;

    Document::AuthoredDepth edited = authored_[*at];
    const auto changed = change(edited);
    if (!changed) {
        ReportProblem(QString::fromStdString(changed.error()));
        return false;
    }
    std::vector<Document::KeyRef> kept = timeline_->SelectedKeys();
    if (!EditAnimation(name, [&edited](AfpAnimation::Animation& animation) {
            using Written = Support::Expected<void, std::string>;
            auto baked = Document::BakedFor(animation, edited);
            if (!baked) return Written(Support::Unexpected(baked.error()));
            return Document::WriteAuthored(animation, edited, *baked);
        })) {
        return false;
    }
    authored_[*at] = std::move(edited);
    SaveProject();
    timeline_->SelectKeys(std::move(kept));
    ShowFrame();
    return true;
}

void Window::ChooseKey(const QString& property, uint32_t frame) {
    timeline_->SelectKey(property, frame);
    FocusKey(property, frame);
}

bool Window::ApplyKeyEdit(const QString& value) {
    if (key_property_.isEmpty() || !key_frame_) return false;
    const std::string property = key_property_.toStdString();
    const std::string text = value.toStdString();
    const uint32_t frame = *key_frame_;
    return EditAuthored(tr("%1 on frame %2").arg(key_property_).arg(frame),
                        [&property, frame, &text](Document::AuthoredDepth& owned) {
                            return Document::SetKeyValueAt(owned, property, frame, text);
                        });
}

bool Window::ApplyKeyFilterEdit(const QString& field, const QString& value) {
    if (!key_frame_) return false;
    const std::string name = field.toStdString();
    const std::string text = value.toStdString();
    const uint32_t frame = *key_frame_;
    return EditAuthored(tr("%1 on frame %2").arg(field).arg(frame),
                        [&name, frame, &text](Document::AuthoredDepth& owned) {
                            return Document::SetKeyFilterFieldAt(owned, frame, name, text);
                        });
}

void Window::PickColour(QTableWidgetItem* cell) {
    const Document::Rgba current =
        Document::ColourOfField(cell->text().toStdString())
            .value_or(Document::Rgba{.red = 255, .green = 255, .blue = 255, .alpha = 255});
    const QColor chosen =
        QColorDialog::getColor(QColor(current.red, current.green, current.blue, current.alpha),
                               this, tr("Pick a colour"), QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    cell->setText(
        QString::fromStdString(Document::ColourFieldText(Document::Rgba{.red = chosen.red(),
                                                                        .green = chosen.green(),
                                                                        .blue = chosen.blue(),
                                                                        .alpha = chosen.alpha()})));
}

void Window::ShowInspectorMenu(const QPoint& where) {
    QMenu menu(this);
    const QTableWidgetItem* row = inspector_->itemAt(where);
    const QTableWidgetItem* name = row == nullptr ? nullptr : inspector_->item(row->row(), 0);
    QTableWidgetItem* value = row == nullptr ? nullptr : inspector_->item(row->row(), 1);
    const std::string field = name == nullptr ? std::string() : name->text().toStdString();
    const QAction* pick = nullptr;
    if (value != nullptr && (value->flags() & Qt::ItemIsEditable) != 0 &&
        Document::PicksColour(field, key_property_.toStdString())) {
        pick = menu.addAction(tr("Pick a colour..."));
    }
    if (!key_frame_ || key_property_ != QStringLiteral("Filters")) {
        if (menu.isEmpty()) return;
        if (menu.exec(inspector_->viewport()->mapToGlobal(where)) == pick && pick != nullptr)
            PickColour(value);
        return;
    }
    const uint32_t frame = *key_frame_;
    const auto add = [this, frame](Document::NewFilter kind) {
        EditAuthored(tr("Add a filter on frame %1").arg(frame),
                     [frame, kind](Document::AuthoredDepth& owned) {
                         return Document::AddKeyFilterAt(owned, frame, kind);
                     });
    };
    menu.addAction(tr("Add colour matrix filter"), this,
                   [add] { add(Document::NewFilter::ColourMatrix); });
    menu.addAction(tr("Add HSV filter"), this, [add] { add(Document::NewFilter::Hsv); });
    const std::optional<std::size_t> number = Document::FilterNumberOf(field);
    if (number) {
        menu.addAction(tr("Remove filter %1").arg(*number), this, [this, frame, field, number] {
            EditAuthored(tr("Remove filter %1 on frame %2").arg(*number).arg(frame),
                         [frame, &field](Document::AuthoredDepth& owned) {
                             return Document::RemoveKeyFilterAt(owned, frame, field);
                         });
        });
    }
    if (menu.exec(inspector_->viewport()->mapToGlobal(where)) == pick && pick != nullptr)
        PickColour(value);
}

void Window::StartAnimating() {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const std::optional<std::size_t> at = AuthoredIndexAt(static_cast<uint16_t>(*depth_), frame_);
    if (!at) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    const auto baked = Document::BakedFor(*animation, authored_[*at]);
    if (!baked) {
        ReportProblem(QString::fromStdString(baked.error()));
        return;
    }
    QStringList choices;
    for (const std::string& property : Document::PropertiesToAdd(authored_[*at], *baked))
        choices.append(QString::fromStdString(property));
    if (choices.isEmpty()) {
        ReportProblem(tr("Depth %1 already animates everything it can").arg(*depth_));
        return;
    }
    bool answered = false;
    const QString picked = QInputDialog::getItem(this, tr("Start animating"), tr("Property"),
                                                 choices, 0, false, &answered);
    if (!answered) return;
    const std::string property = picked.toStdString();
    const Document::BakedDepth resting = *baked;
    if (EditAuthored(tr("Animate %1 on depth %2").arg(picked).arg(*depth_),
                     [&property, &resting](Document::AuthoredDepth& owned) {
                         return Document::AddTrack(owned, resting, property);
                     })) {
        ChooseKey(picked, authored_[*at].first_frame);
    }
}

void Window::ShowKeyMenu(const QPoint& where, const QString& property, uint32_t frame,
                         bool on_key) {
    if (!depth_) return;
    const std::optional<std::size_t> at = AuthoredIndexAt(static_cast<uint16_t>(*depth_), frame_);
    if (!at) return;
    const std::string name = property.toStdString();
    const std::optional<Document::Keyframe> key =
        on_key ? Document::KeyAt(authored_[*at], name, frame) : std::nullopt;

    QMenu menu(this);
    QAction* add =
        on_key ? nullptr
               : menu.addAction(tr("Add a %1 keyframe at frame %2").arg(property).arg(frame));
    QAction* remove =
        on_key ? menu.addAction(tr("Remove the %1 keyframe at frame %2").arg(property).arg(frame))
               : nullptr;
    QMenu* ease = on_key ? menu.addMenu(tr("How it leaves frame %1").arg(frame)) : nullptr;
    QAction* hold = ease != nullptr ? ease->addAction(tr("Hold")) : nullptr;
    QAction* linear = ease != nullptr ? ease->addAction(tr("Linear")) : nullptr;
    QAction* bezier = ease != nullptr ? ease->addAction(tr("Bezier...")) : nullptr;
    menu.addSeparator();
    const std::size_t selected = timeline_->SelectedKeys().size();
    QAction* copy =
        selected > 0
            ? menu.addAction(tr("Copy %n keyframe(s)", nullptr, static_cast<int>(selected)))
            : nullptr;
    QAction* paste =
        copied_keys_ ? menu.addAction(tr("Paste keyframes at frame %1").arg(frame_)) : nullptr;
    QAction* erase =
        selected > 0
            ? menu.addAction(tr("Delete %n keyframe(s)", nullptr, static_cast<int>(selected)))
            : nullptr;
    QMenu* easy = selected > 0 ? menu.addMenu(tr("Easy ease")) : nullptr;
    QAction* easy_both = easy != nullptr ? easy->addAction(tr("Both sides")) : nullptr;
    QAction* easy_in = easy != nullptr ? easy->addAction(tr("In")) : nullptr;
    QAction* easy_out = easy != nullptr ? easy->addAction(tr("Out")) : nullptr;
    QAction* reverse =
        selected > 1
            ? menu.addAction(tr("Time-reverse %n keyframe(s)", nullptr, static_cast<int>(selected)))
            : nullptr;
    if (key) {
        for (QAction* one : {hold, linear, bezier})
            one->setCheckable(true);
        hold->setChecked(key->ease == Document::Ease::Hold);
        linear->setChecked(key->ease == Document::Ease::Linear);
        bezier->setChecked(key->ease == Document::Ease::Bezier);
    }

    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;

    if (chosen == copy) {
        CopySelectedKeys();
        return;
    }
    if (chosen == paste) {
        PasteCopiedKeys();
        return;
    }
    if (chosen == erase) {
        RemoveSelectedKeys();
        return;
    }
    if (chosen == reverse) {
        ReverseSelectedKeys();
        return;
    }
    if (chosen == easy_both || chosen == easy_in || chosen == easy_out) {
        EasyEaseSelectedKeys(chosen == easy_both ? Document::EasySide::Both
                             : chosen == easy_in ? Document::EasySide::In
                                                 : Document::EasySide::Out);
        return;
    }
    if (chosen == add) {
        EditAuthored(tr("Add a %1 keyframe at frame %2").arg(property).arg(frame),
                     [&name, frame](Document::AuthoredDepth& owned) {
                         return Document::AddKeyAt(owned, name, frame);
                     });
        ChooseKey(property, frame);
        return;
    }
    if (chosen == remove) {
        if (EditAuthored(tr("Remove the %1 keyframe at frame %2").arg(property).arg(frame),
                         [&name, frame](Document::AuthoredDepth& owned) {
                             return Document::RemoveKeyAt(owned, name, frame);
                         })) {
            key_frame_.reset();
            timeline_->SelectKey(property, std::nullopt);
            ShowFrame();
        }
        return;
    }

    Document::Ease wanted = Document::Ease::Linear;
    Document::Bezier curve;
    if (chosen == hold) {
        wanted = Document::Ease::Hold;
    } else if (chosen == bezier) {
        const Document::Bezier start = key && key->ease == Document::Ease::Bezier
                                           ? key->bezier
                                           : Document::EasePresets().front().bezier;
        EaseDialog dialog(start, this);
        if (dialog.exec() != QDialog::Accepted) return;
        wanted = Document::Ease::Bezier;
        curve = dialog.Result();
    } else if (chosen != linear) {
        return;
    }

    const Document::KeyRef clicked{.property = name, .frame = frame};
    std::vector<Document::KeyRef> eased = timeline_->SelectedKeys();
    if (std::ranges::find(eased, clicked) == eased.end()) eased = {clicked};
    EditAuthored(tr("Ease %n keyframe(s)", nullptr, static_cast<int>(eased.size())),
                 [&eased, wanted, curve](Document::AuthoredDepth& owned) {
                     return Document::SetKeysEase(owned, eased, wanted, curve);
                 });
}

}
