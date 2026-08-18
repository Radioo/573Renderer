#pragma once

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"

#include <string>

namespace GuiTest {

class Harness {
public:
    Harness();
    ~Harness();
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;
    Harness(Harness&&) = delete;
    Harness& operator=(Harness&&) = delete;

    ImGuiTest* NewTest(const char* name);
    void Run(ImGuiTest* test);

private:
    void Frame();

    ImGuiContext* ui_ = nullptr;
    ImGuiTestEngine* engine_ = nullptr;
};

void FocusChild(ImGuiTestContext* ctx, const char* child_path);

void ComboPick(ImGuiTestContext* ctx, const char* combo_path, const char* item_label);

void ClickTreeArrow(ImGuiTestContext* ctx, const char* item_path);

void SetDisplaySize(float w, float h);

bool TooltipShown(ImGuiTestContext* ctx);

std::string HoverAndCaptureText(ImGuiTestContext* ctx, const char* item_path);

std::string CaptureFrameText(ImGuiTestContext* ctx);

bool HoverShowsTooltip(ImGuiTestContext* ctx, const char* item_path);

void SetBrowseResult(std::string path);

void SetOpenFileResult(std::string path);

void SetSaveFileResult(std::string path);

std::string TakeRevealedPath();

void EnterReadyView(const char* backend_id, const char* profile_slug);

void LoadScene(const char* ifs_path, unsigned cur, unsigned total);

}
