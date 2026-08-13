#include "gui_test_harness.h"

#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::filesystem::path MakeFingerprintedInstall() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "r573_gui_iidx10";
    std::error_code ec;
    std::filesystem::create_directories(root / "JAE", ec);
    std::ofstream f(root / "JAE" / "bm2dx.exe", std::ios::binary);
    const std::vector<char> bytes((size_t)860160, 'A');
    f.write(bytes.data(), (std::streamsize)bytes.size());
    return root;
}

}

TEST_CASE("screens tab stays hidden for a directory with no known build", "[gui][presets]") {
    GuiTest::Harness harness;
    App::Global().SetGameDir("C:/games/not-a-konami-dump");
    GuiTest::EnterReadyView("scene3d", "iidx17");

    ImGuiTest* test = harness.NewTest("presets_unknown_build");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Screens") == false);
    };
    harness.Run(test);
}

TEST_CASE("screens tab lists the presets of a fingerprinted build", "[gui][presets]") {
    const std::filesystem::path root = MakeFingerprintedInstall();
    GuiTest::Harness harness;
    App::Global().SetGameDir(root.string());
    GuiTest::EnterReadyView("scene3d", "iidx11");

    ImGuiTest* test = harness.NewTest("presets_known_build");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Screens"));
        ctx->ItemClick("##inspector_tabs/Screens");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Screens/Music select"));
    };
    harness.Run(test);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
