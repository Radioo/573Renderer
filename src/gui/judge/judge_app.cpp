#include "gui/judge/judge_app.h"

#include "game_profile.h"
#include "gui/gui_style.h"
#include "gui/judge/judge_device.h"
#include "gui/judge/judge_scan.h"
#include "gui/judge/judge_view.h"
#include "judge/judge_clock.h"
#include "judge/verdict_store.h"
#include "judge_engine/judge_engine.h"
#include "support/crash_report.h"
#include "support/log.h"
#include "support/png_write.h"

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace JudgeApp {

namespace {

namespace fs = std::filesystem;

constexpr float kHzSmoothing = 0.05F;
constexpr float kLongestFrame = 0.25F;
constexpr int kRateReportFrames = 240;

struct Session {
    Options options;
    JudgeDevice::Window window;
    std::optional<Judge::VerdictStore> store;
    std::optional<Judge::Catalog> catalog;
    std::unique_ptr<JudgeScan::Scanner> scanner;
    JudgeView::Model model;
    Judge::Clock clock{1.0F / 60.0F};
    std::string note;
    std::optional<std::size_t> open_index;
    std::optional<std::size_t> want_open;
    bool load_shown = false;
    int pending_steps = 0;
    int captured = 0;
    int presented = 0;
    bool engine_ready = false;
    Judge::BackdropFill backdrop;
};

bool Capturing(const Session& s) {
    return !s.options.capture_dir.empty();
}

std::size_t StartIndex(const Session& s) {
    const Judge::Catalog& catalog = *s.catalog;
    if (!s.options.start_clip.empty()) {
        const std::size_t slash = s.options.start_clip.rfind('/');
        if (slash != std::string::npos) {
            const Judge::ClipKey key{.package = s.options.start_clip.substr(0, slash),
                                     .clip = s.options.start_clip.substr(slash + 1)};
            if (auto found = catalog.IndexOf(key)) return *found;
        }
        LOG("Judge", "--judge-clip %s is not in the catalog", s.options.start_clip.c_str());
    }
    if (s.store && catalog.Size() > 0) {
        if (auto next = catalog.NextUnjudged(catalog.Size() - 1, *s.store)) return *next;
    }
    return 0;
}

void Select(Session& s, std::size_t index) {
    if (!s.catalog || index >= s.catalog->Size()) return;
    s.model.current = index;
    const Judge::Entry* entry = s.store ? s.store->Find(s.catalog->Key(index)) : nullptr;
    s.note = entry != nullptr ? entry->note : std::string();
    s.want_open = index;
    s.load_shown = false;
}

void OpenPending(Session& s) {
    if (!s.want_open || !s.engine_ready) return;
    const std::size_t index = *s.want_open;
    const Judge::ClipKey& key = s.catalog->Key(index);
    if (!s.load_shown) {
        s.model.loading = key.package + " / " + key.clip;
        s.load_shown = true;
        return;
    }
    s.want_open.reset();
    s.model.loading.clear();
    s.model.clip_error = JudgeEngine::OpenClip(s.catalog->PathOf(index), key.clip);
    s.open_index = s.model.clip_error.empty() ? std::optional<std::size_t>(index) : std::nullopt;
    s.clock.Restart();
    s.pending_steps = 0;
}

void SaveEntry(Session& s, std::optional<Judge::Verdict> verdict) {
    if (!s.store || !s.catalog || s.model.current >= s.catalog->Size()) return;
    const Judge::ClipKey& key = s.catalog->Key(s.model.current);
    Judge::Entry entry;
    if (const Judge::Entry* old = s.store->Find(key)) entry = *old;
    entry.note = s.note;
    if (verdict) {
        entry.verdict = verdict;
        entry.matrix_lerp = s.model.lerp;
    } else if (!entry.verdict) {
        entry.matrix_lerp = s.model.lerp;
    }
    if (!s.store->Set(key, entry)) s.model.clip_error = s.store->LastError();
}

void Apply(Session& s, const JudgeView::Actions& a) {
    if (a.toggle_pause) s.clock.SetPaused(!s.clock.Paused());
    if (a.step) {
        s.clock.SetPaused(true);
        s.pending_steps += s.clock.StepRender();
    }
    if (a.restart && s.open_index) Select(s, *s.open_index);
    if (a.toggle_loop) s.model.loop = !s.model.loop;
    if (a.toggle_next_after_verdict) s.model.next_after_verdict = !s.model.next_after_verdict;
    if (a.scale_index) s.clock.SetScaleIndex(*a.scale_index);
    if (a.backdrop) {
        s.model.backdrop = *a.backdrop;
        s.backdrop = Judge::FillFor(*a.backdrop, s.window.stage_w, s.window.stage_h);
    }
    if (a.lerp) {
        s.model.lerp = *a.lerp;
        JudgeEngine::SetMatrixLerp(*a.lerp);
    }
    if (a.note_edited) SaveEntry(s, std::nullopt);
    if (a.verdict) {
        SaveEntry(s, a.verdict);
        if (s.model.next_after_verdict && s.store) {
            if (auto next = s.catalog->NextUnjudged(s.model.current, *s.store)) Select(s, *next);
        }
    }
    if (a.select && *a.select != s.model.current) Select(s, *a.select);
}

void AdvanceLogic(Session& s, float real_seconds) {
    if (!s.open_index) return;
    int steps = s.clock.Tick(real_seconds) + s.pending_steps;
    s.pending_steps = 0;
    for (; steps > 0; steps--) {
        const JudgeEngine::Playhead head = JudgeEngine::ReadPlayhead();
        if (!s.model.loop && head.total > 0 && head.frame >= head.total - 1) {
            s.clock.SetPaused(true);
            break;
        }
        JudgeEngine::Advance(s.clock.Step());
    }
    JudgeEngine::SetRenderTime(s.clock.SinceAdvance());
}

void WriteCapture(Session& s) {
    char stem[32];
    std::snprintf(stem, sizeof(stem), "%04d", s.captured);
    const fs::path dir(s.options.capture_dir);
    std::vector<std::uint8_t> bgra;
    int w = 0;
    int h = 0;
    const char* names[] = {"_authored.png", "_interpolated.png"};
    for (std::size_t i = 0; i < s.window.stages.size(); i++) {
        if (JudgeDevice::ReadSurface(s.window, s.window.stages[i].surface, bgra, w, h))
            Support::WritePngBGRA((dir / (std::string(stem) + names[i])).string(), bgra.data(), w,
                                  h);
    }
    const bool first_or_last = s.captured == 0 || s.captured + 1 >= s.options.capture_frames;
    IDirect3DSurface9* back = JudgeDevice::BackBuffer(s.window);
    if (first_or_last && JudgeDevice::ReadSurface(s.window, back, bgra, w, h))
        Support::WritePngBGRA((dir / (std::string(stem) + "_window.png")).string(), bgra.data(), w,
                              h);
    if (back != nullptr) back->Release();
    s.captured++;
}

void Frame(Session& s, float real_seconds) {
    JudgeDevice::ApplyResize(s.window);
    if (!s.catalog && s.scanner) {
        s.model.scan = s.scanner->Status();
        if (auto ready = s.scanner->TakeCatalog()) {
            s.catalog = std::move(ready);
            s.model.catalog = &*s.catalog;
            if (s.catalog->Size() > 0) Select(s, StartIndex(s));
        }
    }
    OpenPending(s);
    AdvanceLogic(s, real_seconds);
    const JudgeEngine::Playhead head = JudgeEngine::ReadPlayhead();
    s.model.frame = head.frame;
    s.model.total = head.total;
    s.model.paused = s.clock.Paused();
    s.model.scale_index = s.clock.ScaleIndex();
    const AfpInterpHooks::LayerStats stats = JudgeEngine::InterpolationStats();
    s.model.moving = stats.moving;
    s.model.blended = stats.swapped;
    s.model.alpha = stats.alpha;
    if (real_seconds > 0.0F) {
        const float hz = 1.0F / real_seconds;
        s.model.display_hz = s.model.display_hz > 0.0F
                                 ? s.model.display_hz + ((hz - s.model.display_hz) * kHzSmoothing)
                                 : hz;
    }

    IDirect3DDevice9Ex* device = s.window.device;
    device->BeginScene();
    if (s.open_index) {
        JudgeDevice::BeginStage(s.window, 0, s.backdrop);
        JudgeEngine::DrawSide(JudgeEngine::Side::Authored);
        JudgeDevice::BeginStage(s.window, 1, s.backdrop);
        JudgeEngine::DrawSide(JudgeEngine::Side::Interpolated);
    }
    JudgeDevice::BeginWindow(s.window);
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const JudgeView::Actions actions = JudgeView::Build(s.model, s.note);
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    device->EndScene();
    if (Capturing(s) && s.open_index) WriteCapture(s);
    JudgeDevice::Present(s.window);
    if (++s.presented == kRateReportFrames)
        LOG("Judge", "presenting at %.1f Hz (vsync %s)", s.model.display_hz,
            Capturing(s) ? "off" : "on");
    Apply(s, actions);
}

void InitUi(Session& s) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    Gui::LoadFonts();
    Gui::ApplyStyle();
    Gui::ApplyAccentForProfile("sdvx1");
    ImGui_ImplWin32_Init(s.window.hwnd);
    ImGui_ImplDX9_Init(s.window.device);
    s.model.authored =
        static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(s.window.stages[0].texture));
    s.model.interpolated =
        static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(s.window.stages[1].texture));
    s.model.stage_w = static_cast<float>(s.window.stage_w);
    s.model.stage_h = static_cast<float>(s.window.stage_h);
    s.backdrop = Judge::FillFor(s.model.backdrop, s.window.stage_w, s.window.stage_h);
}

void ShutdownUi() {
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void BootEngine(Session& s, const std::string& slug) {
    const JudgeEngine::BootResult boot = JudgeEngine::Boot(
        s.options.game_dir, slug, s.window.device, s.window.stage_w, s.window.stage_h);
    s.model.boot_error = boot.error;
    s.model.interp_problem = boot.interp_problem.empty()
                                 ? std::string()
                                 : "Interpolation is off: " + boot.interp_problem + ".";
    if (!boot.error.empty()) return;
    s.clock = Judge::Clock(boot.fixed_step_seconds);
    s.engine_ready = true;
    s.scanner = std::make_unique<JudgeScan::Scanner>(
        (fs::path(s.options.game_dir) / "data" / "graphics").string());
}

float FrameSeconds(const Session& s, LARGE_INTEGER& last, const LARGE_INTEGER& frequency) {
    if (s.options.display_hz > 0) return 1.0F / static_cast<float>(s.options.display_hz);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const auto seconds = static_cast<float>(static_cast<double>(now.QuadPart - last.QuadPart) /
                                            static_cast<double>(frequency.QuadPart));
    last = now;
    return std::min(seconds, kLongestFrame);
}

}

std::string DefaultVerdictsPath(const std::string& profile_slug) {
    std::wstring exe(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
    exe.resize(length);
    return (fs::path(exe).parent_path() / ("judge_verdicts_" + profile_slug + ".json")).string();
}

int Run(const Options& options) {
    Support::InstallCrashReporter();
    Session s;
    s.options = options;
    const GameProfile::Profile* profile = options.profile.empty()
                                              ? GameProfile::AutoDetect(options.game_dir)
                                              : GameProfile::BySlug(options.profile);
    if (profile == nullptr) {
        LOG("Judge", "no game profile for '%s'; pass --profile", options.game_dir.c_str());
        return 2;
    }
    if (s.options.verdicts_path.empty())
        s.options.verdicts_path = DefaultVerdictsPath(profile->slug);
    if (!s.options.background.empty()) {
        const auto backdrop = Judge::BackdropByName(s.options.background);
        if (!backdrop) {
            LOG("Judge", "unknown --judge-background '%s'", s.options.background.c_str());
            return 2;
        }
        s.model.backdrop = *backdrop;
    }
    std::string err;
    s.store = Judge::VerdictStore::Open(s.options.verdicts_path, err);
    if (!s.store) s.model.boot_error = "Verdicts are not saved: " + err;
    s.model.store = s.store ? &*s.store : nullptr;
    if (Capturing(s)) {
        std::error_code ec;
        fs::create_directories(s.options.capture_dir, ec);
    }

    if (!JudgeDevice::Create(s.window, GetModuleHandleW(nullptr), profile->default_render_w,
                             profile->default_render_h, !Capturing(s))) {
        LOG("Judge", "the judge window could not be created");
        JudgeDevice::Destroy(s.window);
        return 3;
    }
    InitUi(s);
    s.model.loading = "the game engine";
    Frame(s, 0.0F);
    s.model.loading.clear();
    std::string const boot_problem = s.model.boot_error;
    BootEngine(s, profile->slug);
    if (s.model.boot_error.empty()) s.model.boot_error = boot_problem;
    LOG("Judge", "verdicts: %s", s.options.verdicts_path.c_str());

    LARGE_INTEGER frequency{};
    LARGE_INTEGER last{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last);
    while (JudgeDevice::Pump(s.window)) {
        Frame(s, FrameSeconds(s, last, frequency));
        if (Capturing(s) && (s.captured >= s.options.capture_frames || !s.engine_ready)) break;
    }
    s.scanner.reset();
    JudgeEngine::Shutdown();
    ShutdownUi();
    JudgeDevice::Destroy(s.window);
    return 0;
}

}
