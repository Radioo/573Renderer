#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "backend/backend.h"
#include "export_internal.h"
#include "media/media_format.h"
#include "state/app_state.h"
#include "state/telemetry.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace Backend {

IBackend* Active() {
    return nullptr;
}

}

namespace {

namespace fs = std::filesystem;

constexpr int kW = 64;
constexpr int kH = 48;
constexpr int kLoopLen = 40;

fs::path OutDir(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / "r573_export_capture_tests" / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

std::vector<uint8_t> LoopFrame(int frame_index) {
    const int phase = frame_index % kLoopLen;
    std::vector<uint8_t> bgra((std::size_t)kW * kH * 4);
    for (int y = 0; y < kH; y++) {
        for (int x = 0; x < kW; x++) {
            const std::size_t o = (((std::size_t)y * kW) + x) * 4;
            bgra[o + 0] = (uint8_t)(((x * 3) + (phase * 20)) & 0xFF);
            bgra[o + 1] = (uint8_t)(((y * 7) + (phase * 11)) & 0xFF);
            bgra[o + 2] = (uint8_t)((x + y + phase) & 0xFF);
            bgra[o + 3] = 255;
        }
    }
    return bgra;
}

int CountPngs(const fs::path& dir) {
    int pngs = 0;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.path().extension() == ".png") pngs++;
    }
    return pngs;
}

}

TEST_CASE("blend-loop capture buffers synthetic frames and composes a looped encode") {
    const fs::path dir = OutDir("blend_seq");

    Export::Session& sess = Export::ActiveSession();
    sess = {};
    sess.active = true;
    sess.blend_loop = true;
    sess.blend_frames = 4;
    sess.fps = 30;
    sess.quality = 40;
    sess.format = (int)MediaSink::Format::PNG_Sequence;
    sess.prefer_hw = false;
    sess.output_path = (dir / "loop").string();

    for (int i = 0; i < (kLoopLen * 2) + 15; i++) {
        std::vector<uint8_t> frame = LoopFrame(i);
        Export::SubmitOneFrame(sess, frame.data(), kW, kH);
    }
    CHECK(sess.frames_captured == (kLoopLen * 2) + 15);
    CHECK((int)sess.blend_buf.size() == (kLoopLen * 2) + 15);

    Export::FinishAndEncode(sess);

    const App::ExportState ex = App::Global().GetExport();
    INFO("error: " << ex.error);
    CHECK(ex.phase == App::ExportPhase::Done);
    CHECK_FALSE(sess.active);
    CHECK(sess.frames_captured == kLoopLen);
    CHECK(CountPngs(dir / "loop") == kLoopLen);
}

TEST_CASE("crop path submits the cropped region straight to the sink") {
    const fs::path dir = OutDir("crop_seq");

    Export::Session& sess = Export::ActiveSession();
    sess = {};
    sess.active = true;
    sess.fps = 30;
    sess.quality = 40;
    sess.format = (int)MediaSink::Format::PNG_Sequence;
    sess.prefer_hw = false;
    sess.crop_x = 8;
    sess.crop_y = 8;
    sess.crop_w = 16;
    sess.crop_h = 12;
    sess.output_path = (dir / "crop").string();

    for (int i = 0; i < 3; i++) {
        std::vector<uint8_t> frame = LoopFrame(i);
        Export::SubmitOneFrame(sess, frame.data(), kW, kH);
    }
    CHECK(sess.frames_captured == 3);

    Export::FinishAndEncode(sess);

    const App::ExportState ex = App::Global().GetExport();
    INFO("error: " << ex.error);
    CHECK(ex.phase == App::ExportPhase::Done);
    CHECK_FALSE(sess.active);
    CHECK(CountPngs(dir / "crop") == 3);
}
