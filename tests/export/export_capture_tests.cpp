#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "backend/backend.h"
#include "export.h"
#include "export_capture.h"
#include "render_backend.h"
#include "export_internal.h"
#include "media/media_format.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/telemetry.h"

#include <any>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

class RefusingDriver final : public Export::ICaptureDriver {
public:
    void BeginCapture(Export::Session& sess) override {
        begins++;
        Export::FailSession(sess, "nothing loaded to bound the export on");
    }
    void TickCapture(Export::Session& sess, D3D9State& d3d) override {
        (void)sess;
        (void)d3d;
        ticks++;
    }
    void EndCapture(Export::Session& sess) override { (void)sess; }

    int begins = 0;
    int ticks = 0;
};

class StubBackend final : public Backend::IBackend {
public:
    [[nodiscard]] const char* Id() const override { return "stub"; }
    bool Boot(const Backend::BootEnv& env) override {
        (void)env;
        return true;
    }
    void Shutdown() override {}
    [[nodiscard]] bool ContentReady() const override { return true; }
    void StartContentScan() override {}
    bool LoadContent(const std::string& path, bool from_arc) override {
        (void)path;
        (void)from_arc;
        return true;
    }
    void UnloadContent() override {}
    void AdvanceFrame(float dt, int frame_count, bool exporting) override {
        (void)dt;
        (void)frame_count;
        (void)exporting;
    }
    void RenderScene(float dt, int frame_count) override {
        (void)dt;
        (void)frame_count;
    }
    void FillAutopilotInputs(Loop::AutopilotInputs& in) override { (void)in; }
    void BindSubmonitor() override {}
    bool HandleCommand(const std::any& payload) override {
        (void)payload;
        return false;
    }
    Export::ICaptureDriver& ExportDriver() override { return driver; }

    RefusingDriver driver;
};

StubBackend& TheStubBackend() {
    static StubBackend backend;
    return backend;
}

bool& StubBackendEnabled() {
    static bool enabled = false;
    return enabled;
}

}

namespace Backend {

IBackend* Active() {
    return StubBackendEnabled() ? &TheStubBackend() : nullptr;
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

TEST_CASE("a driver that refuses to start leaves the export failed, not capturing") {
    StubBackend& backend = TheStubBackend();
    backend.driver.begins = 0;
    backend.driver.ticks = 0;
    StubBackendEnabled() = true;

    Export::Session& sess = Export::ActiveSession();
    sess = {};
    D3D9State d3d;
    App::ExportRequest req;
    req.output_path = (OutDir("refused") / "out").string();
    req.format = MediaSink::ToIndex(MediaSink::Format::PNG_Sequence);
    req.fps = 60;

    Export::HandleStartRequest(req, d3d);

    CHECK(backend.driver.begins == 1);
    CHECK_FALSE(sess.active);
    CHECK_FALSE(Export::IsCapturing());

    const App::ExportState ex = App::Global().GetExport();
    CHECK(ex.phase == App::ExportPhase::Failed);
    CHECK(ex.error == "nothing loaded to bound the export on");

    Export::OnMainLoopTick(d3d);
    CHECK(backend.driver.ticks == 0);
    StubBackendEnabled() = false;
}

TEST_CASE("the planned length prefers the frame limit, then the preset, then the package") {
    CHECK(Export::PlannedFrames(240, 1800, 20) == 240);
    CHECK(Export::PlannedFrames(240, 0, 0) == 240);
    CHECK(Export::PlannedFrames(0, 1800, 20) == 1800);
    CHECK(Export::PlannedFrames(0, 0, 20) == 20);
    CHECK(Export::PlannedFrames(0, 0, 0) == 0);
    CHECK(Export::PlannedFrames(-5, -5, -5) == 0);
}
