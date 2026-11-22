#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "afp_ddr.h"
#include "afp_boot.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "backend/afp_profiles.h"
#include "engine_dlls.h"
#include "render_backend.h"
#include "support/env.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kRenderWidth = 720;
constexpr int kRenderHeight = 1280;
constexpr const char* kPackage = "data/graphics/bg_space.bin";

std::string GameDir() {
    std::optional<std::string> dir = Support::EnvVar("R573_SDVX1_DIR");
    return dir ? *dir : std::string();
}

HWND HiddenWindow() {
    const char* kClass = "r573_ddr_txp2_test";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "ddr txp2", WS_OVERLAPPED, 0, 0, kRenderWidth, kRenderHeight,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

void BootSdvx1(const std::string& dir) {
    const AfpProfiles::AfpConfig* config = AfpProfiles::For("sdvx1");
    REQUIRE(config != nullptr);
    AfpManager::SetActiveConfig(config);
    const std::string dll_dir = EngineDlls::DiscoverDllDir(dir, *config);
    REQUIRE(!dll_dir.empty());
    REQUIRE(EngineDlls::Load(g_engine, dll_dir, *config, true));
    REQUIRE(AvsManager::Boot(g_avs));
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    g_d3d.width = kRenderWidth;
    g_d3d.height = kRenderHeight;
    REQUIRE(g_d3d.Init(hwnd));
    REQUIRE(DdrAfp::Boot(g_afp_dll, g_afpu_dll, g_d3d));
}

}

TEST_CASE("Every clip of an SDVX 1 TXP2 package plays with a readable playhead") {
    const std::string dir = GameDir();
    if (dir.empty()) SKIP("R573_SDVX1_DIR not set");
    BootSdvx1(dir);

    REQUIRE(DdrAfp::LoadTxp2(g_avs, (std::filesystem::path(dir) / kPackage).string()));
    const std::vector<DdrAfp::Label> opening_labels = DdrAfp::EnumerateLabels();
    CHECK(opening_labels.empty());

    const std::vector<std::string> names = DdrAfp::ClipNames();
    CHECK(names ==
          std::vector<std::string>{"bg_ginga", "bg_space", "bg_space_eff", "bg_space_moto"});

    for (const std::string& name : names) {
        INFO("clip " << name);
        REQUIRE(DdrAfp::SwitchClip(name));
        DdrAfp::RenderFrame(1.0F / 60.0F);
        uint32_t cur = 0;
        uint32_t total = 0;
        CHECK(DdrAfp::ReadPlayhead(&cur, &total, nullptr));
        CHECK(total > 0);
    }

    DdrAfp::Shutdown();
}
