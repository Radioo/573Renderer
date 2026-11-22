#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "app_globals.h"
#include "judge_engine/judge_engine.h"
#include "render_backend.h"
#include "support/env.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kStageW = 720;
constexpr int kStageH = 1280;
constexpr float kStep = 1.0F / 60.0F;
constexpr float kHalfStep = 1.0F / 120.0F;
constexpr int kWarmUpSteps = 30;
constexpr int kSampledSteps = 10;

std::string GameDir() {
    std::optional<std::string> dir = Support::EnvVar("R573_SDVX1_DIR");
    return dir ? *dir : std::string();
}

HWND HiddenWindow() {
    const char* kClass = "r573_judge_interp_test";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "judge interp", WS_OVERLAPPED, 0, 0, kStageW, kStageH,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

std::vector<std::uint8_t> Draw(JudgeEngine::Side side) {
    g_d3d.BeginFrame();
    JudgeEngine::DrawSide(side);
    g_d3d.EndFrame();
    std::vector<std::uint8_t> pixels;
    int w = 0;
    int h = 0;
    REQUIRE(g_d3d.ReadOffscreenBGRA(pixels, w, h));
    return pixels;
}

}

TEST_CASE("Interpolated frames move between authored steps while the authored side holds") {
    const std::string dir = GameDir();
    if (dir.empty()) SKIP("R573_SDVX1_DIR not set");
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    g_d3d.width = kStageW;
    g_d3d.height = kStageH;
    REQUIRE(g_d3d.Init(hwnd));

    const JudgeEngine::BootResult boot =
        JudgeEngine::Boot(dir, "sdvx1", g_d3d.device, kStageW, kStageH);
    INFO(boot.error << boot.interp_problem);
    REQUIRE(boot.error.empty());
    REQUIRE(boot.interp_problem.empty());
    REQUIRE(boot.fixed_step_seconds == kStep);

    const std::string package =
        (std::filesystem::path(dir) / "data/graphics/bg_space.bin").string();
    REQUIRE(JudgeEngine::OpenClip(package, "bg_space").empty());
    for (int i = 0; i < kWarmUpSteps; i++)
        JudgeEngine::Advance(kStep);

    int authored_moved = 0;
    int interpolated_moved = 0;
    for (int i = 0; i < kSampledSteps; i++) {
        JudgeEngine::Advance(kStep);
        JudgeEngine::SetRenderTime(0.0F);
        const auto authored_at_step = Draw(JudgeEngine::Side::Authored);
        const auto interpolated_at_step = Draw(JudgeEngine::Side::Interpolated);
        JudgeEngine::SetRenderTime(kHalfStep);
        const auto authored_half_way = Draw(JudgeEngine::Side::Authored);
        const auto interpolated_half_way = Draw(JudgeEngine::Side::Interpolated);
        if (authored_at_step != authored_half_way) authored_moved++;
        if (interpolated_at_step != interpolated_half_way) interpolated_moved++;
    }
    CHECK(JudgeEngine::InterpolationStats().moving > 0);
    CHECK(authored_moved == 0);
    CHECK(interpolated_moved == kSampledSteps);

    JudgeEngine::Shutdown();
    g_d3d.Shutdown();
    DestroyWindow(hwnd);
}
