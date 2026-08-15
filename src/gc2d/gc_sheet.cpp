#include "gc2d/gc_sheet.h"

#include "app_globals.h"
#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "render_backend.h"
#include "support/crash_report.h"
#include "support/log.h"
#include "window.h"

#include <algorithm>
#include <climits>
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

void WriteDraws(std::ofstream& draws, const std::string& name, int frame) {
    draws << name << " frame " << frame << "\n";
    for (const Gc2dHost::DrawInfo& d : Gc2dHost::ListDrawNodes()) {
        draws << "    " << d.cell << "  " << BlendName(d.blend) << "  x=" << (int)d.x
              << " y=" << (int)d.y << " w=" << (int)d.w << " h=" << (int)d.h << " alpha=" << d.alpha
              << " flags=0x" << std::hex << d.flags << " code=0x" << d.blend_code << std::dec
              << " pair=" << d.alpha_a << "," << d.alpha_b << " keys=" << d.alpha_keys << "\n";
    }
}

void Shoot(const std::string& name, bool animated, int frame, const std::string& path) {
    std::vector<Gc2dHost::SpritePlacement> sprites;
    Gc2dHost::SpritePlacement placement;
    placement.name = name;
    placement.animated = animated;
    placement.timing.playback = GcAnim::Playback::HoldLast;
    sprites.push_back(std::move(placement));
    Gc2dHost::SetSprites(std::move(sprites));
    Gc2dHost::SetSpriteFrame(0, frame);

    AppWindow::PumpMessages();
    g_d3d.BeginFrame();
    Gc2dHost::DrawSprites(INT_MIN, INT_MAX);
    g_d3d.EndFrame();
    g_d3d.SaveBackBufferToFile(path.c_str());
}

}

int Run(const std::string& package_dir, const std::string& out_dir, int frame) {
    Support::InstallCrashReporter();
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);

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
    if (!Gc2dHost::Load(package_dir)) return 5;

    const std::vector<std::string> animations = Gc2dHost::ListAnimations();
    const std::vector<std::string> cells = Gc2dHost::ListCells();
    const std::filesystem::path root(out_dir);
    LOG("Gc2dSheet", "%zu animations + %zu cells -> %s", animations.size(), cells.size(),
        out_dir.c_str());

    std::ofstream parts((root / "parts.txt").string(), std::ios::binary | std::ios::trunc);
    std::ofstream draws((root / "draws.txt").string(), std::ios::binary | std::ios::trunc);
    int done = 0;
    const auto total = (int)(animations.size() + cells.size());
    const int samples = std::max(1, frame);
    for (const std::string& name : animations) {
        const int length = Gc2dHost::AnimationLength(name);
        for (int s = 0; s < samples; s++) {
            const int at = (length > 1) ? (((length - 1) * s) / std::max(1, samples - 1)) : 0;
            const std::string path =
                (root / ("anim_" + SafeName(name) + "_f" + std::to_string(at) + ".png")).string();
            Shoot(name, true, at, path);
            WriteDraws(draws, name, at);
            if (length <= 1) break;
        }
        parts << name << "\n";
        for (const std::string& part : Gc2dHost::ListParts(name))
            parts << "    " << part << "\n";
        LOG("Gc2dSheet", "%d/%d animation %s", ++done, total, name.c_str());
    }
    for (const std::string& name : cells) {
        const std::string path = (root / ("cell_" + SafeName(name) + ".png")).string();
        Shoot(name, false, 0, path);
        LOG("Gc2dSheet", "%d/%d cell %s", ++done, total, name.c_str());
    }

    Gc2dHost::Unload();
    LOG("Gc2dSheet", "done: %d image(s) at frame %d -> %s", total, frame, out_dir.c_str());
    return 0;
}

}
