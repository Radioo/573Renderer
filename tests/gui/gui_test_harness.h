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

void SetBrowseResult(std::string path);

std::string TakeRevealedPath();

}
