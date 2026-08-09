#include "gui_test_harness.h"

#include "gui_panels.h"
#include "gui_style.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "native_dialog.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <utility>

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

void SetBrowseResult(std::string path) {
    BrowseResult() = std::move(path);
}

std::string TakeRevealedPath() {
    return std::exchange(RevealedPath(), std::string{});
}

}
