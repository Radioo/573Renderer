#include "editor_window.h"

#include "document/camera_edit.h"
#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/label_edit.h"
#include "document/place_image.h"
#include "formats/afp_animation.h"

#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace Editor {

void Window::ShowTimelineMenu(const QPoint& where, uint32_t frame, const QString& label) {
    if (!file_ || animation_path_.empty()) return;
    const Document::ClipId clip = clip_;
    QMenu menu(this);
    QAction* add = menu.addAction(tr("Add a label at frame %1...").arg(frame));
    QAction* rename = label.isEmpty() ? nullptr : menu.addAction(tr("Rename %1...").arg(label));
    QAction* move =
        label.isEmpty() ? nullptr : menu.addAction(tr("Move %1 to frame %2").arg(label).arg(frame));
    QAction* remove = label.isEmpty() ? nullptr : menu.addAction(tr("Remove %1").arg(label));
    menu.addSeparator();
    QAction* insert_frame = menu.addAction(tr("Insert a frame at %1").arg(frame));
    QAction* remove_frame = menu.addAction(tr("Remove frame %1").arg(frame));
    QAction* add_depth =
        depth_ ? menu.addAction(tr("Add depth %1 from frame %2...").arg(*depth_).arg(frame))
               : nullptr;
    QAction* remove_depth =
        depth_ ? menu.addAction(tr("Remove depth %1 here").arg(*depth_)) : nullptr;
    QAction* restack =
        depth_ ? menu.addAction(tr("Move depth %1 here to another depth...").arg(*depth_))
               : nullptr;
    QAction* copy = depth_ ? menu.addAction(tr("Copy depth %1 here").arg(*depth_)) : nullptr;
    QAction* paste = copied_span_ ? menu.addAction(tr("Paste the copied depth here...")) : nullptr;
    QAction* duplicate =
        depth_ ? menu.addAction(tr("Duplicate depth %1 here onto another depth...").arg(*depth_))
               : nullptr;
    QAction* group =
        depth_ ? menu.addAction(tr("Group depth %1 and up here into a sprite...").arg(*depth_))
               : nullptr;
    QAction* ungroup =
        depth_ ? menu.addAction(tr("Ungroup the sprite on depth %1 here").arg(*depth_)) : nullptr;
    const bool hidden = depth_ && IsHidden(static_cast<uint16_t>(*depth_));
    QAction* hide = depth_ ? menu.addAction(hidden ? tr("Show depth %1 in the view").arg(*depth_)
                                                   : tr("Hide depth %1 in the view").arg(*depth_))
                           : nullptr;
    QAction* solo = depth_ ? menu.addAction(tr("Solo depth %1 in the view").arg(*depth_)) : nullptr;
    QAction* show_all = hidden_.empty() ? nullptr : menu.addAction(tr("Show every hidden depth"));
    const bool locked = depth_ && IsLocked(static_cast<uint16_t>(*depth_));
    QAction* lock = depth_ ? menu.addAction(locked ? tr("Unlock depth %1 on stage").arg(*depth_)
                                                   : tr("Lock depth %1 on stage").arg(*depth_))
                           : nullptr;
    QAction* export_name =
        clip_.sprite ? menu.addAction(tr("Name the export of this sprite...")) : nullptr;
    menu.addSeparator();
    const bool authored =
        depth_ != std::nullopt && AuthoredAt(static_cast<uint16_t>(*depth_), frame) != nullptr;
    QAction* own = (project_ && depth_ && !authored)
                       ? menu.addAction(tr("Let the project own depth %1 from here").arg(*depth_))
                       : nullptr;
    QAction* detach =
        authored ? menu.addAction(tr("Detach depth %1 back to baked data").arg(*depth_)) : nullptr;
    QAction* script =
        authored ? menu.addAction(tr("Edit the script of depth %1...").arg(*depth_)) : nullptr;
    QAction* start =
        authored ? menu.addAction(tr("Start animating depth %1...").arg(*depth_)) : nullptr;
    menu.addSeparator();
    const auto animation = file_->ReadAnimation(animation_path_);
    const AfpAnimation::Container* shown =
        animation ? Document::FindClip(*animation, clip) : nullptr;
    const bool has_camera = shown != nullptr && Document::CameraTag(*shown, frame).has_value();
    const auto clip_frames = static_cast<int>(shown != nullptr ? shown->frames.size() : 0);
    QAction* add_camera =
        has_camera ? nullptr : menu.addAction(tr("Add a camera on frame %1...").arg(frame));
    QAction* remove_camera =
        has_camera ? menu.addAction(tr("Remove the camera on frame %1").arg(frame)) : nullptr;
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;

    if (chosen == own) {
        OwnSelectedDepth(frame);
        return;
    }
    if (chosen == detach) {
        DetachSelectedDepth();
        return;
    }
    if (chosen == script) {
        EditOwnedScript();
        return;
    }
    if (chosen == start) {
        StartAnimating();
        return;
    }
    if (chosen == add_camera) {
        bool answered = false;
        const int id = QInputDialog::getInt(this, tr("Add a camera"), tr("Camera"), 0, 0,
                                            std::numeric_limits<uint16_t>::max(), 1, &answered);
        if (!answered) return;
        const auto number = static_cast<uint16_t>(id);
        EditAnimation(tr("Add camera %1").arg(id),
                      [clip, frame, number](AfpAnimation::Animation& edited) {
                          return Document::AddCamera(edited, clip, frame, number);
                      });
        return;
    }
    if (chosen == remove_camera) {
        EditAnimation(tr("Remove the camera on frame %1").arg(frame),
                      [clip, frame](AfpAnimation::Animation& edited) {
                          return Document::RemoveCamera(edited, clip, frame);
                      });
        return;
    }

    if (chosen == insert_frame) {
        EditAnimation(tr("Insert frame %1").arg(frame),
                      [clip, frame](AfpAnimation::Animation& edited) {
                          return Document::InsertFrame(edited, clip, frame);
                      });
        return;
    }
    if (chosen == remove_frame) {
        EditAnimation(tr("Remove frame %1").arg(frame),
                      [clip, frame](AfpAnimation::Animation& edited) {
                          return Document::RemoveFrame(edited, clip, frame);
                      });
        return;
    }
    if (chosen == add_depth) {
        bool answered = false;
        const int last =
            QInputDialog::getInt(this, tr("Add a depth"), tr("Last frame"), static_cast<int>(frame),
                                 static_cast<int>(frame),
                                 std::max(clip_frames - 1, static_cast<int>(frame)), 1, &answered);
        if (!answered || !animation) return;
        const std::optional<Placeable> choice = ChoosePlaceable(*animation);
        if (!choice) return;
        const auto depth = static_cast<uint16_t>(*depth_);
        const auto until = static_cast<uint32_t>(last);
        if (!choice->character) {
            PlaceImage(choice->image, Document::DepthSpan{.clip = clip,
                                                          .depth = depth,
                                                          .first_frame = frame,
                                                          .last_frame = until});
            return;
        }
        const uint16_t placed = *choice->character;
        EditAnimation(tr("Add depth %1").arg(*depth_),
                      [clip, depth, placed, frame, until](AfpAnimation::Animation& edited) {
                          return Document::AddDepth(edited, clip, depth, placed, frame, until);
                      });
        return;
    }
    if (chosen == export_name) {
        NameShownSpriteExport();
        return;
    }
    if (chosen == show_all) {
        ShowEveryDepth();
        return;
    }
    if (chosen == solo) {
        SoloDepth(static_cast<uint16_t>(*depth_));
        return;
    }
    if (chosen == lock) {
        ToggleLocked(static_cast<uint16_t>(*depth_));
        return;
    }
    if (chosen == hide) {
        ToggleHidden(static_cast<uint16_t>(*depth_));
        return;
    }
    if (chosen == copy) {
        CopySpanAt(static_cast<uint16_t>(*depth_), frame);
        return;
    }
    if (chosen == paste) {
        PasteSpanAt(frame);
        return;
    }
    if (chosen == restack || chosen == duplicate || chosen == group || chosen == ungroup) {
        const auto depth = static_cast<uint16_t>(*depth_);
        if (chosen == restack) MoveSpanToDepth(depth, frame);
        if (chosen == duplicate) DuplicateSpanToDepth(depth, frame);
        if (chosen == group) GroupDepthsIntoSprite(depth, frame);
        if (chosen == ungroup) UngroupSpriteAt(depth, frame);
        return;
    }
    if (chosen == remove_depth) {
        const auto depth = static_cast<uint16_t>(*depth_);
        EditAnimation(tr("Remove depth %1").arg(*depth_),
                      [clip, depth, frame](AfpAnimation::Animation& edited) {
                          return Document::RemoveDepth(edited, clip, depth, frame);
                      });
        return;
    }

    if (chosen == add) {
        bool answered = false;
        const QString name = QInputDialog::getText(this, tr("Add a label"), tr("Name"),
                                                   QLineEdit::Normal, QString(), &answered);
        if (!answered || name.isEmpty()) return;
        const std::string text = name.toStdString();
        EditAnimation(tr("Add label %1").arg(name),
                      [clip, text, frame](AfpAnimation::Animation& edited) {
                          return Document::AddLabel(edited, clip, text, frame);
                      });
        return;
    }
    const std::string named = label.toStdString();
    if (chosen == rename) {
        bool answered = false;
        const QString renamed = QInputDialog::getText(this, tr("Rename a label"), tr("Name"),
                                                      QLineEdit::Normal, label, &answered);
        if (!answered || renamed.isEmpty()) return;
        const std::string text = renamed.toStdString();
        EditAnimation(tr("Rename %1").arg(label),
                      [clip, named, text](AfpAnimation::Animation& edited) {
                          return Document::RenameLabel(edited, clip, named, text);
                      });
        return;
    }
    if (chosen == move) {
        EditAnimation(tr("Move %1").arg(label),
                      [clip, named, frame](AfpAnimation::Animation& edited) {
                          return Document::MoveLabel(edited, clip, named, frame);
                      });
        return;
    }
    if (chosen == remove) {
        EditAnimation(tr("Remove %1").arg(label), [clip, named](AfpAnimation::Animation& edited) {
            return Document::RemoveLabel(edited, clip, named);
        });
    }
}

}
