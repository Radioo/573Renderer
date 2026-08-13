#include "scene3d/scene3d_test.h"

#include "app_globals.h"
#include "render_backend.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_render.h"
#include "support/crash_report.h"
#include "support/log.h"
#include "window.h"

#include <string>

namespace Scene3dTest {

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr float kTicksPerFrame = 160.0F;

}

int Run(const std::string& scene_dir, const std::string& out_png, int frames) {
    Support::InstallCrashReporter();
    LOG("Scene3d-T", "scene=%s out=%s frames=%d", scene_dir.c_str(), out_png.c_str(), frames);

    Scene3d::Scene scene;
    std::string err;
    if (!Scene3d::Load(scene_dir, scene, err)) {
        LOG("Scene3d-T", "load failed: %s", err.c_str());
        return 2;
    }

    HWND hwnd = AppWindow::Create(kWidth, kHeight);
    if (hwnd == nullptr) {
        LOG("Scene3d-T", "could not create the render window");
        return 3;
    }
    g_d3d.width = kWidth;
    g_d3d.height = kHeight;
    if (!g_d3d.Init(hwnd)) {
        LOG("Scene3d-T", "D3D9 init failed");
        return 4;
    }

    Scene3d::Renderer renderer;
    if (!renderer.Init(g_d3d.device, scene)) {
        LOG("Scene3d-T", "renderer init failed");
        return 5;
    }

    frames = (frames > 0) ? frames : 1;
    const float span = (scene.max_time > 0.0F) ? scene.max_time : kTicksPerFrame;
    for (int i = 0; i < frames; i++) {
        AppWindow::PumpMessages();
        const float time = (frames > 1) ? span * ((float)i / (float)(frames - 1)) : 0.0F;
        for (auto& model : scene.models)
            model.time = time;
        g_d3d.BeginFrame();
        renderer.Draw(scene, time, kWidth, kHeight, nullptr);
        g_d3d.EndFrame();
        if (i == frames - 1) {
            LOG("Scene3d-T", "frame %d: t=%.1f ticks, %d draw calls", i, time,
                renderer.DrawCalls());
            g_d3d.SaveBackBufferToFile(out_png.c_str());
        }
    }

    renderer.Release();
    LOG("Scene3d-T", "done -> %s", out_png.c_str());
    return 0;
}

}
