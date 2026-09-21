#include "editor_window.h"

#include "editor_clip_view.h"
#include "editor_host_load.h"
#include "editor_jobs.h"

#include "editor_commands.h"
#include "editor_graph.h"
#include "editor_timeline.h"
#include "editor_stage_bar.h"
#include "editor_timeline_bar.h"
#include "editor_viewport.h"

#include "document/authored.h"
#include "document/characters.h"
#include "document/clip.h"
#include "document/clip_extract.h"
#include "document/clip_trim.h"
#include "document/frame_notes.h"
#include "document/hidden_depths.h"
#include "document/keyframes.h"
#include "document/outline.h"
#include "document/playback.h"
#include "document/place_image.h"
#include "support/expected.h"
#include "document/sprite_exports.h"
#include "document/sprite_preview.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <QAction>
#include <DockWidget.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kGraphPropertiesWidth = 150;

}

QWidget* Window::BuildGraphPanel() {
    graph_properties_ = new QListWidget;
    graph_properties_->setObjectName("graph_properties");
    graph_properties_->setFixedWidth(kGraphPropertiesWidth);
    connect(graph_properties_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const std::string property = item->text().toStdString();
        std::erase(graph_hidden_, property);
        if (item->checkState() != Qt::Checked) graph_hidden_.push_back(property);
        RefreshGraphTracks(depth_ ? AuthoredAt(static_cast<uint16_t>(*depth_), frame_) : nullptr);
    });

    auto* side = new QWidget;
    auto* beside = new QVBoxLayout(side);
    beside->setContentsMargins(4, 4, 4, 4);
    beside->setSpacing(2);
    beside->addWidget(graph_properties_, 1);
    for (const auto& [id, text] : {std::pair{QStringLiteral("graph.fit_all"), tr("Fit all")},
                                   std::pair{QStringLiteral("graph.fit_keys"), tr("Fit keys")}}) {
        auto* button = new QToolButton;
        button->setObjectName("graph_" + id);
        button->setText(text);
        connect(button, &QToolButton::clicked, this, [this, id] { commands_->Run(id); });
        beside->addWidget(button);
    }

    auto* panel = new QWidget;
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(side);
    layout->addWidget(graph_, 1);
    return panel;
}

QWidget* Window::BuildTimelinePanel(QScrollArea* timeline_area) {
    timeline_bar_ = new TimelineBar(*commands_);
    connect(timeline_bar_, &TimelineBar::FrameTyped, this,
            [this](uint32_t frame) { JumpToFrame(frame); });
    connect(timeline_bar_, &TimelineBar::GraphAsked, this, &Window::ShowGraphPanel);
    connect(timeline_bar_, &TimelineBar::ZoomAsked, this,
            [this](double pixels) { timeline_->SetZoomPixels(pixels); });

    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(timeline_bar_);
    layout->addWidget(timeline_area);
    return panel;
}

void Window::RefreshStageBar() {
    if (stage_bar_ == nullptr) return;
    std::vector<Crumb> crumbs;
    if (!animation_name_.empty()) {
        crumbs.push_back(Crumb{.text = QString::fromStdString(animation_name_), .clip = 0});
        for (int at = 0; at < static_cast<int>(clips_.size()); at++) {
            const bool root = !clips_[static_cast<std::size_t>(at)].id.sprite;
            if (!root && at != clip_index_) continue;
            crumbs.push_back(Crumb{.text = ClipName(at), .clip = at});
        }
    }
    stage_bar_->Show(crumbs, viewport_->StageScale());
}

void Window::RefreshTimelineBar() {
    if (timeline_bar_ == nullptr) return;
    QString label;
    for (const Document::AnimationLabel& one : shown_labels_) {
        if (one.frame <= frame_) label = QString::fromStdString(one.name);
    }
    timeline_bar_->Show(TimelineState{.frame = frame_,
                                      .frame_count = ClipFrameCount(),
                                      .rate = shown_rate_,
                                      .label = label,
                                      .work_area = work_area_,
                                      .zoom_pixels = timeline_->ZoomPixels()});
}

void Window::ShowGraphPanel(bool graph) {
    ads::CDockWidget* wanted = graph ? graph_dock_ : timeline_dock_;
    if (wanted == nullptr) return;
    wanted->toggleView(true);
    wanted->setAsCurrentTab();
}

std::optional<Placeable> Window::ChoosePlaceable(const AfpAnimation::Animation& animation) {
    std::vector<Placeable> choices;
    QStringList labels;
    for (const Document::CharacterSummary& one :
         Document::Characters(animation, file_->ShapeImages(animation_path_))) {
        choices.push_back(Placeable{.character = one.id, .image = {}});
        labels.append(QString::fromStdString(one.label));
    }
    for (const Document::Node& node : file_->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Texture) continue;
            choices.push_back(Placeable{.character = std::nullopt, .image = child.name});
            labels.append(
                tr("Package image %1, as a new shape").arg(QString::fromStdString(child.name)));
        }
    }
    if (choices.empty()) {
        ReportProblem(tr("Nothing in this package can be placed"));
        return std::nullopt;
    }
    bool answered = false;
    const QString picked =
        QInputDialog::getItem(this, tr("Add a depth"), tr("Place"), labels, 0, false, &answered);
    if (!answered) return std::nullopt;
    const auto index = labels.indexOf(picked);
    if (index < 0) return std::nullopt;
    return choices[static_cast<std::size_t>(index)];
}

void Window::AddDepthHere() {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportProblem(QString::fromStdString(animation.error()));
        return;
    }
    const uint32_t frames = ClipFrameCount();
    if (frames == 0) return;
    const std::optional<uint16_t> depth = NextFreeDepth(0);
    if (!depth) return;
    const std::optional<Placeable> choice = ChoosePlaceable(*animation);
    if (!choice) return;
    if (!choice->character) {
        PlaceImage(choice->image, Document::DepthSpan{.clip = clip_,
                                                      .depth = *depth,
                                                      .first_frame = frame_,
                                                      .last_frame = frames - 1});
        return;
    }
    AddCharacterDepth(*depth, *choice->character, frame_, frames - 1);
}

void Window::PlaceImage(const std::string& image, const Document::DepthSpan& span) {
    const std::string path = animation_path_;
    EditDocument(tr("Place %1 on depth %2").arg(QString::fromStdString(image)).arg(span.depth),
                 [path, image, span](Document::File& document) {
                     using Placed = Support::Expected<void, std::string>;
                     const auto shape = Document::PlaceImage(document, path, image, span);
                     if (!shape) return Placed(Support::Unexpected(shape.error()));
                     return Placed();
                 });
    ShowClipTimeline();
    ShowFrame();
}

void Window::FillClips() {
    clip_ = {};
    clip_index_ = 0;
    clips_.clear();
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    clips_ = Document::Clips(*animation);
    RefreshStageBar();
}

int Window::ClipIndexOf(const Document::ClipId& wanted) const {
    for (std::size_t at = 0; at < clips_.size(); at++) {
        if (clips_[at].id.sprite == wanted.sprite) return static_cast<int>(at);
    }
    return -1;
}

QString Window::ClipName(int index) const {
    if (index < 0 || index >= static_cast<int>(clips_.size())) return {};
    return QString::fromStdString(Document::ClipLabel(clips_[static_cast<std::size_t>(index)]));
}

void Window::RefillClipsKeepingChoice() {
    const Document::ClipId kept = clip_;
    FillClips();
    const int index = ClipIndexOf(kept);
    if (index < 0) return;
    clip_index_ = index;
    clip_ = kept;
    RefreshStageBar();
}

void Window::NameShownSpriteExport() {
    if (!file_ || animation_path_.empty() || !clip_.sprite) return;
    const uint16_t sprite = *clip_.sprite;
    const QString current =
        QString::fromStdString(Document::SpriteExportName(*file_, animation_path_, sprite));
    bool answered = false;
    const QString name =
        QInputDialog::getText(this, tr("Name the export"), tr("Export name (empty removes it)"),
                              QLineEdit::Normal, current, &answered);
    if (!answered || name == current) return;
    const std::string path = animation_path_;
    const std::string text = name.toStdString();
    if (!EditDocument(tr("Name sprite %1 %2").arg(sprite).arg(name),
                      [path, sprite, text](Document::File& document) {
                          return Document::NameSpriteExport(document, path, sprite, text);
                      })) {
        return;
    }
    RefillClipsKeepingChoice();
}

void Window::EnterSprite(uint16_t character) {
    const int index = ClipIndexOf(Document::ClipId{.sprite = character});
    if (index < 0) {
        ShowRefusal(tr("Sprite %1 is not a clip of this animation").arg(character));
        return;
    }
    ChooseClip(index);
}

void Window::EnterSpriteAt(double x, double y) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;
    const AfpAnimation::Container* clip = Document::FindClip(*animation, clip_);
    if (clip == nullptr) return;
    const std::optional<uint16_t> depth =
        Document::DepthAt(OutlinesOnStage(*animation), Document::Point{x, y});
    if (!depth) return;
    const std::vector<Document::DepthRow> rows = Document::DepthRows(*clip);
    const auto row = std::ranges::find(rows, *depth, &Document::DepthRow::depth);
    if (row == rows.end()) return;
    std::optional<uint16_t> character;
    for (const auto& [at, shown] : row->shows) {
        if (at <= frame_) character = shown;
    }
    if (!character) return;
    if (timeline_->IsSprite(*character)) {
        EnterSprite(*character);
        return;
    }
    ChooseDepth(*depth);
    const QString label = timeline_->CharacterLabel(*character);
    ShowResult(tr("%1 has no timeline of its own: its keyframes are on depth %2")
                   .arg(label.isEmpty() ? tr("Character %1").arg(*character) : label)
                   .arg(*depth),
               false);
}

void Window::LeaveClip() {
    ChooseClip(0);
}

void Window::ChooseClip(int index) {
    if (index < 0 || index >= static_cast<int>(clips_.size())) return;
    clip_index_ = index;
    clip_ = clips_[static_cast<std::size_t>(index)].id;
    StopPlayback();
    depth_.reset();
    key_property_.clear();
    key_frame_.reset();
    frame_ = clip_.sprite ? 0 : root_frame_;
    SetWorkArea(std::nullopt);
    ShowClipTimeline();
    if (!file_) {
        SeekViewport(frame_);
        ShowFrame();
        SayWhichClip(index);
        return;
    }
    LoadIntoHost(false, *file_, tr("Loading %1").arg(ClipName(index)), [this, index](bool) {
        ShowClipTimeline();
        SeekViewport(frame_);
        ShowFrame();
        SayWhichClip(index);
    });
    SeekViewport(frame_);
    ShowFrame();
}

void Window::SayWhichClip(int index) {
    if (!clip_.sprite) return;
    if (context_) {
        statusBar()->showMessage(tr("Editing %1 in place on the root, at frame %2")
                                     .arg(ClipName(index))
                                     .arg(root_frame_));
        return;
    }
    const bool wanted_in_place = context_action_ != nullptr && context_action_->isChecked();
    if (symbol_shown_) {
        statusBar()->showMessage(
            wanted_in_place
                ? tr("%1 is not placed on the root at frame %2, so it is shown on its own")
                      .arg(ClipName(index))
                      .arg(root_frame_)
                : tr("Showing %1 on its own").arg(ClipName(index)));
        return;
    }
    statusBar()->showMessage(tr("Editing %1. The viewport still shows the root animation, at "
                                "frame %2.")
                                 .arg(ClipName(index))
                                 .arg(root_frame_));
}

void Window::LoadIntoHost(bool fresh, Document::File document, const QString& what,
                          std::function<void(bool)> then) {
    if (!host_.Running() || animation_path_.empty()) {
        if (then) then(false);
        return;
    }
    if (host_busy_) {
        pending_load_ = PendingLoad{
            .fresh = fresh, .document = std::move(document), .what = what, .then = std::move(then)};
        return;
    }
    host_busy_ = true;
    symbol_shown_ = false;
    viewport_->ShowMessage(what);
    RefreshState();
    JobStarted();
    const HostRequest asked{.fresh = fresh,
                            .document = std::move(document),
                            .hidden = hidden_,
                            .animation_path = animation_path_,
                            .package_name = package_name_,
                            .animation_name = animation_name_,
                            .clip = clip_,
                            .wants_in_place =
                                context_action_ != nullptr && context_action_->isChecked(),
                            .root_frame = root_frame_};
    Jobs::Start<HostLoad>(
        this, pool_,
        [this, asked](QPromise<HostLoad>& promise) {
            promise.addResult(Editor::LoadIntoHost(host_, asked));
        },
        [this, then = std::move(then)](HostLoad load) {
            host_busy_ = false;
            JobFinished();
            if (!load.refusal.isEmpty()) ReportOnce(load.refusal);
            if (load.loaded) {
                frame_count_ = load.frame_count;
                symbol_shown_ = load.symbol_shown;
                ResizeViewport();
            }
            RefreshState();
            if (then) then(load.loaded);
            if (!pending_load_) return;
            PendingLoad next = std::move(*pending_load_);
            pending_load_.reset();
            LoadIntoHost(next.fresh, std::move(next.document), next.what, std::move(next.then));
        });
}

void Window::ShowClipTimeline() {
    if (!file_ || animation_path_.empty()) return;
    if (view_busy_) {
        view_again_ = true;
        return;
    }
    view_busy_ = true;
    timeline_->Waiting(tr("Reading %1").arg(QString::fromStdString(animation_name_)));
    JobStarted();
    Jobs::Start<ClipView>(
        this, pool_,
        [copy = *file_, path = animation_path_, clip = clip_](QPromise<ClipView>& promise) mutable {
            promise.addResult(ReadClipView(copy, path, clip));
        },
        [this](ClipView view) {
            view_busy_ = false;
            timeline_->Waiting(QString());
            JobFinished();
            ApplyClipView(std::move(view));
            if (!view_again_) return;
            view_again_ = false;
            ShowClipTimeline();
        });
}

void Window::ApplyClipView(ClipView view) {
    if (!view.read) {
        ReportOnce(view.refusal);
        return;
    }
    if (!view.has_clip) {
        FillClips();
        if (view_retried_) return;
        view_retried_ = true;
        ShowClipTimeline();
        return;
    }
    view_retried_ = false;
    shown_labels_ = view.labels;
    shown_rate_ = view.rate;
    FillLibrary(view.characters);
    timeline_->SetCharacterNames(std::move(view.names));
    timeline_->SetCharacterKinds(std::move(view.kinds));
    std::vector<uint16_t> keyed;
    for (const Document::AuthoredDepth& owned : authored_) {
        if (owned.animation == animation_path_ && owned.clip == clip_) keyed.push_back(owned.depth);
    }
    timeline_->SetFrameNotes(std::move(view.notes));
    timeline_->SetDepthMarks(std::move(view.marks));
    timeline_->SetKeyedDepths(std::move(keyed));
    model_frames_ = view.model_frames;
    const uint32_t count = ClipFrameCount();
    timeline_->ShowAnimation(count, std::move(view.depths), view.labels);
    UpdateViewRows();
    frame_ = count == 0 ? 0 : std::min(frame_, count - 1);
    timeline_->SetFrame(frame_);
    ShowKeysForDepth(depth_ ? AuthoredAt(static_cast<uint16_t>(*depth_), frame_) : nullptr);
    RefreshTimelineBar();
    RefreshStageBar();
}

void Window::ShowAnimation(const std::string& name) {
    StopPlayback();
    animation_name_ = name;
    frame_ = 0;
    SetWorkArea(std::nullopt);
    root_frame_ = 0;
    symbol_shown_ = false;
    depth_.reset();
    if (!host_.Running()) {
        viewport_->ShowMessage(
            tr("Choose a game install to preview %1").arg(QString::fromStdString(name)));
        frame_count_ = 0;
        FillClips();
        ShowClipTimeline();
        ShowFrame();
        return;
    }
    if (!file_) return;
    LoadIntoHost(true, *file_, tr("Loading %1").arg(QString::fromStdString(name)),
                 [this](bool loaded) {
                     if (!loaded) return;
                     FillClips();
                     ShowClipTimeline();
                     ResizeViewport();
                 });
}

bool Window::HostReady() const {
    return host_.Running() && !host_busy_;
}

void Window::SeekViewport(uint32_t frame) {
    if (!clip_.sprite) root_frame_ = frame;
    if (!HostReady()) return;
    const auto sought = host_.Seek(frame);
    if (!sought) {
        ReportOnce(QString::fromStdString(sought.error()));
        StopPlayback();
        return;
    }
    RenderFrame();
}

uint32_t Window::ClipFrameCount() const {
    const bool from_model = !host_.Running() || (clip_.sprite && !symbol_shown_);
    if (!from_model) return frame_count_;
    if (!file_ || animation_path_.empty()) return 0;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return 0;
    const auto details = Document::DescribeClip(*animation, clip_);
    return details ? details->frame_count : 0;
}

void Window::JumpToFrame(int64_t frame) {
    const uint32_t count = ClipFrameCount();
    if (count == 0) return;
    StopPlayback();
    SeekTo(static_cast<uint32_t>(std::clamp<int64_t>(frame, 0, count - 1)));
}

void Window::GoToFrame() {
    const uint32_t count = ClipFrameCount();
    if (count == 0) return;
    bool answered = false;
    const int frame =
        QInputDialog::getInt(this, tr("Go to frame"), tr("Frame"), static_cast<int>(frame_), 0,
                             static_cast<int>(count - 1), 1, &answered);
    if (answered) JumpToFrame(frame);
}

void Window::StepToMark(Document::Direction direction) {
    if (!file_ || animation_path_.empty() || !depth_) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    std::vector<uint32_t> marks;
    if (const Document::AuthoredDepth* owned = AuthoredAt(depth, frame_)) {
        for (const Document::Track& track : owned->tracks) {
            for (const Document::Keyframe& key : track.keys)
                marks.push_back(key.frame);
        }
        std::ranges::sort(marks);
        marks.erase(std::ranges::unique(marks).begin(), marks.end());
    } else {
        const auto animation = file_->ReadAnimation(animation_path_);
        if (!animation) return;
        const AfpAnimation::Container* clip = Document::FindClip(*animation, clip_);
        if (clip == nullptr) return;
        marks = Document::DepthMarks(*clip, depth);
    }
    const std::optional<uint32_t> next = Document::NextMark(marks, frame_, direction);
    if (next) JumpToFrame(*next);
}

std::optional<Document::Span> Window::WorkAreaToEdit() {
    if (const std::optional<QString> refused = WorkAreaRefusal()) {
        ReportProblem(*refused);
        return std::nullopt;
    }
    return Document::Span{.first_frame = work_area_->first_frame,
                          .last_frame = work_area_->last_frame};
}

void Window::ExtractWorkArea() {
    const std::optional<Document::Span> cut = WorkAreaToEdit();
    if (!cut) return;
    const Document::ClipId clip = clip_;
    const uint32_t removed = cut->last_frame - cut->first_frame + 1;
    uint32_t playhead = frame_;
    if (frame_ >= cut->first_frame)
        playhead = frame_ > cut->last_frame ? frame_ - removed : cut->first_frame;
    std::size_t moving = 0;
    if (!EditAnimation(tr("Extract frames %1 to %2").arg(cut->first_frame).arg(cut->last_frame),
                       [clip, &cut, &moving](AfpAnimation::Animation& edited)
                           -> Support::Expected<void, std::string> {
                           auto extracted = Document::ExtractFrames(edited, clip, *cut);
                           if (!extracted) return Support::Unexpected(extracted.error());
                           moving = *extracted;
                           return {};
                       })) {
        return;
    }
    SetWorkArea(std::nullopt);
    SeekTo(std::min(playhead, std::max<uint32_t>(ClipFrameCount(), 1) - 1));
    if (moving == 0) return;
    ShowResult(tr("%n depth(s) showing a sprite or another clip cross the cut, so their own "
                  "timelines no longer line up with the frames after it",
                  nullptr, static_cast<int>(moving)),
               true);
}

void Window::LiftWorkArea() {
    const std::optional<Document::Span> cut = WorkAreaToEdit();
    if (!cut) return;
    const Document::ClipId clip = clip_;
    std::size_t restarting = 0;
    if (!EditAnimation(tr("Lift frames %1 to %2").arg(cut->first_frame).arg(cut->last_frame),
                       [clip, &cut, &restarting](AfpAnimation::Animation& edited)
                           -> Support::Expected<void, std::string> {
                           auto lifted = Document::LiftFrames(edited, clip, *cut);
                           if (!lifted) return Support::Unexpected(lifted.error());
                           restarting = *lifted;
                           return {};
                       })) {
        return;
    }
    if (restarting == 0) return;
    ShowResult(tr("%n depth(s) showing a sprite or another clip start again after the lifted "
                  "frames",
                  nullptr, static_cast<int>(restarting)),
               true);
}

void Window::TrimClipToWorkArea() {
    const std::optional<Document::Span> work = WorkAreaToEdit();
    if (!work) return;
    const Document::ClipId clip = clip_;
    const Document::Span kept = *work;
    const uint32_t playhead =
        frame_ < kept.first_frame ? 0 : std::min(frame_, kept.last_frame) - kept.first_frame;
    std::size_t restarted = 0;
    if (!EditAnimation(
            tr("Trim the clip to frames %1 to %2").arg(kept.first_frame).arg(kept.last_frame),
            [clip, kept,
             &restarted](AfpAnimation::Animation& edited) -> Support::Expected<void, std::string> {
                auto trimmed = Document::TrimClipToFrames(edited, clip, kept);
                if (!trimmed) return Support::Unexpected(trimmed.error());
                restarted = *trimmed;
                return {};
            })) {
        return;
    }
    SetWorkArea(std::nullopt);
    SeekTo(playhead);
    if (restarted == 0) return;
    ShowResult(tr("%n depth(s) crossing frame %1 show a sprite or another clip, which now starts "
                  "again from its own first frame",
                  nullptr, static_cast<int>(restarted))
                   .arg(kept.first_frame),
               true);
}

void Window::SetWorkArea(std::optional<Document::WorkArea> area) {
    work_area_ = area;
    timeline_->SetWorkArea(area);
    RefreshTimelineBar();
}

void Window::SeekTo(uint32_t frame) {
    frame_ = frame;
    RefreshTimelineBar();
    if (sketch_) sketch_->offsets[frame] = sketch_->latest;
    timeline_->SetFrame(frame);
    graph_->SetFrame(frame);
    if (!clip_.sprite || symbol_shown_) SeekViewport(frame);
    if (!Playing()) ShowFrame();
}

void Window::Reload() {
    if (animation_name_.empty() || !file_) return;
    ShowClipTimeline();
    ShowFrame();
    LoadIntoHost(false, *file_, tr("Updating the preview"), [this](bool) {
        ShowClipTimeline();
        SeekViewport(symbol_shown_ ? frame_ : root_frame_);
        ShowFrame();
    });
}

}
