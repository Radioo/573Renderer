#include <catch2/catch_test_macros.hpp>

#include "afp_ddr.h"
#include "afp_ddr_funcs.h"
#include "app_globals.h"
#include "judge/interp_tracks.h"
#include "judge_engine/judge_engine.h"
#include "judge_hooks/afp_interp_hooks.h"
#include "render_backend.h"
#include "support/crash_report.h"
#include "support/env.h"

#include <windows.h>

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
constexpr std::uint32_t kDeepGotoAndPlay = 0xF08;
constexpr std::uint32_t kCurrentFrame = 0x1010;
constexpr int kLoopStart = 74;
constexpr int kLoopEnd = 264;

struct Observed {
    std::vector<AfpInterp::Sample> before;
    std::vector<AfpInterp::Sample> after;
};

Observed& Seen() {
    static Observed seen;
    return seen;
}

void Observe(std::span<const AfpInterp::Sample> before, std::span<const AfpInterp::Sample> after,
             int steps) {
    (void)steps;
    Seen().before.assign(before.begin(), before.end());
    Seen().after.assign(after.begin(), after.end());
}

struct Change {
    std::size_t kept = 0;
    std::size_t kept_moved = 0;
    std::size_t removed = 0;
    std::size_t created = 0;
    std::size_t created_parent_new = 0;
    std::size_t same_slot = 0;
    std::size_t same_slot_contiguous = 0;
    std::size_t same_slot_moved = 0;
    std::size_t same_slot_parent_pair = 0;
};

std::string Describe(const AfpInterp::Sample& s) {
    return std::format("obj {:x} parent {:x} depth {} char {:x} span {}..{} x {:.2f} y {:.2f} "
                       "a {:.3f}",
                       s.object, s.parent, s.pose.depth, s.pose.character, s.created,
                       s.lifetime_end, s.pose.matrix[4], s.pose.matrix[5], s.pose.matrix[0]);
}

std::vector<const AfpInterp::Sample*> Removed(const std::vector<AfpInterp::Sample>& from,
                                              const std::vector<AfpInterp::Sample>& to, bool dump) {
    std::unordered_set<std::uintptr_t> is;
    for (const auto& s : to)
        is.insert(s.object);
    std::vector<const AfpInterp::Sample*> removed;
    for (const auto& s : from) {
        if (is.contains(s.object)) continue;
        removed.push_back(&s);
        if (dump) std::cerr << "[title]     gone " << Describe(s) << "\n";
    }
    return removed;
}

bool SameSlot(const AfpInterp::Sample& old, const AfpInterp::Sample& created,
              std::uintptr_t parent) {
    return old.parent == parent && old.pose.depth == created.pose.depth &&
           old.pose.character == created.pose.character;
}

void MatchSlot(const AfpInterp::Sample& s, const std::vector<const AfpInterp::Sample*>& removed,
               std::unordered_map<std::uintptr_t, std::uintptr_t>& old_of_new, Change& c) {
    const auto paired_parent = old_of_new.find(s.parent);
    const bool through_pair = paired_parent != old_of_new.end();
    const std::uintptr_t look_parent = through_pair ? paired_parent->second : s.parent;
    for (const AfpInterp::Sample* r : removed) {
        if (!SameSlot(*r, s, look_parent)) continue;
        c.same_slot++;
        if (through_pair) c.same_slot_parent_pair++;
        if (r->lifetime_end == s.created) c.same_slot_contiguous++;
        if (!(r->pose == s.pose)) c.same_slot_moved++;
        old_of_new.emplace(s.object, r->object);
        return;
    }
}

Change Compare(const std::vector<AfpInterp::Sample>& from, const std::vector<AfpInterp::Sample>& to,
               bool dump) {
    Change c;
    std::unordered_map<std::uintptr_t, const AfpInterp::Sample*> was;
    for (const auto& s : from)
        was.emplace(s.object, &s);
    const std::vector<const AfpInterp::Sample*> removed = Removed(from, to, dump);
    c.removed = removed.size();
    std::unordered_map<std::uintptr_t, std::uintptr_t> old_of_new;
    for (const auto& s : to) {
        const auto old = was.find(s.object);
        if (old != was.end()) {
            c.kept++;
            if (!(old->second->pose == s.pose)) c.kept_moved++;
            continue;
        }
        c.created++;
        if (dump) std::cerr << "[title]     new  " << Describe(s) << "\n";
        if (!was.contains(s.parent)) c.created_parent_new++;
        MatchSlot(s, removed, old_of_new, c);
    }
    return c;
}

void Print(const char* what, const Change& c) {
    std::cerr << std::format(
        "[title] {}: kept {} (moved {}), removed {}, created {} (parent created too {}), "
        "same slot {} (through a paired parent {}, span contiguous {}, pose differs {})\n",
        what, c.kept, c.kept_moved, c.removed, c.created, c.created_parent_new, c.same_slot,
        c.same_slot_parent_pair, c.same_slot_contiguous, c.same_slot_moved);
}

HWND HiddenWindow() {
    const char* kClass = "r573_judge_title_study";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "judge title", WS_OVERLAPPED, 0, 0, kStageW, kStageH, nullptr,
                           nullptr, GetModuleHandleA(nullptr), nullptr);
}

}

TEST_CASE("What the title screen's frame by frame deep goto does to its objects") {
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
    const std::string package = (std::filesystem::path(*dir) / "data/graphics/title.bin").string();
    REQUIRE(JudgeEngine::OpenClip(package, "title").empty());

    const AfpDdrFuncs& afp = DdrAfp::Funcs();
    const std::uint32_t layer = afp.LayerCreate(DdrAfp::ClipStreamId("title"), "title");
    REQUIRE(afp.LayerValid(layer));
    afp.afp_layer_set_attribute(layer, 1, 1);
    afp.afp_layer_play(layer, 1.0F);
    AfpInterpHooks::SetLayerInterpolated(layer, true);
    AfpInterpHooks::SetAdvanceObserver(&Observe);

    std::cerr << "[title] plain playback\n";
    for (int i = 0; i < 300; i++) {
        JudgeEngine::Advance(boot.fixed_step_seconds);
        if (i >= kLoopStart + 30 && i < kLoopStart + 33)
            Print(std::format("play {}", i).c_str(), Compare(Seen().before, Seen().after, false));
    }

    afp.afp_layer_play(layer, 0.0F);
    const auto root = static_cast<std::uint32_t>(afp.afp_layer_mc_refer(layer, ""));
    std::cerr << "[title] paused, deep goto by one frame\n";
    std::vector<AfpInterp::Sample> previous;
    for (int frame = kLoopStart; frame < kLoopEnd; frame++) {
        afp.afp_mc_op_frame(root, kDeepGotoAndPlay, frame);
        JudgeEngine::Advance(boot.fixed_step_seconds);
        int current = -1;
        afp.afp_mc_get_param(root, kCurrentFrame, &current);
        if (!previous.empty() && frame >= kLoopStart + 30 && frame < kLoopStart + 33) {
            const bool dump = frame == kLoopStart + 30;
            Print(std::format("goto {} (afp reports {})", frame, current).c_str(),
                  Compare(previous, Seen().before, dump));
            AfpInterp::LayerTracks as_step;
            as_step.BeginAdvance(previous);
            as_step.EndAdvance(1, Seen().before);
            AfpInterp::LayerTracks as_jump;
            as_jump.BeginAdvance(previous);
            for (const AfpInterp::Sample& s : previous)
                as_jump.MarkJumped(s.parent);
            as_jump.EndAdvance(1, Seen().before);
            std::cerr << std::format("[title]   blended objects: goto as a step {}, every clip "
                                     "marked as jumped {}\n",
                                     as_step.MovingCount(), as_jump.MovingCount());
        }
        previous = Seen().after;
    }
    AfpInterpHooks::SetAdvanceObserver(nullptr);
    AfpInterpHooks::SetLayerInterpolated(layer, false);
    DdrAfp::DestroyLayer(layer);
    JudgeEngine::Shutdown();
    g_d3d.Shutdown();
    DestroyWindow(hwnd);
}
