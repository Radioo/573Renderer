#include "gc2d/gc_sheet.h"

#include "app_globals.h"
#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "gc2d/gc_playfield.h"
#include "iidx_playfield.h"
#include "render_backend.h"
#include "support/crash_report.h"
#include "support/log.h"
#include "support/png_write.h"
#include "window.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace Gc2dSheet {

namespace {

constexpr int kRenderW = 640;
constexpr int kRenderH = 480;

std::string SafeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        const bool keep = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                          (c >= 'a' && c <= 'z') || c == '_' || c == '-';
        out += keep ? c : '_';
    }
    return out;
}

const char* BlendName(int blend) {
    switch (blend) {
    case 1:
        return "additive";
    case 2:
        return "subtract";
    case 3:
        return "replace";
    default:
        return "normal";
    }
}

void WriteElements(std::ofstream& elements, const std::string& name, int frame) {
    for (const GcAnim::ElementNode& e : Gc2dHost::ListElements()) {
        elements << name << " frame " << frame << "  element " << e.id << " x=" << (int)e.x
                 << " y=" << (int)e.y << '\n';
    }
}

void WriteDraws(std::ofstream& draws, const std::string& name, int frame) {
    draws << name << " frame " << frame << "\n";
    for (const Gc2dHost::DrawInfo& d : Gc2dHost::ListDrawNodes()) {
        draws << "    " << d.cell << "  " << BlendName(d.blend) << "  x=" << (int)d.x
              << " y=" << (int)d.y << " w=" << (int)d.w << " h=" << (int)d.h << " alpha=" << d.alpha
              << " flags=0x" << std::hex << d.flags << " code=0x" << d.blend_code << std::dec
              << " pair=" << d.alpha_a << "," << d.alpha_b << " keys=" << d.alpha_keys << "\n";
    }
}

void Place(const std::string& name, bool animated, int frame, std::array<float, 2> at,
           bool with_values) {
    std::vector<Gc2dHost::SpritePlacement> sprites;
    Gc2dHost::SpritePlacement placement;
    placement.name = name;
    placement.animated = animated;
    placement.x = at[0];
    placement.y = at[1];
    placement.timing.playback = GcAnim::Playback::HoldLast;
    sprites.push_back(std::move(placement));
    if (with_values) Gc2dPlayfield::Append(sprites);
    Gc2dHost::SetSprites(std::move(sprites));
    Gc2dHost::SetSpriteFrame(0, frame);
}

void RenderOnce() {
    AppWindow::PumpMessages();
    g_d3d.BeginFrame();
    Gc2dHost::DrawSprites(INT_MIN, INT_MAX);
    g_d3d.EndFrame();
}

void PlaceScene(const std::string& name, bool animated, int frame, std::array<float, 2> at) {
    if (Gc2dPlayfield::Enabled()) {
        Place(name, animated, frame, at, false);
        RenderOnce();
    }
    Place(name, animated, frame, at, Gc2dPlayfield::Enabled());
}

bool DrawOver(uint32_t clear, std::vector<uint8_t>& out, int& w, int& h) {
    g_d3d.clear_color = clear;
    RenderOnce();
    return g_d3d.ReadOffscreenBGRA(out, w, h);
}

void Unmix(const std::vector<uint8_t>& over_black, const std::vector<uint8_t>& over_white,
           std::vector<uint8_t>& out) {
    out.assign(over_black.size(), 0);
    for (std::size_t i = 0; i + 3 < over_black.size(); i += 4) {
        int coverage = 0;
        for (std::size_t ch = 0; ch < 3; ch++) {
            const int black = over_black[i + ch];
            const int white = over_white[i + ch];
            coverage = std::max(coverage, 255 - std::clamp(white - black, 0, 255));
        }
        for (std::size_t ch = 0; ch < 3; ch++) {
            const int straight = (over_black[i + ch] * 255) / std::max(coverage, 1);
            out[i + ch] = static_cast<uint8_t>(std::clamp(straight, 0, 255));
        }
        out[i + 3] = static_cast<uint8_t>(coverage);
    }
}

void Shoot(const std::string& name, bool animated, int frame, const std::string& path,
           std::array<float, 2> at, bool straight_alpha) {
    PlaceScene(name, animated, frame, at);
    if (!straight_alpha) {
        RenderOnce();
        g_d3d.SaveBackBufferToFile(path.c_str());
        return;
    }

    std::vector<uint8_t> over_black;
    std::vector<uint8_t> over_white;
    int w = 0;
    int h = 0;
    int white_w = 0;
    int white_h = 0;
    if (!DrawOver(0xFF000000, over_black, w, h)) return;
    PlaceScene(name, animated, frame, at);
    if (!DrawOver(0xFFFFFFFF, over_white, white_w, white_h)) return;
    if (w != white_w || h != white_h) return;

    std::vector<uint8_t> straight;
    Unmix(over_black, over_white, straight);
    Support::WritePngBGRA(path, straight.data(), w, h);
}

}

namespace {

constexpr const char* kPartsAsset = "parts";

int OpenWindow() {
    HWND hwnd = AppWindow::Create(kRenderW, kRenderH);
    if (hwnd == nullptr) {
        LOG("Gc2dSheet", "could not create the render window");
        return 3;
    }
    g_d3d.width = kRenderW;
    g_d3d.height = kRenderH;
    g_d3d.present_width = kRenderW;
    g_d3d.present_height = kRenderH;
    if (!g_d3d.Init(hwnd)) {
        LOG("Gc2dSheet", "D3D9 init failed");
        return 4;
    }
    return 0;
}

int OpenPackages(const Job& job) {
    if (!Gc2dHost::Load(job.package_dir)) return 5;
    if (job.parts_dir.empty() || job.playfield.empty()) return 0;
    if (!Gc2dHost::LoadAsset(kPartsAsset, job.parts_dir)) {
        LOG("Gc2dSheet", "could not load the parts package '%s'", job.parts_dir.c_str());
        return 6;
    }
    IidxPlayfield::Values values;
    std::string err;
    if (!IidxPlayfield::Parse(job.playfield, values, err)) {
        LOG("Gc2dSheet", "playfield spec rejected: %s", err.c_str());
        return 7;
    }
    Gc2dPlayfield::Enable(kPartsAsset, values);
    return 0;
}

}

int Run(const Job& job) {
    const std::string& out_dir = job.out_dir;
    const int frame = job.samples;
    const std::array<float, 2> at = job.at;
    const bool straight_alpha = job.straight_alpha;
    Support::InstallCrashReporter();
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);

    const int opened = OpenWindow();
    if (opened != 0) return opened;
    const int loaded = OpenPackages(job);
    if (loaded != 0) return loaded;

    const std::vector<std::string> animations = Gc2dHost::ListAnimations();
    const std::vector<std::string> cells = Gc2dHost::ListCells();
    const std::filesystem::path root(out_dir);
    LOG("Gc2dSheet", "%zu animations + %zu cells -> %s", animations.size(), cells.size(),
        out_dir.c_str());

    std::ofstream parts((root / "parts.txt").string(), std::ios::binary | std::ios::trunc);
    std::ofstream draws((root / "draws.txt").string(), std::ios::binary | std::ios::trunc);
    std::ofstream elements((root / "elements.txt").string(), std::ios::binary | std::ios::trunc);
    int done = 0;
    const auto total = (int)(animations.size() + cells.size());
    const int samples = std::max(1, frame);
    for (const std::string& name : animations) {
        const int length = Gc2dHost::AnimationLength(name);
        for (int s = 0; s < samples; s++) {
            const int frame_at = (length > 1) ? (((length - 1) * s) / std::max(1, samples - 1)) : 0;
            const std::string path =
                (root / ("anim_" + SafeName(name) + "_f" + std::to_string(frame_at) + ".png"))
                    .string();
            Shoot(name, true, frame_at, path, at, straight_alpha);
            WriteDraws(draws, name, frame_at);
            WriteElements(elements, name, frame_at);
            if (length <= 1) break;
        }
        parts << name << "\n";
        for (const std::string& part : Gc2dHost::ListParts(name))
            parts << "    " << part << "\n";
        LOG("Gc2dSheet", "%d/%d animation %s", ++done, total, name.c_str());
    }
    for (const std::string& name : cells) {
        const std::string path = (root / ("cell_" + SafeName(name) + ".png")).string();
        Shoot(name, false, 0, path, at, straight_alpha);
        LOG("Gc2dSheet", "%d/%d cell %s", ++done, total, name.c_str());
    }

    Gc2dHost::Unload();
    LOG("Gc2dSheet", "done: %d image(s) at frame %d -> %s", total, frame, out_dir.c_str());
    return 0;
}

}
