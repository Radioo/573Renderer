#include "editor_window.h"

#include "editor_files.h"
#include "editor_viewport.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/stage_bounds.h"
#include "document/stage_move.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <DockManager.h>
#include <DockWidget.h>

#include <QAction>
#include <QImage>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QString>
#include <QTimer>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

void Window::AddViewMenu() {
    QMenu* view = menuBar()->addMenu(tr("&View"));
    QAction* snap = view->addAction(tr("&Snap while moving on stage"));
    snap->setCheckable(true);
    snap->setChecked(QSettings().value(kSnapKey, true).toBool());
    viewport_->SetSnapping(snap->isChecked());
    connect(snap, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(kSnapKey, on);
        viewport_->SetSnapping(on);
    });
    QAction* rulers = view->addAction(tr("&Rulers"));
    rulers->setCheckable(true);
    rulers->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    rulers->setChecked(QSettings().value(kRulersKey, false).toBool());
    viewport_->SetRulers(rulers->isChecked());
    connect(rulers, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(kRulersKey, on);
        viewport_->SetRulers(on);
    });
    QAction* clear_guides = view->addAction(tr("Clear &guides"));
    connect(clear_guides, &QAction::triggered, viewport_, &Viewport::ClearGuides);
    onion_action_ = view->addAction(tr("&Onion skin"));
    onion_action_->setCheckable(true);
    onion_action_->setChecked(QSettings().value(kOnionKey, false).toBool());
    connect(onion_action_, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(kOnionKey, on);
        if (!on) {
            viewport_->ShowGhosts({});
            return;
        }
        if (host_.Running() && !animation_name_.empty()) RenderFrame();
    });
    QMenu* panels = view->addMenu(tr("&Panels"));
    for (ads::CDockWidget* dock : docks_->dockWidgetsMap())
        panels->addAction(dock->toggleViewAction());
    QAction* fit = view->addAction(tr("&Fit the stage in the view"));
    fit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(fit, &QAction::triggered, viewport_, &Viewport::FitStage);
}

void Window::ShowGhostsAround(uint32_t frame) {
    std::vector<QImage> ghosts;
    for (const int64_t step : {int64_t{-1}, int64_t{1}}) {
        const int64_t neighbour = static_cast<int64_t>(frame) + step;
        if (neighbour < 0 || neighbour >= static_cast<int64_t>(frame_count_)) continue;
        if (!host_.Seek(static_cast<uint32_t>(neighbour))) continue;
        auto read = ReadFrame();
        if (read) ghosts.push_back(std::move(read->image));
    }
    const auto back = host_.Seek(frame);
    if (!back) ReportOnce(QString::fromStdString(back.error()));
    viewport_->ShowGhosts(std::move(ghosts));
}

bool Window::OutlinesMatchView() const {
    return !clip_.sprite || symbol_shown_;
}

void Window::UpdateOutlines(const AfpAnimation::Animation& animation) {
    if (!file_ || !OutlinesMatchView()) {
        viewport_->ShowOutlines({}, std::nullopt);
        return;
    }
    if (shape_bounds_path_ != animation_path_) {
        shape_bounds_ = file_->ShapeBounds(animation_path_);
        shape_bounds_path_ = animation_path_;
    }
    viewport_->ShowOutlines(VisibleOutlines(animation),
                            depth_ ? std::optional<uint16_t>(static_cast<uint16_t>(*depth_))
                                   : std::nullopt);
}

void Window::PickOnStage(double x, double y) {
    if (!file_ || animation_path_.empty() || !OutlinesMatchView()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const std::optional<uint16_t> picked = Document::DepthAt(VisibleOutlines(*animation), {x, y});
    if (picked) {
        ChooseDepth(*picked);
        return;
    }
    depth_.reset();
    ShowFrame();
}

namespace {

AnimationChange OwnedAsAnimation(Document::AuthoredDepth authored, OwnedChange owned) {
    return [authored = std::move(authored),
            owned = std::move(owned)](AfpAnimation::Animation& animation) {
        using Changed = Support::Expected<void, std::string>;
        Document::AuthoredDepth edited = authored;
        const auto baked = Document::BakedFor(animation, edited);
        if (!baked) return Changed(Support::Unexpected(baked.error()));
        auto changed = owned(edited, *baked);
        if (!changed) return changed;
        const auto rebaked = Document::BakedFor(animation, edited);
        if (!rebaked) return Changed(Support::Unexpected(rebaked.error()));
        return Document::WriteAuthored(animation, edited, *rebaked);
    };
}

}

void Window::EditOnStage(uint16_t depth, const QString& name, const OwnedChange& owned,
                         const AnimationChange& baked, bool finished) {
    if (!file_ || animation_path_.empty()) return;
    const std::optional<std::size_t> index = AuthoredIndexAt(depth, frame_);
    if (!finished) {
        PreviewOnStage(index ? OwnedAsAnimation(authored_[*index], owned) : baked);
        return;
    }
    pending_preview_.reset();
    depth_ = depth;
    const bool changed = index ? EditOwned(name, owned) : EditAnimation(name, baked);
    if (!changed && previewed_) Reload();
    previewed_ = false;
}

void Window::PreviewOnStage(AnimationChange change) {
    pending_preview_ = std::move(change);
    if (preview_scheduled_) return;
    preview_scheduled_ = true;
    QTimer::singleShot(0, this, &Window::RunStagePreview);
}

void Window::RunStagePreview() {
    preview_scheduled_ = false;
    if (!pending_preview_) return;
    const AnimationChange change = std::move(*pending_preview_);
    pending_preview_.reset();
    if (!file_ || !host_.Running() || !OutlinesMatchView()) return;
    Document::File shown = *file_;
    auto animation = shown.ReadAnimation(animation_path_);
    if (!animation || !change(*animation)) return;
    if (!shown.WriteAnimation(animation_path_, *animation)) return;
    if (!LoadViewportClip(shown)) return;
    previewed_ = true;
    SeekViewport(symbol_shown_ ? frame_ : root_frame_);
}

void Window::MoveOnStage(uint16_t depth, double dx, double dy, bool finished) {
    const Document::StageOffset offset{.x = dx, .y = dy};
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    EditOnStage(
        depth, tr("Move depth %1").arg(depth),
        [frame, offset](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
            return Document::MoveOwnedDepth(authored, baked, frame, offset);
        },
        [clip, depth, frame, offset](AfpAnimation::Animation& edited) {
            return Document::MoveBakedDepth(edited, clip, depth, frame, offset);
        },
        finished);
}

void Window::ReshapeOnStage(uint16_t depth, double scale_x, double scale_y, double turn,
                            bool finished) {
    const Document::Reshape reshape{.scale_x = scale_x, .scale_y = scale_y, .turn = turn};
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    const QString name =
        turn != 0 ? tr("Turn depth %1").arg(depth) : tr("Scale depth %1").arg(depth);
    EditOnStage(
        depth, name,
        [frame, reshape](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
            return Document::ReshapeOwnedDepth(authored, baked, frame, reshape);
        },
        [clip, depth, frame, reshape](AfpAnimation::Animation& edited) {
            return Document::ReshapeBakedDepth(edited, clip, depth, frame, reshape);
        },
        finished);
}

}
