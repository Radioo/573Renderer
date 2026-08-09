#include "gui_test_harness.h"

#include "gui_panels.h"
#include "gui_style.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "native_dialog.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"
#include "state/telemetry.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#ifdef Yield
#undef Yield
#endif

namespace GuiTest {

namespace {

constexpr int kFrameBudget = 4000;

std::string& BrowseResult() {
    static std::string value;
    return value;
}

std::string& RevealedPath() {
    static std::string value;
    return value;
}

std::string BrowseStub(const std::string& initial) {
    (void)initial;
    return BrowseResult();
}

bool RevealStub(const std::string& path) {
    RevealedPath() = path;
    return true;
}

void ResetAppState() {
    auto& state = App::Global();
    while (state.TakeCommand().has_value()) {
    }
    state.SetBootState(App::BootState::WaitingForDir);
    state.SetBootError({});
    state.SetGameDir({});
    state.SetGameProfileSlug({});
    state.SetActiveBackendId({});
    state.SetActiveIfs({});
    state.SetAvailableIfs({});
    state.SetStatus({});
    state.SetLiveState({});
    state.SetLiveOverrides({});
    state.SetExport({});
    state.SetLoadProgress({});
    state.SetCropRect({});
    state.SetCropPickMode(false);
    state.SetRenderSize(1280, 720);
    state.SetRenderFps(120);
}

ImGuiContext* CreateUiContext() {
    ResetAppState();
    BrowseResult().clear();
    RevealedPath().clear();
    NativeDialog::SetOverrides(
        {.browse_for_folder = &BrowseStub, .reveal_in_file_manager = &RevealStub});

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1600.0F, 900.0F);
    io.DeltaTime = 1.0F / 60.0F;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    Gui::LoadFonts();
    Gui::ApplyStyle();
    return ImGui::GetCurrentContext();
}

void ServiceTextures() {
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
        if (tex->Status == ImTextureStatus_WantCreate) {
            tex->SetTexID(static_cast<ImTextureID>(static_cast<uintptr_t>(tex->UniqueID) + 1));
            tex->SetStatus(ImTextureStatus_OK);
        } else if (tex->Status == ImTextureStatus_WantUpdates) {
            tex->SetStatus(ImTextureStatus_OK);
        } else if (tex->Status == ImTextureStatus_WantDestroy) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

}

Harness::Harness() : ui_(CreateUiContext()), engine_(ImGuiTestEngine_CreateContext()) {
    ImGuiTestEngineIO& te_io = ImGuiTestEngine_GetIO(engine_);
    te_io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    te_io.ConfigNoThrottle = true;
    te_io.ConfigSavedSettings = false;
    te_io.ConfigCaptureEnabled = false;
    te_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Warning;
    te_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    ImGuiTestEngine_Start(engine_, ui_);
}

Harness::~Harness() {
    ImGuiTestEngine_Stop(engine_);
    ImGui::DestroyContext(ui_);
    ImGuiTestEngine_DestroyContext(engine_);
    NativeDialog::SetOverrides({});
}

ImGuiTest* Harness::NewTest(const char* name) {
    return ImGuiTestEngine_RegisterTest(engine_, "gui", name);
}

void Harness::Frame() {
    ImGui::NewFrame();
    Panels::Build();
    ImGui::Render();
    ServiceTextures();
    ImGuiTestEngine_PostSwap(engine_);
}

void Harness::Run(ImGuiTest* test) {
    ImGuiTestEngine_QueueTest(engine_, test);

    int frames = 0;
    while (!ImGuiTestEngine_IsTestQueueEmpty(engine_) && frames < kFrameBudget) {
        Frame();
        frames++;
    }

    const char* log_text = test->Output.Log.GetText();
    std::string const log = log_text != nullptr ? log_text : "";
    INFO(log);
    REQUIRE(frames < kFrameBudget);
    REQUIRE(test->Output.Status == ImGuiTestStatus_Success);
}

void FocusChild(ImGuiTestContext* ctx, const char* child_path) {
    ImGuiTestItemInfo const info = ctx->WindowInfo(child_path);
    IM_CHECK_SILENT(info.Window != nullptr);
    ctx->SetRef(info.Window);
}

void ComboPick(ImGuiTestContext* ctx, const char* combo_path, const char* item_label) {
    ctx->ItemClick(combo_path);
    ctx->Yield(2);
    ctx->ItemClick(ctx->GetID(item_label, ctx->GetID("//$FOCUSED")));
    ctx->Yield(3);
}

void ClickTreeArrow(ImGuiTestContext* ctx, const char* item_path) {
    ctx->MouseMove(item_path);
    ImGuiTestItemInfo const info = ctx->ItemInfo(item_path);
    IM_CHECK_SILENT(info.ID != 0);
    ImVec2 const arrow(info.RectFull.Min.x + (ImGui::GetFontSize() * 0.5F),
                       (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F);
    ctx->MouseMoveToPos(arrow);
    ctx->MouseClick(0);
    ctx->Yield(2);
}

void SetDisplaySize(float w, float h) {
    ImGui::GetIO().DisplaySize = ImVec2(w, h);
}

bool TooltipShown(ImGuiTestContext* ctx) {
    ImGuiContext const& g = *ctx->UiContext;
    return std::ranges::any_of(g.Windows, [](const ImGuiWindow* w) {
        return w->Active && strncmp(w->Name, "##Tooltip", 9) == 0;
    });
}

bool HoverShowsTooltip(ImGuiTestContext* ctx, const char* item_path) {
    ctx->MouseMove(item_path);
    ctx->Yield(4);
    return TooltipShown(ctx);
}

void EnterReadyView(const char* backend_id, const char* profile_slug) {
    auto& state = App::Global();
    state.SetBootState(App::BootState::Ready);
    state.SetActiveBackendId(backend_id);
    state.SetGameProfileSlug(profile_slug);
}

void LoadScene(const char* ifs_path, unsigned cur, unsigned total) {
    auto& state = App::Global();
    App::Status status = state.GetStatus();
    status.scene_loaded = true;
    status.current_ifs_path = ifs_path;
    state.SetStatus(status);
    state.SetActiveIfs(ifs_path);

    App::State::LiveState live = state.GetLiveState();
    live.mc_cur = cur;
    live.mc_total = total;
    live.have_mc_playhead = true;
    state.SetLiveState(live);
}

void SetBrowseResult(std::string path) {
    BrowseResult() = std::move(path);
}

std::string TakeRevealedPath() {
    return std::exchange(RevealedPath(), std::string{});
}

}
