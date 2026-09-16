#include "editor_window.h"

#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/clip.h"
#include "document/outline.h"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

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
    play_action_->setEnabled(!clip_.sprite);
    ShowClipTimeline();
    ShowFrame();
    if (clip_.sprite) {
        statusBar()->showMessage(tr("Editing %1. The viewport still shows the root animation, at "
                                    "frame %2.")
                                     .arg(clip_box_->itemText(index))
                                     .arg(root_frame_));
    }
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
    const uint32_t count = clip_.sprite ? details->frame_count : frame_count_;
    timeline_->ShowAnimation(count, details->depths, details->labels);
    frame_ = count == 0 ? 0 : std::min(frame_, count - 1);
    timeline_->SetFrame(frame_);
}

void Window::ShowAnimation(const std::string& name) {
    StopPlayback();
    animation_name_ = name;
    if (!host_.Running()) {
        viewport_->ShowMessage(
            tr("Choose a game install to preview %1").arg(QString::fromStdString(name)));
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
    frame_ = 0;
    root_frame_ = 0;
    depth_.reset();
    FillClips();
    play_action_->setEnabled(true);
    ShowClipTimeline();
    ResizeViewport();
}

void Window::SeekViewport(uint32_t frame) {
    root_frame_ = frame;
    if (!host_.Running()) return;
    const auto sought = host_.Seek(frame);
    if (!sought) {
        ReportOnce(QString::fromStdString(sought.error()));
        StopPlayback();
        return;
    }
    RenderFrame();
}

void Window::SeekTo(uint32_t frame) {
    frame_ = frame;
    timeline_->SetFrame(frame);
    if (!clip_.sprite) SeekViewport(frame);
    if (!Playing()) ShowFrame();
}

void Window::Reload() {
    if (!host_.Running() || animation_name_.empty() || !file_) return;
    const auto encoded = file_->Encode();
    if (!encoded) {
        ReportOnce(QString::fromStdString(encoded.error()));
        return;
    }
    const auto loaded = host_.Reload(package_name_, animation_name_, *encoded);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return;
    }
    frame_count_ = loaded->frame_count;
    ShowClipTimeline();
    SeekViewport(root_frame_);
    ShowFrame();
}

}
