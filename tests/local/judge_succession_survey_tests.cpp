#include <catch2/catch_test_macros.hpp>

#include "app_globals.h"
#include "judge/clip_catalog.h"
#include "judge/interp_tracks.h"
#include "judge_engine/judge_engine.h"
#include "judge_hooks/afp_interp_hooks.h"
#include "render_backend.h"
#include "support/crash_report.h"
#include "support/env.h"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr int kStageW = 720;
constexpr int kStageH = 1280;
constexpr int kAdvancesPerClip = 240;

enum Kind : std::size_t {
    kSameSlotContiguous,
    kSameSlotGap,
    kSameCharacterOtherDepth,
    kSameCharacterOtherDepthContiguous,
    kHandover,
    kChildOfNew,
    kNoRemovedSibling,
    kNotOneStep,
    kKinds
};

constexpr std::array<const char*, kKinds> kKindNames{
    "same parent, depth and character, span contiguous",
    "same parent, depth and character, span not contiguous",
    "same parent and character, other depth, span not contiguous",
    "same parent and character, other depth, span contiguous",
    "other character whose span starts where a removed sibling's ended",
    "child of an object created in the same advance",
    "no removed sibling",
    "created in an advance that was not one frame step"};

struct Tally {
    std::array<std::size_t, kKinds> kinds{};
    std::size_t moved = 0;
    std::size_t advances = 0;
};

Tally g_clip;
std::vector<std::string> g_pairs;

Kind Classify(const AfpInterp::Sample& created,
              const std::unordered_map<std::uintptr_t, const AfpInterp::Sample*>& before,
              const std::vector<const AfpInterp::Sample*>& removed) {
    if (!before.contains(created.parent) && created.parent != 0) return kChildOfNew;
    bool same_slot_gap = false;
    bool other_depth = false;
    bool other_depth_contiguous = false;
    bool handover = false;
    for (const AfpInterp::Sample* old : removed) {
        if (old->parent != created.parent) continue;
        const bool same_character = old->pose.character == created.pose.character;
        if (same_character && old->pose.depth == created.pose.depth) {
            if (old->lifetime_end == created.created) return kSameSlotContiguous;
            same_slot_gap = true;
        } else if (same_character) {
            other_depth = true;
            if (old->lifetime_end == created.created) {
                other_depth_contiguous = true;
                if (g_pairs.size() < 40) {
                    g_pairs.push_back(std::format(
                        "depth {} -> {} frames {}..{} -> {}..{} x {:.1f} -> {:.1f} y {:.1f} -> "
                        "{:.1f} "
                        "a {:.3f} -> {:.3f} colour {} -> {}",
                        old->pose.depth, created.pose.depth, old->created, old->lifetime_end,
                        created.created, created.lifetime_end, old->pose.matrix[4],
                        created.pose.matrix[4], old->pose.matrix[5], created.pose.matrix[5],
                        old->pose.matrix[0], created.pose.matrix[0],
                        old->pose.has_color ? old->pose.color[3] : 1.0F,
                        created.pose.has_color ? created.pose.color[3] : 1.0F));
                }
            }
        } else if (old->lifetime_end == created.created) {
            handover = true;
        }
    }
    if (same_slot_gap) return kSameSlotGap;
    if (other_depth_contiguous) return kSameCharacterOtherDepthContiguous;
    if (other_depth) return kSameCharacterOtherDepth;
    if (handover) return kHandover;
    return kNoRemovedSibling;
}

void Observe(std::span<const AfpInterp::Sample> before, std::span<const AfpInterp::Sample> after,
             int steps) {
    g_clip.advances++;
    std::unordered_map<std::uintptr_t, const AfpInterp::Sample*> was;
    for (const AfpInterp::Sample& s : before)
        was.emplace(s.object, &s);
    std::unordered_set<std::uintptr_t> is;
    for (const AfpInterp::Sample& s : after)
        is.insert(s.object);
    std::vector<const AfpInterp::Sample*> removed;
    for (const AfpInterp::Sample& s : before) {
        if (!is.contains(s.object)) removed.push_back(&s);
    }
    for (const AfpInterp::Sample& s : after) {
        const auto old = was.find(s.object);
        if (old != was.end()) {
            if (!(old->second->pose == s.pose)) g_clip.moved++;
            continue;
        }
        g_clip.kinds[steps == 1 ? Classify(s, was, removed) : kNotOneStep]++;
    }
}

HWND HiddenWindow() {
    const char* kClass = "r573_judge_survey";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "judge survey", WS_OVERLAPPED, 0, 0, kStageW, kStageH,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

}

TEST_CASE("How SDVX 1 clips replace objects between authored frames") {
    const std::optional<std::string> dir = Support::EnvVar("R573_SDVX1_DIR");
    if (!dir) SKIP("R573_SDVX1_DIR not set");
    Support::InstallCrashReporter();
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    g_d3d.width = kStageW;
    g_d3d.height = kStageH;
    REQUIRE(g_d3d.Init(hwnd));
    const JudgeEngine::BootResult boot =
        JudgeEngine::Boot(*dir, "sdvx1", g_d3d.device, kStageW, kStageH);
    REQUIRE(boot.error.empty());
    REQUIRE(boot.interp_problem.empty());
    AfpInterpHooks::SetAdvanceObserver(&Observe);

    const std::string graphics = (std::filesystem::path(*dir) / "data" / "graphics").string();
    Tally total;
    std::array<std::size_t, kKinds> clips_with{};
    std::size_t clips = 0;
    std::size_t still_with_handover = 0;
    std::vector<std::string> examples;
    std::vector<std::string> depth_examples;
    for (const std::string& file : Judge::ListPackageFiles(graphics)) {
        std::string err;
        const auto names = Judge::ReadClipNames(file, err);
        if (!names) continue;
        for (const std::string& clip : *names) {
            g_clip = {};
            if (!JudgeEngine::OpenClip(file, clip).empty()) continue;
            for (int i = 0; i < kAdvancesPerClip; i++)
                JudgeEngine::Advance(boot.fixed_step_seconds);
            clips++;
            for (std::size_t k = 0; k < kKinds; k++) {
                total.kinds[k] += g_clip.kinds[k];
                if (g_clip.kinds[k] > 0) clips_with[k]++;
            }
            total.moved += g_clip.moved;
            total.advances += g_clip.advances;
            if (g_clip.kinds[kSameCharacterOtherDepthContiguous] > 0 && depth_examples.size() < 12)
                depth_examples.push_back(
                    std::format("{}/{} ({})", Judge::PackageName(graphics, file), clip,
                                g_clip.kinds[kSameCharacterOtherDepthContiguous]));
            if (g_clip.kinds[kHandover] > 0 && g_clip.moved == 0) {
                still_with_handover++;
                if (examples.size() < 12)
                    examples.push_back(Judge::PackageName(graphics, file) + "/" + clip);
            }
        }
    }
    AfpInterpHooks::SetAdvanceObserver(nullptr);

    std::cerr << std::format("[replace] {} clips, {} advances, {} object pose changes\n", clips,
                             total.advances, total.moved);
    for (std::size_t k = 0; k < kKinds; k++)
        std::cerr << std::format("[replace] {}: {} objects in {} clips\n", kKindNames[k],
                                 total.kinds[k], clips_with[k]);
    std::cerr << std::format("[replace] clips whose only change is a handover: {}\n",
                             still_with_handover);
    for (const std::string& example : examples)
        std::cerr << std::format("[replace]   {}\n", example);
    for (const std::string& example : depth_examples)
        std::cerr << std::format("[replace]   other depth {}\n", example);
    for (const std::string& pair : g_pairs)
        std::cerr << "[replace]   pair " << pair << "\n";
    CHECK(clips > 0);
    JudgeEngine::Shutdown();
    g_d3d.Shutdown();
    DestroyWindow(hwnd);
}
