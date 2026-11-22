#pragma once

#include <array>
#include <cstddef>

namespace Judge {

inline constexpr std::array<float, 5> kTimeScales{1.0F, 0.5F, 0.25F, 0.1F, 0.05F};
inline constexpr int kRendersPerLogicStep = 2;

class Clock {
public:
    explicit Clock(float step_seconds) : step_(step_seconds) {}

    [[nodiscard]] int Tick(float real_seconds);
    [[nodiscard]] int StepRender();
    void Restart();

    void SetPaused(bool paused) { paused_ = paused; }
    [[nodiscard]] bool Paused() const { return paused_; }

    void SetScaleIndex(std::size_t index);
    [[nodiscard]] std::size_t ScaleIndex() const { return scale_index_; }
    [[nodiscard]] float Scale() const { return kTimeScales.at(scale_index_); }

    [[nodiscard]] float SinceAdvance() const { return since_advance_; }
    [[nodiscard]] float Step() const { return step_; }

private:
    float step_;
    float since_advance_ = 0.0F;
    bool paused_ = false;
    std::size_t scale_index_ = 0;
};

}
