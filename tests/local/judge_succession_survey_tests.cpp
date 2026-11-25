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
constexpr std::size_t kMostPairs = 40;
constexpr std::size_t kMostExamples = 12;

enum class Kind : std::uint8_t {
    SameSlotContiguous,
    SameSlotGap,
    SameCharacterOtherDepth,
    SameCharacterOtherDepthContiguous,
    Handover,
    ChildOfNew,
    NoRemovedSibling,
    NotOneStep,
    Count
};

constexpr std::size_t kKinds = static_cast<std::size_t>(Kind::Count);

constexpr std::size_t Index(Kind kind) {
    return static_cast<std::size_t>(kind);
}

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

struct Survey {
    Tally clip;
    std::vector<std::string> pairs;
};

Survey& Current() {
    static Survey survey;
    return survey;
}

struct Siblings {
    bool same_slot_contiguous = false;
    bool same_slot_gap = false;
    bool other_depth = false;
    bool other_depth_contiguous = false;
    bool handover = false;
};

float Alpha(const AfpInterp::Sample& s) {
    return s.pose.has_color ? s.pose.color[3] : 1.0F;
}

void RecordPair(const AfpInterp::Sample& old, const AfpInterp::Sample& created) {
    std::vector<std::string>& pairs = Current().pairs;
    if (pairs.size() >= kMostPairs) return;
    pairs.push_back(
        std::format("depth {} -> {} frames {}..{} -> {}..{} x {:.1f} -> {:.1f} y {:.1f} "
                    "-> {:.1f} a {:.3f} -> {:.3f} colour {} -> {}",
                    old.pose.depth, created.pose.depth, old.created, old.lifetime_end,
                    created.created, created.lifetime_end, old.pose.matrix[4],
                    created.pose.matrix[4], old.pose.matrix[5], created.pose.matrix[5],
                    old.pose.matrix[0], created.pose.matrix[0], Alpha(old), Alpha(created)));
}

Siblings Look(const AfpInterp::Sample& created,
              const std::vector<const AfpInterp::Sample*>& removed) {
    Siblings seen;
    for (const AfpInterp::Sample* old : removed) {
        if (old->parent != created.parent) continue;
        const bool same_character = old->pose.character == created.pose.character;
        const bool contiguous = old->lifetime_end == created.created;
        if (same_character && old->pose.depth == created.pose.depth) {
            if (contiguous) {
                seen.same_slot_contiguous = true;
                return seen;
            }
            seen.same_slot_gap = true;
        } else if (same_character) {
            seen.other_depth = true;
            if (contiguous) {
                seen.other_depth_contiguous = true;
                RecordPair(*old, created);
            }
        } else if (contiguous) {
            seen.handover = true;
        }
    }
    return seen;
}

Kind Classify(const AfpInterp::Sample& created,
              const std::unordered_map<std::uintptr_t, const AfpInterp::Sample*>& before,
              const std::vector<const AfpInterp::Sample*>& removed) {
    if (!before.contains(created.parent) && created.parent != 0) return Kind::ChildOfNew;
    const Siblings seen = Look(created, removed);
    if (seen.same_slot_contiguous) return Kind::SameSlotContiguous;
    if (seen.same_slot_gap) return Kind::SameSlotGap;
    if (seen.other_depth_contiguous) return Kind::SameCharacterOtherDepthContiguous;
    if (seen.other_depth) return Kind::SameCharacterOtherDepth;
    if (seen.handover) return Kind::Handover;
    return Kind::NoRemovedSibling;
}

void Observe(std::span<const AfpInterp::Sample> before, std::span<const AfpInterp::Sample> after,
             int steps) {
    Tally& clip = Current().clip;
    clip.advances++;
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
            if (!(old->second->pose == s.pose)) clip.moved++;
            continue;
        }
        const Kind kind = steps == 1 ? Classify(s, was, removed) : Kind::NotOneStep;
        clip.kinds.at(Index(kind))++;
    }
}

struct Totals {
    Tally all;
    std::array<std::size_t, kKinds> clips_with{};
    std::size_t clips = 0;
    std::size_t still_with_handover = 0;
    std::vector<std::string> examples;
    std::vector<std::string> depth_examples;
};

void Collect(const Tally& clip, const std::string& name, Totals& totals) {
    totals.clips++;
    for (std::size_t k = 0; k < kKinds; k++) {
        totals.all.kinds.at(k) += clip.kinds.at(k);
        if (clip.kinds.at(k) > 0) totals.clips_with.at(k)++;
    }
    totals.all.moved += clip.moved;
    totals.all.advances += clip.advances;
    const std::size_t other_depth = clip.kinds.at(Index(Kind::SameCharacterOtherDepthContiguous));
    if (other_depth > 0 && totals.depth_examples.size() < kMostExamples)
        totals.depth_examples.push_back(std::format("{} ({})", name, other_depth));
    if (clip.kinds.at(Index(Kind::Handover)) > 0 && clip.moved == 0) {
        totals.still_with_handover++;
        if (totals.examples.size() < kMostExamples) totals.examples.push_back(name);
    }
}

void Report(const Totals& totals) {
    std::cerr << std::format("[replace] {} clips, {} advances, {} object pose changes\n",
                             totals.clips, totals.all.advances, totals.all.moved);
    for (std::size_t k = 0; k < kKinds; k++) {
        std::cerr << std::format("[replace] {}: {} objects in {} clips\n", kKindNames.at(k),
                                 totals.all.kinds.at(k), totals.clips_with.at(k));
    }
    std::cerr << std::format("[replace] clips whose only change is a handover: {}\n",
                             totals.still_with_handover);
    for (const std::string& example : totals.examples)
        std::cerr << std::format("[replace]   {}\n", example);
    for (const std::string& example : totals.depth_examples)
        std::cerr << std::format("[replace]   other depth {}\n", example);
    for (const std::string& pair : Current().pairs)
        std::cerr << "[replace]   pair " << pair << "\n";
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
    Totals totals;
    for (const std::string& file : Judge::ListPackageFiles(graphics)) {
        std::string err;
        const auto names = Judge::ReadClipNames(file, err);
        if (!names) continue;
        for (const std::string& clip : *names) {
            Current().clip = {};
            if (!JudgeEngine::OpenClip(file, clip).empty()) continue;
            for (int i = 0; i < kAdvancesPerClip; i++)
                JudgeEngine::Advance(boot.fixed_step_seconds);
            Collect(Current().clip, Judge::PackageName(graphics, file) + "/" + clip, totals);
        }
    }
    AfpInterpHooks::SetAdvanceObserver(nullptr);

    Report(totals);
    CHECK(totals.clips > 0);
    JudgeEngine::Shutdown();
    g_d3d.Shutdown();
    DestroyWindow(hwnd);
}
