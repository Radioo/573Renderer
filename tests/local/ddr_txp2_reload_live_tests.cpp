#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "afp_ddr.h"
#include "afp_boot.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "backend/afp_profiles.h"
#include "engine_dlls.h"
#include "judge/clip_catalog.h"
#include "render_backend.h"
#include "support/env.h"

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>

namespace {

constexpr int kRenderWidth = 720;
constexpr int kRenderHeight = 1280;
constexpr int kFramesPerPackage = 30;

HWND HiddenWindow() {
    const char* kClass = "r573_ddr_txp2_reload_test";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "ddr txp2 reload", WS_OVERLAPPED, 0, 0, kRenderWidth,
                           kRenderHeight, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

}

TEST_CASE("Every SDVX 1 package loads right after the previous one was played") {
    const std::optional<std::string> dir = Support::EnvVar("R573_SDVX1_DIR");
    if (!dir) SKIP("R573_SDVX1_DIR not set");
    const AfpProfiles::AfpConfig* config = AfpProfiles::For("sdvx1");
    REQUIRE(config != nullptr);
    AfpManager::SetActiveConfig(config);
    REQUIRE(EngineDlls::Load(g_engine, EngineDlls::DiscoverDllDir(*dir, *config), *config, true));
    REQUIRE(AvsManager::Boot(g_avs));
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    g_d3d.width = kRenderWidth;
    g_d3d.height = kRenderHeight;
    REQUIRE(g_d3d.Init(hwnd));
    REQUIRE(DdrAfp::Boot(g_afp_dll, g_afpu_dll, g_d3d));

    const std::string graphics = (std::filesystem::path(*dir) / "data" / "graphics").string();
    int loaded = 0;
    int packages = 0;
    for (const std::string& file : Judge::ListPackageFiles(graphics)) {
        std::string err;
        const auto clips = Judge::ReadClipNames(file, err);
        if (!clips || clips->empty()) continue;
        packages++;
        INFO(file);
        REQUIRE(DdrAfp::LoadTxp2(g_avs, file));
        loaded++;
        for (int i = 0; i < kFramesPerPackage; i++) {
            g_d3d.BeginFrame();
            DdrAfp::RenderFrame(1.0F / 60.0F);
            g_d3d.EndFrame();
        }
    }
    CHECK(packages > 0);
    CHECK(loaded == packages);
    DdrAfp::Shutdown();
    g_d3d.Shutdown();
    DestroyWindow(hwnd);
}
