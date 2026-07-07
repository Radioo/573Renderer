#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

namespace App {

enum class BootState : std::uint8_t {
    WaitingForDir,
    Booting,
    Ready,
    Failed,
};

struct LoadProgress {
    std::string stage;
    std::string target;
    std::string detail;
    int textures_expected = 0;
    int textures_loaded = 0;
    float fraction = -1.0F;
    bool active = false;
};

class BootLifecycle {
public:
    static constexpr int kLoadMinHoldMs = 600;

    [[nodiscard]] BootState GetBootState() const;
    void SetBootState(BootState s);

    [[nodiscard]] std::string GetBootError() const;
    void SetBootError(std::string msg);

    [[nodiscard]] std::string GameDir() const;
    void SetGameDir(std::string dir);

    [[nodiscard]] std::string GetGameProfileSlug() const;
    void SetGameProfileSlug(std::string slug);

    [[nodiscard]] bool IsDdrMode() const;
    void SetIsDdrMode(bool on);

    [[nodiscard]] LoadProgress GetLoadProgress() const;
    void SetLoadProgress(LoadProgress p);
    void BeginLoad(std::string target);
    void UpdateLoadStage(std::string stage, float fraction = -1.0F);
    void EndLoad();
    void SetLoadDetail(std::string detail);
    void SetTexturesExpected(int n);
    void BumpTexturesLoaded();

private:
    mutable std::mutex mu_;
    std::chrono::steady_clock::time_point load_ended_at_;
    std::string game_dir_;
    std::string boot_error_;
    std::string game_profile_slug_;
    LoadProgress progress_;
    BootState boot_state_{BootState::WaitingForDir};
    bool is_ddr_mode_{false};
};

}
