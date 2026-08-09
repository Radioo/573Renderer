#include "gui_gpu.h"
#include "gui_mock_assets.h"
#include "gui_test_harness.h"

#include "gc2d/gc_host.h"
#include "warp_device.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "scene3d/camera.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_host.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

void OpenHostTab(ImGuiTestContext* ctx, const char* tab) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/pane_right");
    ctx->ItemClick(tab);
    ctx->Yield(2);
}

}

TEST_CASE("2D package panel drives a loaded mock package", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("gc2d_pkg");
    GuiTest::WriteMock2dPackage(assets.path());

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("scene3d", "iidx17");
    REQUIRE(Gc2dHost::Load(assets.path()));
    REQUIRE(Gc2dHost::Active());

    Gc2dHost::Status const initial = Gc2dHost::GetStatus();
    CHECK(initial.cells == 2);
    CHECK(initial.animations == 2);
    CHECK(initial.tiles == 1);
    CHECK(initial.animation == "anim_intro");
    CHECK(initial.length == 30);

    ImGuiTest* test = harness.NewTest("gc2d_live");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/2D package");
        ctx->ItemClick("##inspector_tabs/2D package/Pause##gc2d");
        ctx->ItemInputValue("##inspector_tabs/2D package/speed##gc2d", 2.5F);
        ctx->ItemInputValue("##inspector_tabs/2D package/##gc2dframe", 12);
    };
    harness.Run(test);

    Gc2dHost::Status const after = Gc2dHost::GetStatus();
    CHECK(after.paused);
    CHECK(after.speed == 2.5F);
    CHECK(after.frame == 12);

    Gc2dHost::Unload();
}

TEST_CASE("2D package animation combo switches the playing animation", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("gc2d_anim");
    GuiTest::WriteMock2dPackage(assets.path());

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("scene3d", "iidx17");
    REQUIRE(Gc2dHost::Load(assets.path()));

    std::vector<std::string> const names = Gc2dHost::ListAnimations();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "anim_intro");
    CHECK(names[1] == "anim_loop");

    ImGuiTest* test = harness.NewTest("gc2d_anim_combo");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/2D package");
        GuiTest::ComboPick(ctx, "##inspector_tabs/2D package/##gc2danim", "anim_loop");
    };
    harness.Run(test);

    Gc2dHost::Status const after = Gc2dHost::GetStatus();
    CHECK(after.animation == "anim_loop");
    CHECK(after.length == 45);

    Gc2dHost::Unload();
}

TEST_CASE("3D scene panel drives a loaded mock scene", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("scene3d_basic");
    GuiTest::WriteMock3dScene(assets.path(), false);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    REQUIRE(Scene3dHost::Load(assets.path()));
    REQUIRE(Scene3dHost::Active());

    Scene3dHost::Status const initial = Scene3dHost::GetStatus();
    CHECK(initial.models == 1);
    CHECK(initial.tiles == 1);
    CHECK(initial.max_time == 600.0F);
    CHECK_FALSE(initial.has_authored_camera);

    ImGuiTest* test = harness.NewTest("scene3d_live");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ctx->ItemClick("##inspector_tabs/3D scene/Pause##s3d");
        ctx->ItemInputValue("##inspector_tabs/3D scene/speed##s3d", 0.5F);
        ctx->ItemInputValue("##inspector_tabs/3D scene/##s3dtime", 300.0F);
        ctx->ItemUncheck("##inspector_tabs/3D scene/Animate models##s3d");
    };
    harness.Run(test);

    Scene3dHost::Status const after = Scene3dHost::GetStatus();
    CHECK(after.paused);
    CHECK(after.speed == 0.5F);
    CHECK(after.time == 300.0F);
    CHECK_FALSE(after.animate_models);

    Scene3dHost::Unload();
}

TEST_CASE("3D scene camera checkbox is gated on an authored camera", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("scene3d_nocam");
    GuiTest::WriteMock3dScene(assets.path(), false);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    REQUIRE(Scene3dHost::Load(assets.path()));

    ImGuiTest* test = harness.NewTest("scene3d_camera_gated");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ImGuiTestItemInfo const info =
            ctx->ItemInfo("##inspector_tabs/3D scene/Animate camera##s3d");
        IM_CHECK_NE(info.ID, 0U);
        IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
    };
    harness.Run(test);

    Scene3dHost::Unload();
}

TEST_CASE("3D scene free camera and reset drive the camera state", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("scene3d_cam");
    GuiTest::WriteMock3dScene(assets.path(), true);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    REQUIRE(Scene3dHost::Load(assets.path()));
    REQUIRE(Scene3dHost::GetStatus().has_authored_camera);

    ImGuiTest* free_test = harness.NewTest("scene3d_free_camera");
    free_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ctx->ItemCheck("##inspector_tabs/3D scene/Free camera##s3d");
    };
    harness.Run(free_test);
    CHECK(Scene3dHost::GetStatus().free_camera);

    ImGuiTest* gate_test = harness.NewTest("scene3d_camera_gated_by_free");
    gate_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ImGuiTestItemInfo const info =
            ctx->ItemInfo("##inspector_tabs/3D scene/Animate camera##s3d");
        IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
    };
    harness.Run(gate_test);

    Scene3dHost::MutCamera().x = 1234.0F;
    ImGuiTest* reset_test = harness.NewTest("scene3d_reset_view");
    reset_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ctx->ItemClick("##inspector_tabs/3D scene/Reset view##s3d");
    };
    harness.Run(reset_test);
    CHECK(Scene3dHost::MutCamera().x != 1234.0F);

    Scene3dHost::Unload();
}

TEST_CASE("3D scene move speed drag edits the camera", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("scene3d_speed");
    GuiTest::WriteMock3dScene(assets.path(), false);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    REQUIRE(Scene3dHost::Load(assets.path()));

    ImGuiTest* test = harness.NewTest("scene3d_move_speed");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ctx->ItemInputValue("##inspector_tabs/3D scene/move speed##s3d", 42.0F);
    };
    harness.Run(test);

    CHECK(Scene3dHost::MutCamera().speed == 42.0F);

    Scene3dHost::Unload();
}

TEST_CASE("3D scene model list toggles visibility and blend mode", "[gui][hosts][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("scene3d_models");
    GuiTest::WriteMock3dScene(assets.path(), false);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    REQUIRE(Scene3dHost::Load(assets.path()));
    REQUIRE(Scene3dHost::ListModels().size() == 1);
    CHECK(Scene3dHost::ListModels()[0].name == "mock_model");

    ImGuiTest* test = harness.NewTest("scene3d_models");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenHostTab(ctx, "##inspector_tabs/3D scene");
        ctx->ItemClick("##inspector_tabs/3D scene/$$0/##vis");
        GuiTest::ComboPick(ctx, "##inspector_tabs/3D scene/$$0/##blend", "additive");
    };
    harness.Run(test);

    std::vector<Scene3dHost::ModelInfo> const models = Scene3dHost::ListModels();
    REQUIRE(models.size() == 1);
    CHECK_FALSE(models[0].visible);
    CHECK(models[0].blend_mode == Scene3d::kBlendAdditive);

    Scene3dHost::Unload();
}
