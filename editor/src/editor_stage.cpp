#include "editor_window.h"

#include "editor_files.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/anchor_edit.h"
#include "document/authored.h"
#include "document/document.h"
#include "document/keyframes.h"
#include "document/motion_path.h"
#include "document/stage_align.h"
#include "document/stage_bounds.h"
#include "document/stage_fit.h"
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

void Window::ShowGhostsAround(uint32_t frame) {
    if (animation_name_.empty()) return;
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
    return !clip_.sprite || symbol_shown_ || context_.has_value();
}

std::optional<Document::StageOutline>
Window::ContextOf(const AfpAnimation::Animation& animation) const {
    if (!clip_.sprite || symbol_shown_) return std::nullopt;
    const AfpAnimation::Container* root = Document::FindClip(animation, Document::ClipId{});
    if (root == nullptr) return std::nullopt;
    const std::vector<Document::DepthRow> rows = Document::DepthRows(*root);
    std::optional<uint16_t> placed;
    for (const Document::DepthRow& row : rows) {
        for (const auto& [at, character] : row.shows) {
            if (at <= root_frame_ && character == *clip_.sprite) placed = row.depth;
        }
    }
    if (!placed) return std::nullopt;
    const std::vector<Document::StageOutline> outlines =
        Document::StageOutlines(animation, Document::ClipId{}, root_frame_, shape_bounds_);
    const auto found = std::ranges::find(outlines, *placed, &Document::StageOutline::depth);
    if (found == outlines.end()) return std::nullopt;
    return *found;
}

Document::StageOffset Window::UnderContext(double dx, double dy) const {
    if (!context_) return Document::StageOffset{.x = dx, .y = dy};
    const std::optional<Document::Point> moved = Document::UnderOutline(
        *context_, Document::Point{context_->anchor[0] + dx, context_->anchor[1] + dy});
    if (!moved) return Document::StageOffset{.x = dx, .y = dy};
    return Document::StageOffset{.x = (*moved)[0], .y = (*moved)[1]};
}

std::vector<Document::StageOutline>
Window::OutlinesOnStage(const AfpAnimation::Animation& animation) const {
    std::vector<Document::StageOutline> outlines = VisibleOutlines(animation);
    if (!context_) return outlines;
    for (Document::StageOutline& outline : outlines)
        outline = Document::OutlineThrough(outline, *context_);
    return outlines;
}

void Window::RefreshContext(const AfpAnimation::Animation& animation) {
    context_.reset();
    if (context_action_ == nullptr || !context_action_->isChecked()) return;
    context_ = ContextOf(animation);
}

void Window::UpdateOutlines(const AfpAnimation::Animation& animation) {
    if (shape_bounds_path_ != animation_path_ && file_) {
        shape_bounds_ = file_->ShapeBounds(animation_path_);
        shape_bounds_path_ = animation_path_;
    }
    RefreshContext(animation);
    viewport_->ShowContext(
        context_ ? std::optional<std::array<Document::Point, 4>>(context_->corners) : std::nullopt);
    viewport_->ShowPath(PathOfDepth(animation));
    if (!file_ || !OutlinesMatchView()) {
        viewport_->ShowOutlines({}, std::nullopt);
        return;
    }
    viewport_->ShowOutlines(OutlinesOnStage(animation),
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
    std::vector<Document::PathPoint> path = Document::MotionPath(
        *shown, depth, frame_, owned != nullptr ? owned->tracks : std::vector<Document::Track>{});
    if (!context_) return path;
    for (Document::PathPoint& point : path)
        point.at = Document::ThroughOutline(*context_, point.at);
    return path;
}

void Window::PickOnStage(double x, double y) {
    if (!file_ || animation_path_.empty() || !OutlinesMatchView()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const std::optional<uint16_t> picked = Document::DepthAt(OutlinesOnStage(*animation), {x, y});
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

void Window::CentreChosenAnchor() {
    if (!file_ || animation_path_.empty() || !depth_) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    if (RefuseOwnedAnchor(depth)) return;
    const std::map<uint16_t, Document::Box> shapes = file_->ShapeBounds(animation_path_);
    const Document::ClipId clip = clip_;
    const uint32_t frame = frame_;
    EditAnimation(tr("Centre the anchor of depth %1").arg(depth),
                  [clip, depth, frame, &shapes](AfpAnimation::Animation& edited) {
                      return Document::CentreAnchor(edited, clip, depth, frame, shapes);
                  });
}

bool Window::RefuseOwnedAnchor(uint16_t depth) {
    const std::optional<QString> refused = AnchorRefusal(depth);
    if (refused) ReportProblem(*refused);
    return refused.has_value();
}

void Window::MoveAnchorOnStage(uint16_t depth, double dx, double dy) {
    if (!file_ || animation_path_.empty() || RefuseOwnedAnchor(depth)) return;
    const Document::ClipId clip = clip_;
    const uint32_t frame = frame_;
    const Document::StageOffset under = UnderContext(dx, dy);
    EditAnimation(tr("Move the anchor of depth %1").arg(depth),
                  [clip, depth, frame, under](AfpAnimation::Animation& edited) {
                      return Document::MoveAnchor(edited, clip, depth, frame,
                                                  Document::Point{under.x, under.y});
                  });
}

void Window::FitChosenToStage(Document::StageFit fit) {
    if (const std::optional<QString> refused = FitRefusal()) {
        ReportProblem(*refused);
        return;
    }
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const auto depth = static_cast<uint16_t>(*depth_);
    const uint32_t frame = frame_;
    const auto change =
        Document::FitToStage(*animation, depth, frame, file_->ShapeBounds(animation_path_), fit);
    if (!change) {
        ReportProblem(QString::fromStdString(change.error()));
        return;
    }
    const Document::FitChange fitted = *change;
    EditOnStage(
        depth, tr("Fit depth %1 to the stage").arg(depth),
        [frame, fitted](Document::AuthoredDepth& authored,
                        const Document::BakedDepth& baked) -> Support::Expected<void, std::string> {
            auto reshaped = Document::ReshapeOwnedDepth(authored, baked, frame, fitted.reshape);
            if (!reshaped) return reshaped;
            return Document::MoveOwnedDepth(authored, baked, frame, fitted.offset);
        },
        [depth, frame,
         fitted](AfpAnimation::Animation& edited) -> Support::Expected<void, std::string> {
            auto reshaped = Document::ReshapeBakedDepth(edited, {}, depth, frame, fitted.reshape);
            if (!reshaped) return reshaped;
            return Document::MoveBakedDepth(edited, {}, depth, frame, fitted.offset);
        },
        true);
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
                               const std::vector<Document::StageOutline>&)>& offsets) {
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
    std::vector<Document::DepthOffset> moves;
    for (const Document::DepthOffset& move : offsets(chosen)) {
        if (move.offset[0] != 0 || move.offset[1] != 0) moves.push_back(move);
    }
    if (moves.empty()) {
        ShowResult(tr("The chosen depths are already lined up"), false);
        return;
    }
    MoveDepthsOnStage(moves, name, true);
}

void Window::PreviewOnStage(AnimationChange change) {
    pending_preview_ = std::move(change);
    if (preview_scheduled_) return;
    preview_scheduled_ = true;
    QTimer::singleShot(0, this, &Window::RunStagePreview);
}

void Window::CancelPreview(std::vector<Document::KeyRef> keys) {
    pending_preview_.reset();
    if (previewed_) Reload();
    previewed_ = false;
    timeline_->SelectKeys(std::move(keys));
    ShowFrame();
}

void Window::PreviewAuthored(const AuthoredChange& change) {
    if (!depth_) return;
    const std::optional<std::size_t> at = AuthoredIndexAt(static_cast<uint16_t>(*depth_), frame_);
    if (!at) return;
    Document::AuthoredDepth edited = authored_[*at];
    if (!change(edited)) return;
    PreviewOnStage([edited](AfpAnimation::Animation& animation) {
        using Written = Support::Expected<void, std::string>;
        auto baked = Document::BakedFor(animation, edited);
        if (!baked) return Written(Support::Unexpected(baked.error()));
        return Document::WriteAuthored(animation, edited, *baked);
    });
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

bool Window::SketchMove(uint16_t depth, double dx, double dy, bool finished) {
    if (viewport_->CurrentTool() != Tool::Sketch) return false;
    if (!sketch_ && !AuthoredIndexAt(depth, frame_)) return false;
    const Document::StageOffset offset{.x = dx, .y = dy};
    if (!sketch_) {
        sketch_ = Sketch{.depth = depth, .pressed = frame_, .latest = offset, .offsets = {}};
        if (!Playing()) TogglePlay();
    }
    sketch_->latest = offset;
    sketch_->offsets[frame_] = offset;
    if (!finished) return true;
    const Sketch sketched = *std::exchange(sketch_, std::nullopt);
    StopPlayback();
    SeekTo(sketched.pressed);
    EditOwned(tr("Sketch the motion of depth %1").arg(sketched.depth),
              [&sketched](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
                  return Document::SketchOwnedDepth(authored, baked, sketched.pressed,
                                                    sketched.offsets);
              });
    return true;
}

void Window::MoveOnStage(uint16_t depth, double stage_dx, double stage_dy, bool finished) {
    const Document::StageOffset offset = UnderContext(stage_dx, stage_dy);
    const double dx = offset.x;
    const double dy = offset.y;
    if (SketchMove(depth, dx, dy, finished)) return;
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
