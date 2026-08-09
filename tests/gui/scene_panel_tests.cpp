#include "gui_test_harness.h"

#include "backend/afp_commands.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <any>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

App::SubLayerNode MakeLeaf(const char* name, const char* path) {
    App::SubLayerNode node;
    node.name = name;
    node.path = path;
    node.enumerated = true;
    return node;
}

void ReadyWithTree(const char* ifs) {
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene(ifs, 0, 300);

    auto& state = App::Global();
    auto& cfg = state.MutConfig(ifs);
    cfg.filename = ifs;
    cfg.anim_names = {"bg_main", "bg_alt"};
    cfg.bitmap_names = {"tex_a", "tex_b"};

    App::Status status = state.GetStatus();
    status.playing_animation = "bg_main";
    status.mc_tree = MakeLeaf("root", "root");
    status.mc_tree.enumerated = true;
    status.mc_tree.children.push_back(MakeLeaf("coin", "coin"));
    status.mc_tree.children.push_back(MakeLeaf("star", "star"));
    status.mc_children.push_back({.name = "coin", .x = 12.0F, .y = 34.0F, .have_pos = true});
    state.SetStatus(status);
}

void ClearFilter(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/pane_center");
    ctx->ItemInputValue("##scene_filter", "");
    ctx->ItemInputValue("##new_slot", "");
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

TEST_CASE("scene pane prompts for an IFS while none is active", "[gui][scene]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("scene_no_ifs");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        IM_CHECK(ctx->ItemExists("##scene_filter") == false);
        IM_CHECK(ctx->ItemExists("##new_slot") == false);
    };
    harness.Run(test);
}

TEST_CASE("scene pane reports an IFS with no listed layers", "[gui][scene]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_empty.ifs", 0, 300);
    App::Global().MutConfig("bg_empty.ifs").filename = "bg_empty.ifs";

    ImGuiTest* test = harness.NewTest("scene_no_layers");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        IM_CHECK(ctx->ItemExists("##scene_filter") == false);
    };
    harness.Run(test);
}

TEST_CASE("scene pane double-click on a layer posts SwitchAnimation", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_dbl.ifs");

    ImGuiTest* test = harness.NewTest("scene_layer_dblclick");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        ctx->ItemDoubleClick("$$1/##layer");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* sw = TakeAfpCommand<AfpCmd::SwitchAnimation>(slot);
    REQUIRE(sw != nullptr);
    CHECK(sw->name == "bg_alt");
}

TEST_CASE("scene pane child visibility checkbox records a sublayer override", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_vis.ifs");

    ImGuiTest* test = harness.NewTest("scene_child_visibility");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        ctx->ItemClick("$$0/##layer/$$0/##vis");
    };
    harness.Run(test);

    auto const overrides = App::Global().GetSublayerOverrides("bg_vis.ifs");
    REQUIRE(overrides.size() == 1);
    CHECK(overrides[0].first == "coin");
    CHECK_FALSE(overrides[0].second);
}

TEST_CASE("scene pane child tree expansion is recorded", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_expand.ifs");
    auto& state = App::Global();
    App::Status status = state.GetStatus();
    status.mc_tree.children[0].children.push_back(MakeLeaf("inner", "coin/inner"));
    state.SetStatus(status);

    ImGuiTest* test = harness.NewTest("scene_child_expand");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        GuiTest::ClickTreeArrow(ctx, "$$0/##layer/$$0/coin");
        ImGuiTestItemInfo const after = ctx->ItemInfo("$$0/##layer/$$0/coin");
        IM_CHECK((after.StatusFlags & ImGuiItemStatusFlags_Opened) != 0);
    };
    harness.Run(test);

    auto const expanded = App::Global().GetSublayerExpanded();
    CHECK(std::ranges::find(expanded, "coin") != expanded.end());
}

TEST_CASE("scene pane filter narrows the layer list", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_filter.ifs");

    ImGuiTest* test = harness.NewTest("scene_filter");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemInputValue("##scene_filter", "alt");
        ctx->Yield(2);
        GuiTest::FocusChild(ctx, "scene_scroll");
        IM_CHECK(ctx->ItemExists("$$1/##layer"));
        IM_CHECK(ctx->ItemExists("$$0/##layer") == false);
        ClearFilter(ctx);
    };
    harness.Run(test);
}

TEST_CASE("scene pane Add registers a variant slot", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_slot.ifs");

    ImGuiTest* test = harness.NewTest("scene_add_slot");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemInputValue("##new_slot", "coin");
        ctx->ItemClick("Add");
    };
    harness.Run(test);

    const App::IfsConfig* cfg = App::Global().FindConfig("bg_slot.ifs");
    REQUIRE(cfg != nullptr);
    REQUIRE(cfg->slots.size() == 1);
    CHECK(cfg->slots[0].path == "coin");
    CHECK(cfg->slots[0].default_bitmap == "coin");
    CHECK_FALSE(cfg->slots[0].is_valid);
}

TEST_CASE("scene pane Add ignores an empty slot name", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_slot_empty.ifs");

    ImGuiTest* test = harness.NewTest("scene_add_slot_empty");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemClick("Add");
    };
    harness.Run(test);

    const App::IfsConfig* cfg = App::Global().FindConfig("bg_slot_empty.ifs");
    REQUIRE(cfg != nullptr);
    CHECK(cfg->slots.empty());
}

TEST_CASE("scene pane Add refuses a duplicate slot path", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_slot_dup.ifs");
    {
        auto& cfg = App::Global().MutConfig("bg_slot_dup.ifs");
        App::VariantSlot slot;
        slot.path = "coin";
        cfg.slots.push_back(std::move(slot));
    }

    ImGuiTest* test = harness.NewTest("scene_add_slot_dup");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemInputValue("##new_slot", "coin");
        ctx->ItemClick("Add");
    };
    harness.Run(test);

    const App::IfsConfig* cfg = App::Global().FindConfig("bg_slot_dup.ifs");
    REQUIRE(cfg != nullptr);
    CHECK(cfg->slots.size() == 1);
}

TEST_CASE("selected clip with a slot exposes the slot controls", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_slotsel.ifs");
    {
        auto& cfg = App::Global().MutConfig("bg_slotsel.ifs");
        App::VariantSlot slot;
        slot.path = "coin";
        slot.default_bitmap = "tex_a";
        slot.is_valid = true;
        cfg.slots.push_back(std::move(slot));
    }

    ImGuiTest* test = harness.NewTest("scene_slot_properties");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        ctx->ItemClick("$$0/##layer/$$0/coin");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Properties");
        ctx->Yield(2);
        ctx->ItemClick("##inspector_tabs/Properties/Slot visible");
    };
    harness.Run(test);

    const App::IfsConfig* cfg = App::Global().FindConfig("bg_slotsel.ifs");
    REQUIRE(cfg != nullptr);
    REQUIRE(cfg->slots.size() == 1);
    CHECK_FALSE(cfg->slots[0].visible);
}

TEST_CASE("slot bitmap combo picks an override and restores the default", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_slotbmp.ifs");
    {
        auto& cfg = App::Global().MutConfig("bg_slotbmp.ifs");
        App::VariantSlot slot;
        slot.path = "coin";
        slot.default_bitmap = "tex_a";
        slot.is_valid = true;
        cfg.slots.push_back(std::move(slot));
    }

    ImGuiTest* pick = harness.NewTest("scene_slot_bitmap_pick");
    pick->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        ctx->ItemClick("$$0/##layer/$$0/coin");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Properties");
        ctx->Yield(2);
        GuiTest::ComboPick(ctx, "##inspector_tabs/Properties/##bitmap", "tex_b");
    };
    harness.Run(pick);

    {
        const App::IfsConfig* cfg = App::Global().FindConfig("bg_slotbmp.ifs");
        REQUIRE(cfg != nullptr);
        REQUIRE(cfg->slots.size() == 1);
        CHECK(cfg->slots[0].bitmap == "tex_b");
        CHECK(cfg->slots[0].bitmap_override);
    }

    ImGuiTest* restore = harness.NewTest("scene_slot_bitmap_default");
    restore->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        GuiTest::ComboPick(ctx, "##inspector_tabs/Properties/##bitmap", "(default)");
    };
    harness.Run(restore);

    const App::IfsConfig* cfg = App::Global().FindConfig("bg_slotbmp.ifs");
    REQUIRE(cfg != nullptr);
    CHECK(cfg->slots[0].bitmap.empty());
    CHECK_FALSE(cfg->slots[0].bitmap_override);

    std::optional<App::Command> slot;
    const auto* replay = TakeAfpCommand<AfpCmd::ForceReplay>(slot);
    CHECK(replay != nullptr);
}

TEST_CASE("slot bitmap falls back to a text field with no listed bitmaps", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_slottext.ifs");
    {
        auto& cfg = App::Global().MutConfig("bg_slottext.ifs");
        cfg.bitmap_names.clear();
        App::VariantSlot slot;
        slot.path = "coin";
        slot.is_valid = true;
        cfg.slots.push_back(std::move(slot));
    }

    ImGuiTest* test = harness.NewTest("scene_slot_bitmap_text");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        ctx->ItemClick("$$0/##layer/$$0/coin");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Properties");
        ctx->Yield(2);
        ctx->ItemInputValue("##inspector_tabs/Properties/##bitmap", "custom_tex");
    };
    harness.Run(test);

    const App::IfsConfig* cfg = App::Global().FindConfig("bg_slottext.ifs");
    REQUIRE(cfg != nullptr);
    REQUIRE(cfg->slots.size() == 1);
    CHECK(cfg->slots[0].bitmap == "custom_tex");
    CHECK(cfg->slots[0].bitmap_override);
}

TEST_CASE("unresolved slots are listed under the playing layer", "[gui][scene]") {
    GuiTest::Harness harness;
    ReadyWithTree("bg_unresolved.ifs");
    {
        auto& cfg = App::Global().MutConfig("bg_unresolved.ifs");
        App::VariantSlot slot;
        slot.path = "ghost";
        slot.is_valid = false;
        cfg.slots.push_back(std::move(slot));
    }

    ImGuiTest* test = harness.NewTest("scene_unresolved_slot");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "scene_scroll");
        ctx->ItemClick("$$0/##layer/ghost/   ghost");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Properties");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Properties/Slot visible"));
    };
    harness.Run(test);
}
