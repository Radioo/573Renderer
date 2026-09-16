#include "editor_window.h"

#include "document/playback.h"

#include <QAction>
#include <QStatusBar>
#include <QString>
#include <QTimer>

namespace Editor {

bool Window::Playing() const {
    return play_timer_ != nullptr && play_timer_->isActive();
}

void Window::TogglePlay() {
    if (Playing()) {
        StopPlayback();
        return;
    }
    if (clip_.sprite && !symbol_shown_) {
        statusBar()->showMessage(
            tr("This sprite is not showing on its own, so there is nothing of it to play"));
        return;
    }
    if (!file_ || animation_path_.empty() || frame_count_ == 0) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const double rate = Document::FrameRate(*animation);
    play_timer_->setInterval(Document::FrameIntervalMs(rate));
    play_timer_->start();
    play_action_->setText(tr("&Pause"));
    statusBar()->showMessage(tr("Playing %1 at %2 fps")
                                 .arg(QString::fromStdString(animation_name_))
                                 .arg(rate, 0, 'g', 4));
}

void Window::StepPlayback() {
    const Document::Step step = Document::Advance(
        Document::Playback{.frame_count = frame_count_, .looping = loop_action_->isChecked()},
        frame_);
    SeekTo(step.frame);
    if (!step.playing) StopPlayback();
}

void Window::StopPlayback() {
    if (play_timer_ == nullptr) return;
    const bool was_playing = play_timer_->isActive();
    play_timer_->stop();
    play_action_->setText(tr("&Play"));
    if (was_playing) ShowFrame();
}

}
