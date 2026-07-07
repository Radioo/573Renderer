#include "state/live_controls.h"

#include <algorithm>
#include <mutex>

namespace App {

void LiveControls::GetRenderSize(int& w, int& h) const {
    const std::scoped_lock lk(mu_);
    w = render_w_;
    h = render_h_;
}

void LiveControls::SetRenderSize(int w, int h) {
    const std::scoped_lock lk(mu_);
    if (w > 0) render_w_ = w;
    if (h > 0) render_h_ = h;
}

int LiveControls::GetRenderFps() const {
    const std::scoped_lock lk(mu_);
    return render_fps_;
}

void LiveControls::SetRenderFps(int fps) {
    const std::scoped_lock lk(mu_);
    if (fps > 0) render_fps_ = fps;
}

bool LiveControls::GetLoopMaster() const {
    const std::scoped_lock lk(mu_);
    return loop_master_;
}

void LiveControls::SetLoopMaster(bool on) {
    const std::scoped_lock lk(mu_);
    loop_master_ = on;
}

LiveControls::RootLoopMode LiveControls::GetRootLoopMode() const {
    const std::scoped_lock lk(mu_);
    return root_loop_mode_;
}

void LiveControls::SetRootLoopMode(RootLoopMode m) {
    const std::scoped_lock lk(mu_);
    root_loop_mode_ = m;
}

float LiveControls::GetMasterScale() const {
    const std::scoped_lock lk(mu_);
    return master_scale_;
}

void LiveControls::SetMasterScale(float s) {
    const std::scoped_lock lk(mu_);
    master_scale_ = std::clamp(s, 0.1F, 8.0F);
}

LiveControls::LiveOverrides LiveControls::GetLiveOverrides() const {
    const std::scoped_lock lk(mu_);
    return live_overrides_;
}

void LiveControls::SetLiveOverrides(LiveOverrides o) {
    const std::scoped_lock lk(mu_);
    o.continuous_loop_mode = std::clamp(o.continuous_loop_mode, -1, 1);
    o.trim_frames = std::max(o.trim_frames, 0);
    live_overrides_ = o;
}

LiveControls::LiveState LiveControls::GetLiveState() const {
    const std::scoped_lock lk(mu_);
    return live_state_;
}

void LiveControls::SetLiveState(const LiveState& s) {
    const std::scoped_lock lk(mu_);
    live_state_ = s;
}

CropRect LiveControls::GetCropRect() const {
    const std::scoped_lock lk(mu_);
    return crop_rect_;
}

void LiveControls::SetCropRect(CropRect r) {
    const std::scoped_lock lk(mu_);
    crop_rect_ = r;
}

bool LiveControls::GetCropPickMode() const {
    const std::scoped_lock lk(mu_);
    return crop_pick_mode_;
}

void LiveControls::SetCropPickMode(bool on) {
    const std::scoped_lock lk(mu_);
    crop_pick_mode_ = on;
}

}
