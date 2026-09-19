#include "editor_window.h"

#include "editor_files.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/keyframes.h"
#include "document/motion_path.h"
#include "document/stage_align.h"
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
#include <QStatusBar>
#include <QString>
#include <QTimer>

#include <algorithm>
#include <array>
#include <functional>
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
    path_action_ = view->addAction(tr("Motion &path"));
    path_action_->setCheckable(true);
    path_action_->setChecked(QSettings().value(kPathKey, true).toBool());
    connect(path_action_, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(kPathKey, on);
        ShowFrame();
    });
    QAction* zoom_in = view->addAction(tr("Zoom the timeline &in"));
    zoom_in->setShortcut(QKeySequence(Qt::Key_Equal));
    connect(zoom_in, &QAction::triggered, timeline_, &Timeline::ZoomIn);
    QAction* zoom_out = view->addAction(tr("Zoom the timeline ou&t"));
    zoom_out->setShortcut(QKeySequence(Qt::Key_Minus));
    connect(zoom_out, &QAction::triggered, timeline_, &Timeline::ZoomOut);
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
    viewport_->ShowPath(PathOfDepth(animation));
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
                                   : std::nullopt,
                            SelectedDepths());
}

std::vector<Document::PathPoint>
Window::PathOfDepth(const AfpAnimation::Animation& animation) const {
    const AfpAnimation::Container* shown = Document::FindClip(animation, clip_);
    if (!depth_ || shown == nullptr || !OutlinesMatchView() || path_action_ == nullptr ||
        !path_action_->isChecked()) {
        return {};
    }
    const auto depth = static_cast<uint16_t>(*depth_);
    const std::vector<uint16_t> hidden = HiddenHere();
    if (std::ranges::find(hidden, depth) != hidden.end()) return {};
    const Document::AuthoredDepth* owned = AuthoredAt(depth, frame_);
    return Document::MotionPath(*shown, depth, frame_,
                                owned != nullptr ? owned->tracks : std::vector<Document::Track>{});
}

void Window::PickOnStage(double x, double y) {
    if (!file_ || animation_path_.empty() || !OutlinesMatchView()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const std::optional<uint16_t> picked = Document::DepthAt(VisibleOutlines(*animation), {x, y});
    const std::vector<uint16_t> group = SelectedDepths();
    if (picked && group.size() > 1 && std::ranges::find(group, *picked) != group.end()) {
        depth_ = *picked;
        ShowFrame();
        return;
    }
    if (picked) {
        ChooseDepth(*picked);
        return;
    }
    depth_.reset();
    selected_depths_.clear();
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

std::vector<uint16_t> Window::SelectedDepths() const {
    if (!depth_) return {};
    const auto primary = static_cast<uint16_t>(*depth_);
    if (std::ranges::find(selected_depths_, primary) != selected_depths_.end())
        return selected_depths_;
    return {primary};
}

void Window::MoveDepthsOnStage(const std::vector<Document::DepthOffset>& moves, const QString& name,
                               bool finished) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    std::vector<std::pair<std::size_t, Document::AuthoredDepth>> owned;
    std::vector<Document::DepthOffset> baked;
    for (const Document::DepthOffset& move : moves) {
        const Document::StageOffset offset{.x = move.offset[0], .y = move.offset[1]};
        const std::optional<std::size_t> index = AuthoredIndexAt(move.depth, frame);
        if (!index) {
            baked.push_back(move);
            continue;
        }
        Document::AuthoredDepth moved = authored_[*index];
        const auto state = Document::BakedFor(*animation, moved);
        auto shifted =
            state ? Document::MoveOwnedDepth(moved, *state, frame, offset)
                  : Support::Expected<void, std::string>(Support::Unexpected(state.error()));
        if (!shifted) {
            ReportOnce(QString::fromStdString(shifted.error()));
            return;
        }
        owned.emplace_back(*index, std::move(moved));
    }
    const AnimationChange change = [owned, baked, clip, frame](AfpAnimation::Animation& edited) {
        using Changed = Support::Expected<void, std::string>;
        for (const auto& [index, moved] : owned) {
            const auto state = Document::BakedFor(edited, moved);
            if (!state) return Changed(Support::Unexpected(state.error()));
            auto written = Document::WriteAuthored(edited, moved, *state);
            if (!written) return written;
        }
        for (const Document::DepthOffset& move : baked) {
            auto shifted = Document::MoveBakedDepth(
                edited, clip, move.depth, frame,
                Document::StageOffset{.x = move.offset[0], .y = move.offset[1]});
            if (!shifted) return shifted;
        }
        return Changed();
    };
    if (!finished) {
        PreviewOnStage(change);
        return;
    }
    pending_preview_.reset();
    const bool changed = EditAnimation(name, change);
    if (!changed && previewed_) Reload();
    previewed_ = false;
    if (!changed) return;
    for (auto& [index, moved] : owned)
        authored_[index] = std::move(moved);
    if (!owned.empty()) SaveProject();
    ShowFrame();
}

void Window::ArrangeChosen(const QString& name,
                           const std::function<std::vector<Document::DepthOffset>(
                               const std::vector<Document::StageOutline>&)>& offsets,
                           std::size_t fewest) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    const std::vector<uint16_t> group = SelectedDepths();
    std::vector<Document::StageOutline> chosen;
    for (const Document::StageOutline& outline : VisibleOutlines(*animation)) {
        if (std::ranges::find(group, outline.depth) != group.end()) chosen.push_back(outline);
    }
    if (!OutlinesMatchView() || chosen.size() < fewest) {
        ReportProblem(tr("Choose at least %1 depths shown on the stage first").arg(fewest));
        return;
    }
    std::vector<Document::DepthOffset> moves;
    for (const Document::DepthOffset& move : offsets(chosen)) {
        if (move.offset[0] != 0 || move.offset[1] != 0) moves.push_back(move);
    }
    if (moves.empty()) {
        statusBar()->showMessage(tr("The chosen depths are already lined up"));
        return;
    }
    MoveDepthsOnStage(moves, name, true);
}

void Window::AddAlignMenu(QMenu* edit) {
    QMenu* align = edit->addMenu(tr("&Align"));
    const std::array<std::pair<QString, Document::AlignTo>, 6> lines{{
        {tr("&Left edges"), Document::AlignTo::Left},
        {tr("&Horizontal centres"), Document::AlignTo::HorizontalCentre},
        {tr("&Right edges"), Document::AlignTo::Right},
        {tr("&Top edges"), Document::AlignTo::Top},
        {tr("&Vertical centres"), Document::AlignTo::VerticalCentre},
        {tr("&Bottom edges"), Document::AlignTo::Bottom},
    }};
    for (const auto& [text, how] : lines) {
        connect(align->addAction(text), &QAction::triggered, this, [this, text, how] {
            ArrangeChosen(
                tr("Align %1").arg(QString(text).remove('&').toLower()),
                [how](const std::vector<Document::StageOutline>& chosen) {
                    return Document::AlignOffsets(chosen, how);
                },
                2);
        });
    }
    align->addSeparator();
    const std::array<std::pair<QString, Document::Spread>, 2> spreads{{
        {tr("Spread centres &across"), Document::Spread::Across},
        {tr("Spread centres &down"), Document::Spread::Down},
    }};
    for (const auto& [text, how] : spreads) {
        connect(align->addAction(text), &QAction::triggered, this, [this, text, how] {
            ArrangeChosen(
                QString(text).remove('&'),
                [how](const std::vector<Document::StageOutline>& chosen) {
                    return Document::SpreadOffsets(chosen, how);
                },
                3);
        });
    }
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
    const std::vector<uint16_t> group = SelectedDepths();
    if (group.size() > 1 && std::ranges::find(group, depth) != group.end()) {
        std::vector<Document::DepthOffset> moves;
        for (const uint16_t member : group)
            moves.push_back(Document::DepthOffset{.depth = member, .offset = {dx, dy}});
        MoveDepthsOnStage(moves, tr("Move %n depths", nullptr, static_cast<int>(group.size())),
                          finished);
        return;
    }
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
