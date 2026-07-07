#include <catch2/catch_test_macros.hpp>

#include "loop/ddr_loop_detector.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace {

std::vector<uint8_t> SolidBgra(int px, uint8_t g) {
    std::vector<uint8_t> v(static_cast<std::size_t>(px) * 4, 0);
    for (int i = 0; i < px; i++)
        v[(static_cast<std::size_t>(i) * 4) + 1] = g;
    return v;
}

struct FakeSink {
    int submits = 0;
    bool ok = true;
    bool operator()([[maybe_unused]] uint8_t* bgra, [[maybe_unused]] int w,
                    [[maybe_unused]] int h) {
        submits++;
        return ok;
    }
};

}

TEST_CASE("DDR pre-roll skips the one-shot intro below the loop label") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    const std::vector<uint8_t> frame = SolidBgra(64, 0);
    const Loop::DdrFeed in{.bgra = frame,
                           .w = 8,
                           .h = 8,
                           .current_frame = 5,
                           .loop_label_frame = 10,
                           .loop_count = 1,
                           .label_active = false};
    const Loop::DdrResult r = det.Feed(in, std::ref(sink));
    CHECK_FALSE(r.finished);
    CHECK(sink.submits == 0);
    CHECK(det.LoopsDone() == 0);
}

TEST_CASE("DDR afp authored loop detects a clean current_frame wrap") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    const int loop_label = 10;
    auto feed = [&](int cf) {
        const std::vector<uint8_t> frame = SolidBgra(64, 0);
        const Loop::DdrFeed in{.bgra = frame,
                               .w = 8,
                               .h = 8,
                               .current_frame = cf,
                               .loop_label_frame = loop_label,
                               .loop_count = 1,
                               .label_active = false};
        return det.Feed(in, std::ref(sink));
    };
    feed(5);
    feed(12);
    feed(20);
    feed(30);
    const Loop::DdrResult r = feed(13);
    CHECK(r.finished);
    CHECK(r.reason == Loop::DdrReason::AfpAuthoredLoop);
    CHECK(r.loops_done == 1);
}

TEST_CASE("DDR afp loop counts multiple cycles before finishing") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    const int loop_label = 10;
    auto feed = [&](int cf) {
        const std::vector<uint8_t> frame = SolidBgra(64, 0);
        const Loop::DdrFeed in{.bgra = frame,
                               .w = 8,
                               .h = 8,
                               .current_frame = cf,
                               .loop_label_frame = loop_label,
                               .loop_count = 2,
                               .label_active = false};
        return det.Feed(in, std::ref(sink));
    };
    feed(12);
    feed(30);
    const Loop::DdrResult first = feed(13);
    CHECK_FALSE(first.finished);
    feed(30);
    const Loop::DdrResult second = feed(13);
    CHECK(second.finished);
    CHECK(second.loops_done == 2);
}

TEST_CASE("DDR label export ends on a frozen current_frame") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    const int loop_label = 10;
    auto feed = [&]() {
        const std::vector<uint8_t> frame = SolidBgra(64, 0);
        const Loop::DdrFeed in{.bgra = frame,
                               .w = 8,
                               .h = 8,
                               .current_frame = 12,
                               .loop_label_frame = loop_label,
                               .loop_count = 1,
                               .label_active = true};
        return det.Feed(in, std::ref(sink));
    };
    Loop::DdrResult r;
    for (int i = 0; i < 12 && !r.finished; i++)
        r = feed();
    CHECK(r.finished);
    CHECK(r.reason == Loop::DdrReason::LabelEnded);
}

TEST_CASE("DDR content fallback detects a visible loop") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    auto feed = [&](uint8_t g) {
        const std::vector<uint8_t> frame = SolidBgra(64, g);
        const Loop::DdrFeed in{.bgra = frame,
                               .w = 8,
                               .h = 8,
                               .current_frame = -1,
                               .loop_label_frame = -1,
                               .loop_count = 1,
                               .label_active = false};
        return det.Feed(in, std::ref(sink));
    };
    feed(0);
    for (int i = 0; i < 62; i++)
        feed(100);
    feed(0);
    const Loop::DdrResult r = feed(100);
    CHECK(r.finished);
    CHECK(r.reason == Loop::DdrReason::ContentLoop);
    CHECK(r.loops_done == 1);
}

TEST_CASE("DDR content fallback does not loop before the minimum period") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    auto feed = [&](uint8_t g) {
        const std::vector<uint8_t> frame = SolidBgra(64, g);
        const Loop::DdrFeed in{.bgra = frame,
                               .w = 8,
                               .h = 8,
                               .current_frame = -1,
                               .loop_label_frame = -1,
                               .loop_count = 1,
                               .label_active = false};
        return det.Feed(in, std::ref(sink));
    };
    feed(0);
    feed(100);
    feed(0);
    const Loop::DdrResult r = feed(100);
    CHECK_FALSE(r.finished);
}

TEST_CASE("DDR detector stops when the sink fails") {
    Loop::DdrLoopDetector det;
    FakeSink sink;
    sink.ok = false;
    auto feed = [&](uint8_t g) {
        const std::vector<uint8_t> frame = SolidBgra(64, g);
        const Loop::DdrFeed in{.bgra = frame,
                               .w = 8,
                               .h = 8,
                               .current_frame = -1,
                               .loop_label_frame = -1,
                               .loop_count = 1,
                               .label_active = false};
        return det.Feed(in, std::ref(sink));
    };
    feed(0);
    const Loop::DdrResult r = feed(50);
    CHECK_FALSE(r.finished);
}
