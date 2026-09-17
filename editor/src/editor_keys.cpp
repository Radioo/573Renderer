#include "editor_window.h"

#include "editor_timeline.h"

#include "document/authored.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Editor {

namespace {

QString CurveText(const Document::Bezier& bezier) {
    return QString("%1, %2, %3, %4").arg(bezier.x1).arg(bezier.y1).arg(bezier.x2).arg(bezier.y2);
}

std::optional<Document::Bezier> CurveFrom(const QString& text) {
    const QStringList parts = text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    if (parts.size() != 4) return std::nullopt;
    Document::Bezier bezier;
    bool ok = true;
    bezier.x1 = parts[0].toDouble(&ok);
    if (!ok) return std::nullopt;
    bezier.y1 = parts[1].toDouble(&ok);
    if (!ok) return std::nullopt;
    bezier.x2 = parts[2].toDouble(&ok);
    if (!ok) return std::nullopt;
    bezier.y2 = parts[3].toDouble(&ok);
    if (!ok) return std::nullopt;
    return bezier;
}

}

void Window::ShowKeysForDepth(const Document::AuthoredDepth* owned) {
    if (owned == nullptr) {
        timeline_->ShowKeys(std::nullopt, {});
        return;
    }
    timeline_->ShowKeys(owned->depth, owned->tracks);
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
        bool accepted = false;
        const QString text = QInputDialog::getText(
            this, tr("IFS Editor"), tr("Control points as x1, y1, x2, y2:"), QLineEdit::Normal,
            key ? CurveText(key->bezier) : CurveText({}), &accepted);
        if (!accepted) return;
        const std::optional<Document::Bezier> parsed = CurveFrom(text);
        if (!parsed) {
            ReportProblem(tr("An ease needs four numbers: x1, y1, x2, y2."));
            return;
        }
        wanted = Document::Ease::Bezier;
        curve = *parsed;
    } else if (chosen != linear) {
        return;
    }

    EditAuthored(tr("Ease the %1 keyframe at frame %2").arg(property).arg(frame),
                 [&name, frame, wanted, curve](Document::AuthoredDepth& owned) {
                     return Document::SetKeyEaseAt(owned, name, frame, wanted, curve);
                 });
}

}
