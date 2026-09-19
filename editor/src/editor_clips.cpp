#include "editor_window.h"

#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/authored.h"
#include "document/characters.h"
#include "document/clip.h"
#include "document/clip_extract.h"
#include "document/clip_trim.h"
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
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QStringList>
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

QWidget* Window::BuildTimelinePanel(QScrollArea* timeline_area) {
    clip_box_ = new QComboBox;
    clip_box_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(clip_box_, &QComboBox::currentIndexChanged, this, &Window::ChooseClip);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(4, 2, 4, 2);
    header->addWidget(new QLabel(tr("Clip")));
    header->addWidget(clip_box_);
    header->addStretch();

    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(timeline_area);
    return panel;
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
    const QSignalBlocker blocked(clip_box_);
    clip_box_->clear();
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    for (const Document::ClipSummary& clip : Document::Clips(*animation)) {
        const QVariant sprite =
            clip.id.sprite ? QVariant(static_cast<int>(*clip.id.sprite)) : QVariant();
        clip_box_->addItem(QString::fromStdString(Document::ClipLabel(clip)), sprite);
    }
    clip_box_->setCurrentIndex(0);
}

void Window::RefillClipsKeepingChoice() {
    const Document::ClipId kept = clip_;
    FillClips();
    const QVariant wanted = kept.sprite ? QVariant(static_cast<int>(*kept.sprite)) : QVariant();
    const int index = clip_box_->findData(wanted);
    if (index < 0) return;
    const QSignalBlocker blocked(clip_box_);
    clip_box_->setCurrentIndex(index);
    clip_ = kept;
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

void Window::ChooseClip(int index) {
    if (index < 0) return;
    const QVariant sprite = clip_box_->itemData(index);
    clip_ = sprite.isValid() ? Document::ClipId{.sprite = static_cast<uint16_t>(sprite.toInt())}
                             : Document::ClipId{};
    StopPlayback();
    depth_.reset();
    key_property_.clear();
    key_frame_.reset();
    frame_ = clip_.sprite ? 0 : root_frame_;
    SetWorkArea(std::nullopt);
    if (file_) LoadViewportClip(*file_);
    ShowClipTimeline();
    SeekViewport(frame_);
    ShowFrame();
    if (!clip_.sprite) return;
    if (symbol_shown_) {
        statusBar()->showMessage(tr("Showing %1 on its own").arg(clip_box_->itemText(index)));
        return;
    }
    statusBar()->showMessage(tr("Editing %1. The viewport still shows the root animation, at "
                                "frame %2.")
                                 .arg(clip_box_->itemText(index))
                                 .arg(root_frame_));
}

bool Window::LoadViewportClip(const Document::File& document) {
    symbol_shown_ = false;
    if (!host_.Running() || animation_path_.empty()) return false;
    std::optional<Document::File> view;
    if (!hidden_.empty()) {
        auto filtered = Document::ViewWithout(document, hidden_);
        if (!filtered) {
            ReportOnce(QString::fromStdString(filtered.error()));
            return false;
        }
        view = std::move(*filtered);
    }
    const Document::File& file = view ? *view : document;
    std::vector<uint8_t> bytes;
    std::string symbol;
    if (clip_.sprite) {
        auto preview = Document::PreviewSymbolFor(file, animation_path_, clip_);
        if (!preview) {
            ReportOnce(QString::fromStdString(preview.error()));
            return false;
        }
        bytes = std::move(preview->ifs);
        symbol = std::move(preview->name);
    } else {
        auto encoded = file.Encode();
        if (!encoded) {
            ReportOnce(QString::fromStdString(encoded.error()));
            return false;
        }
        bytes = std::move(*encoded);
    }
    const auto loaded = host_.Reload(package_name_, animation_name_, bytes);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return false;
    }
    frame_count_ = loaded->frame_count;
    if (symbol.empty()) return true;
    const auto shown = host_.ShowSymbol(symbol);
    if (!shown) {
        ReportOnce(QString::fromStdString(shown.error()));
        return false;
    }
    frame_count_ = shown->frame_count;
    symbol_shown_ = true;
    return true;
}

void Window::ShowClipTimeline() {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const std::optional<Document::AnimationDetails> details =
        Document::DescribeClip(*animation, clip_);
    if (!details) {
        FillClips();
        ShowClipTimeline();
        return;
    }
    const std::vector<Document::CharacterSummary> characters =
        Document::Characters(*animation, file_->ShapeImages(animation_path_));
    FillLibrary(*animation, characters);
    std::map<uint16_t, QString> names;
    for (const Document::CharacterSummary& one : characters)
        names.emplace(one.id, QString::fromStdString(one.label));
    timeline_->SetCharacterNames(std::move(names));
    const uint32_t count = ClipFrameCount();
    timeline_->ShowAnimation(count, details->depths, details->labels);
    UpdateViewRows();
    frame_ = count == 0 ? 0 : std::min(frame_, count - 1);
    timeline_->SetFrame(frame_);
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
    const auto encoded = file_->Encode();
    if (!encoded) {
        ReportOnce(QString::fromStdString(encoded.error()));
        return;
    }
    const auto loaded = host_.ShowAnimation(package_name_, name, *encoded);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return;
    }
    frame_count_ = loaded->frame_count;
    FillClips();
    ShowClipTimeline();
    ResizeViewport();
}

void Window::SeekViewport(uint32_t frame) {
    if (!clip_.sprite) root_frame_ = frame;
    if (!host_.Running()) return;
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

void Window::AddStepActions(QMenu* menu) {
    struct StepAction {
        QString text;
        QKeySequence keys;
        std::function<void()> run;
    };
    const std::vector<StepAction> actions{
        {tr("&Previous frame"), QKeySequence(Qt::Key_PageUp),
         [this] { JumpToFrame(static_cast<int64_t>(frame_) - 1); }},
        {tr("&Next frame"), QKeySequence(Qt::Key_PageDown),
         [this] { JumpToFrame(static_cast<int64_t>(frame_) + 1); }},
        {tr("&First frame"), QKeySequence(Qt::Key_Home), [this] { JumpToFrame(0); }},
        {tr("L&ast frame"), QKeySequence(Qt::Key_End),
         [this] { JumpToFrame(static_cast<int64_t>(ClipFrameCount()) - 1); }},
        {tr("&Go to frame..."), QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_J),
         [this] { GoToFrame(); }},
        {tr("Previous &change on the depth"), QKeySequence(Qt::Key_J),
         [this] { StepToMark(Document::Direction::Back); }},
        {tr("Next c&hange on the depth"), QKeySequence(Qt::Key_K),
         [this] { StepToMark(Document::Direction::Forward); }},
        {tr("Start the &work area here"), QKeySequence(Qt::Key_B),
         [this] {
             SetWorkArea(Document::WithWorkAreaStart(work_area_, frame_, ClipFrameCount()));
         }},
        {tr("En&d the work area here"), QKeySequence(Qt::Key_N),
         [this] { SetWorkArea(Document::WithWorkAreaEnd(work_area_, frame_, ClipFrameCount())); }},
        {tr("Clear the work area"), QKeySequence(), [this] { SetWorkArea(std::nullopt); }},
    };
    menu->addSeparator();
    for (const StepAction& step : actions) {
        QAction* action = menu->addAction(step.text);
        action->setShortcut(step.keys);
        connect(action, &QAction::triggered, this, step.run);
    }
}

std::optional<Document::Span> Window::WorkAreaToEdit() {
    if (!file_ || animation_path_.empty()) return std::nullopt;
    if (!work_area_) {
        ReportProblem(tr("Mark the frames as the work area first, with B and N"));
        return std::nullopt;
    }
    const bool owned = std::ranges::any_of(authored_, [this](const Document::AuthoredDepth& one) {
        return one.animation == animation_path_ && one.clip == clip_;
    });
    if (owned) {
        ReportProblem(
            tr("The project owns depths in this clip. Detach them before cutting its frames."));
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
    statusBar()->showMessage(tr("%n depth(s) showing a sprite or another clip cross the cut, so "
                                "their own timelines no longer line up with the frames after it",
                                nullptr, static_cast<int>(moving)));
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
    statusBar()->showMessage(
        tr("%n depth(s) crossing frame %1 show a sprite or another clip, which "
           "now starts again from its own first frame",
           nullptr, static_cast<int>(restarted))
            .arg(kept.first_frame));
}

void Window::SetWorkArea(std::optional<Document::WorkArea> area) {
    work_area_ = area;
    timeline_->SetWorkArea(area);
}

void Window::SeekTo(uint32_t frame) {
    frame_ = frame;
    timeline_->SetFrame(frame);
    if (!clip_.sprite || symbol_shown_) SeekViewport(frame);
    if (!Playing()) ShowFrame();
}

void Window::Reload() {
    if (animation_name_.empty() || !file_) return;
    LoadViewportClip(*file_);
    ShowClipTimeline();
    SeekViewport(symbol_shown_ ? frame_ : root_frame_);
    ShowFrame();
}

}
