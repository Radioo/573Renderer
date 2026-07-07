#include "state/boot_lifecycle.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <utility>

namespace App {

BootState BootLifecycle::GetBootState() const {
    const std::scoped_lock lk(mu_);
    return boot_state_;
}

void BootLifecycle::SetBootState(BootState s) {
    const std::scoped_lock lk(mu_);
    boot_state_ = s;
}

std::string BootLifecycle::GetBootError() const {
    const std::scoped_lock lk(mu_);
    return boot_error_;
}

void BootLifecycle::SetBootError(std::string msg) {
    const std::scoped_lock lk(mu_);
    boot_error_ = std::move(msg);
}

std::string BootLifecycle::GameDir() const {
    const std::scoped_lock lk(mu_);
    return game_dir_;
}

void BootLifecycle::SetGameDir(std::string dir) {
    const std::scoped_lock lk(mu_);
    game_dir_ = std::move(dir);
}

std::string BootLifecycle::GetGameProfileSlug() const {
    const std::scoped_lock lk(mu_);
    return game_profile_slug_;
}

void BootLifecycle::SetGameProfileSlug(std::string slug) {
    const std::scoped_lock lk(mu_);
    game_profile_slug_ = std::move(slug);
}

bool BootLifecycle::IsDdrMode() const {
    const std::scoped_lock lk(mu_);
    return is_ddr_mode_;
}

void BootLifecycle::SetIsDdrMode(bool on) {
    const std::scoped_lock lk(mu_);
    is_ddr_mode_ = on;
}

LoadProgress BootLifecycle::GetLoadProgress() const {
    const std::scoped_lock lk(mu_);
    LoadProgress p = progress_;

    if (p.active && p.textures_expected > 0) {
        const float f =
            static_cast<float>(p.textures_loaded) / static_cast<float>(p.textures_expected);
        p.fraction = std::clamp(f, 0.0F, 1.0F);
    }

    if (!p.active && load_ended_at_.time_since_epoch().count() != 0) {
        const auto elapsed = std::chrono::steady_clock::now() - load_ended_at_;
        if (elapsed < std::chrono::milliseconds(kLoadMinHoldMs)) {
            p.active = true;
            p.stage = "Finalizing";
            p.fraction = 1.0F;
        }
    }

    return p;
}

void BootLifecycle::SetLoadProgress(LoadProgress p) {
    const std::scoped_lock lk(mu_);
    progress_ = std::move(p);
}

void BootLifecycle::BeginLoad(std::string target) {
    const std::scoped_lock lk(mu_);
    progress_ = {};
    progress_.active = true;
    progress_.target = std::move(target);
    load_ended_at_ = {};
}

void BootLifecycle::UpdateLoadStage(std::string stage, float fraction) {
    const std::scoped_lock lk(mu_);
    progress_.active = true;
    progress_.stage = std::move(stage);
    if (fraction >= 0.0F) progress_.fraction = fraction;
}

void BootLifecycle::EndLoad() {
    const std::scoped_lock lk(mu_);
    progress_.active = false;
    load_ended_at_ = std::chrono::steady_clock::now();
}

void BootLifecycle::SetLoadDetail(std::string detail) {
    const std::scoped_lock lk(mu_);
    progress_.detail = std::move(detail);
}

void BootLifecycle::SetTexturesExpected(int n) {
    const std::scoped_lock lk(mu_);
    progress_.textures_expected = n;
}

void BootLifecycle::BumpTexturesLoaded() {
    const std::scoped_lock lk(mu_);
    progress_.textures_loaded++;
}

}
