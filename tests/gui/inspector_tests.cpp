#include "gui_test_harness.h"

#include "backend/afp_commands.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/ifs_catalog.h"
#include "state/live_controls.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

void OpenTab(ImGuiTestContext* ctx, const char* tab_path) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/pane_right");
    ctx->ItemClick(tab_path);
    ctx->Yield(2);
}

void ReadyWithLayers(const char* ifs) {
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene(ifs, 0, 300);

    auto& state = App::Global();
    auto& cfg = state.MutConfig(ifs);
    cfg.filename = ifs;
    cfg.anim_names = {"bg_main", "bg_alt"};
    cfg.bitmap_names = {"tex_a", "tex_b"};

    App::Status status = state.GetStatus();
    status.playing_animation = "bg_main";
    state.SetStatus(status);
}

template <typename T> const T* TakeAfpCommand(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    const auto* wrapped = std::get_if<App::Cmd::BackendCommand>(&*slot);
    if (wrapped == nullptr) return nullptr;
    const auto* payload = std::any_cast<AfpCmd::Any>(&wrapped->payload);
    if (payload == nullptr) return nullptr;
    return std::get_if<T>(payload);
}

}

TEST_CASE("properties tab prompts for a selection while nothing is picked", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_sel.ifs");

    ImGuiTest* test = harness.NewTest("insp_no_selection");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Properties");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Properties/Play") == false);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Properties/Replay") == false);
    };
    harness.Run(test);
}

TEST_CASE("properties tab Replay posts SwitchAnimation for the playing layer", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_replay.ifs");

    ImGuiTest* test = harness.NewTest("insp_replay");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center/scene_scroll");
        ctx->ItemClick("$$0/##layer");
        OpenTab(ctx, "##inspector_tabs/Properties");
        ctx->ItemClick("##inspector_tabs/Properties/Replay");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* sw = TakeAfpCommand<AfpCmd::SwitchAnimation>(slot);
    REQUIRE(sw != nullptr);
    CHECK(sw->name == "bg_main");
    CHECK(sw->label.empty());
}

TEST_CASE("properties tab Play posts SwitchAnimation for an idle layer", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_play.ifs");

    ImGuiTest* test = harness.NewTest("insp_play");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center/scene_scroll");
        ctx->ItemClick("$$1/##layer");
        OpenTab(ctx, "##inspector_tabs/Properties");
        ctx->ItemClick("##inspector_tabs/Properties/Play");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* sw = TakeAfpCommand<AfpCmd::SwitchAnimation>(slot);
    REQUIRE(sw != nullptr);
    CHECK(sw->name == "bg_alt");
}

TEST_CASE("render tab loop-master checkbox persists to state", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_loopmaster.ifs");
    App::Global().SetLoopMaster(false);

    ImGuiTest* test = harness.NewTest("insp_loop_master");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/Loop master animation");
    };
    harness.Run(test);

    CHECK(App::Global().GetLoopMaster());
}

TEST_CASE("render tab root-loop segmented switches to force mode", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_rootloop.ifs");
    App::Global().SetRootLoopMode(App::State::RootLoopMode::Hold);

    ImGuiTest* test = harness.NewTest("insp_root_loop");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/##root_loop/$$1/Force loop");
    };
    harness.Run(test);

    CHECK(App::Global().GetRootLoopMode() == App::State::RootLoopMode::Force);
}

TEST_CASE("render tab root-loop segmented switches back to auto-hold", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_roothold.ifs");
    App::Global().SetRootLoopMode(App::State::RootLoopMode::Force);

    ImGuiTest* test = harness.NewTest("insp_root_loop_hold");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/##root_loop/$$0/Auto-hold");
    };
    harness.Run(test);

    CHECK(App::Global().GetRootLoopMode() == App::State::RootLoopMode::Hold);
}

TEST_CASE("render tab continuous-loop segmented applies each mode", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_cont.ifs");

    ImGuiTest* on_test = harness.NewTest("insp_cont_on");
    on_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/##live_cont/$$2/ON");
    };
    harness.Run(on_test);
    CHECK(App::Global().GetLiveOverrides().continuous_loop_mode == 1);

    ImGuiTest* off_test = harness.NewTest("insp_cont_off");
    off_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/##live_cont/$$0/OFF");
    };
    harness.Run(off_test);
    CHECK(App::Global().GetLiveOverrides().continuous_loop_mode == -1);
}

TEST_CASE("render tab trim input feeds the live overrides", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_trim.ifs");

    ImGuiTest* test = harness.NewTest("insp_trim");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemInputValue("##inspector_tabs/Render/##live_trim", 240);
    };
    harness.Run(test);

    CHECK(App::Global().GetLiveOverrides().trim_frames == 240);
}

TEST_CASE("render tab master-scale preset buttons set the scale", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_scalebtn.ifs");

    ImGuiTest* wide = harness.NewTest("insp_scale_15");
    wide->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/1.5x##scale_sdvx_old");
    };
    harness.Run(wide);
    CHECK(App::Global().GetMasterScale() == 1.5F);

    ImGuiTest* reset = harness.NewTest("insp_scale_reset");
    reset->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/1.0x##scale_reset");
    };
    harness.Run(reset);
    CHECK(App::Global().GetMasterScale() == 1.0F);
}

TEST_CASE("render tab master-scale slider sets an arbitrary scale", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_scaleslider.ifs");

    ImGuiTest* test = harness.NewTest("insp_scale_slider");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemInputValue("##inspector_tabs/Render/##master_scale", 2.5F);
    };
    harness.Run(test);

    CHECK(App::Global().GetMasterScale() == 2.5F);
}

TEST_CASE("render tab background segmented selects a preview colour", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_bg.ifs");

    ImGuiTest* test = harness.NewTest("insp_bg_red");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/##bg_color/$$3/red");
    };
    harness.Run(test);

    CHECK(App::Global().GetLiveOverrides().bg_color_index == 2);
}

TEST_CASE("render tab filter checkbox toggles the live override", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_filter.ifs");

    ImGuiTest* test = harness.NewTest("insp_filter");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/Filter (F7)");
    };
    harness.Run(test);

    CHECK(App::Global().GetLiveOverrides().filter_enabled);
}

TEST_CASE("render tab MC-names checkbox reveals the name-type segmented", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_mcnames.ifs");

    ImGuiTest* test = harness.NewTest("insp_mc_names");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render/##mc_name_type/$$1/column") == false);
        ctx->ItemClick("##inspector_tabs/Render/Show MC names (F3)");
        ctx->Yield(2);
        ctx->ItemClick("##inspector_tabs/Render/##mc_name_type/$$1/column");
    };
    harness.Run(test);

    auto const ov = App::Global().GetLiveOverrides();
    CHECK(ov.show_mc_names);
    CHECK(ov.mc_name_type == 1);
}

TEST_CASE("render tab reset button clears every live override", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_reset.ifs");
    App::State::LiveOverrides ov;
    ov.filter_enabled = true;
    ov.trim_frames = 99;
    ov.bg_color_index = 3;
    App::Global().SetLiveOverrides(ov);

    ImGuiTest* test = harness.NewTest("insp_reset_overrides");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        ctx->ItemClick("##inspector_tabs/Render/Reset live overrides");
    };
    harness.Run(test);

    auto const after = App::Global().GetLiveOverrides();
    CHECK_FALSE(after.filter_enabled);
    CHECK(after.trim_frames == 0);
    CHECK(after.bg_color_index == -1);
}

TEST_CASE("ddr render tab offers background and reset only", "[gui][inspector]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    GuiTest::LoadScene("bg_ddr.arc", 0, 120);

    ImGuiTest* test = harness.NewTest("insp_ddr_render_tab");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Render");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render/Loop master animation") == false);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render/Filter (F7)") == false);
        ctx->ItemClick("##inspector_tabs/Render/##bg_color/$$2/black");
    };
    harness.Run(test);

    CHECK(App::Global().GetLiveOverrides().bg_color_index == 1);
}

TEST_CASE("live tab lists MC names only in column mode", "[gui][inspector]") {
    GuiTest::Harness harness;
    ReadyWithLayers("bg_live.ifs");
    auto& state = App::Global();
    App::Status status = state.GetStatus();
    status.mc_children.push_back({.name = "coin", .x = 10.0F, .y = 20.0F, .have_pos = true});
    state.SetStatus(status);

    ImGuiTest* off_test = harness.NewTest("insp_live_no_names");
    off_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Live");
        IM_CHECK(ctx->WindowInfo("##inspector_tabs/Live/mc_names_list", ImGuiTestOpFlags_NoError)
                     .Window == nullptr);
    };
    harness.Run(off_test);

    App::State::LiveOverrides ov;
    ov.show_mc_names = true;
    ov.mc_name_type = 1;
    state.SetLiveOverrides(ov);

    ImGuiTest* on_test = harness.NewTest("insp_live_names");
    on_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenTab(ctx, "##inspector_tabs/Live");
        IM_CHECK(ctx->WindowInfo("##inspector_tabs/Live/mc_names_list").Window != nullptr);
    };
    harness.Run(on_test);
}
