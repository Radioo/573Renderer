#pragma once

#include <cstdint>

namespace Loop {

class DissolveCycler {
public:
    explicit DissolveCycler(int loop_frames);

    struct Tick {
        bool cycle_changed = false;
        int cycle = 0;
    };

    Tick Advance(uint32_t cur, uint32_t total);

private:
    static constexpr uint32_t kUnseeded = 0xFFFFFFFF;
    int loop_frames_;
    int64_t frames_ = 0;
    uint32_t prev_cur_ = kUnseeded;
    int cycle_ = 0;
};

class FadeCycler {
public:
    FadeCycler(int fade_frames, int dwell_frames);

    struct Tick {
        bool fade_in = false;
        bool fade_out = false;
        int cycle = 0;
    };

    Tick Advance();

private:
    int period_;
    int fade_;
    int64_t count_ = 0;
    int cycle_ = 0;
    bool fade_out_fired_ = false;
};

}
