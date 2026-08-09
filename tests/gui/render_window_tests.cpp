#include "scene3d/camera.h"
#include "scene3d/scene3d_input.h"
#include "state/app_state.h"
#include "state/live_controls.h"
#include "window.h"

#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <array>
#include <cmath>

namespace {

class LiveRenderWindow {
public:
    LiveRenderWindow() : hwnd_(AppWindow::Create(640, 480)) {
        AppWindow::SetRenderRtSize(640, 480);
    }
    ~LiveRenderWindow() {
        if (hwnd_ != nullptr) DestroyWindow(hwnd_);
        Drain();
    }
    LiveRenderWindow(const LiveRenderWindow&) = delete;
    LiveRenderWindow& operator=(const LiveRenderWindow&) = delete;
    LiveRenderWindow(LiveRenderWindow&&) = delete;
    LiveRenderWindow& operator=(LiveRenderWindow&&) = delete;

    [[nodiscard]] HWND get() const { return hwnd_; }

    static void Drain() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
        }
    }

private:
    HWND hwnd_ = nullptr;
};

LPARAM Point(int x, int y) {
    return MAKELPARAM(x, y);
}

}

TEST_CASE("render window crop pick drag publishes a normalised rect", "[render][window]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    auto& state = App::Global();
    state.SetCropRect({});
    state.SetCropPickMode(true);

    SendMessageW(win.get(), WM_LBUTTONDOWN, 0, Point(400, 300));
    SendMessageW(win.get(), WM_MOUSEMOVE, 0, Point(100, 80));
    SendMessageW(win.get(), WM_LBUTTONUP, 0, Point(100, 80));

    App::CropRect const rect = state.GetCropRect();
    CHECK(rect.w > 0);
    CHECK(rect.h > 0);
    CHECK(rect.x >= 0);
    CHECK(rect.y >= 0);
    CHECK_FALSE(state.GetCropPickMode());

    state.SetCropRect({});
}

TEST_CASE("render window crop pick discards a zero-area rect", "[render][window]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    auto& state = App::Global();
    state.SetCropRect({});
    state.SetCropPickMode(true);

    SendMessageW(win.get(), WM_LBUTTONDOWN, 0, Point(200, 150));
    SendMessageW(win.get(), WM_LBUTTONUP, 0, Point(200, 150));

    App::CropRect const rect = state.GetCropRect();
    CHECK(rect.w == 0);
    CHECK(rect.h == 0);
    CHECK_FALSE(state.GetCropPickMode());
}

TEST_CASE("render window Escape cancels crop pick without closing the app", "[render][window]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    auto& state = App::Global();
    state.SetCropPickMode(true);

    SendMessageW(win.get(), WM_LBUTTONDOWN, 0, Point(120, 90));
    SendMessageW(win.get(), WM_KEYDOWN, VK_ESCAPE, 0);

    CHECK_FALSE(state.GetCropPickMode());
    CHECK(IsWindow(win.get()) != 0);

    state.SetCropRect({});
}

TEST_CASE("render window crop pick swaps in the crosshair cursor", "[render][window]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    App::Global().SetCropPickMode(true);

    LRESULT const handled =
        SendMessageW(win.get(), WM_SETCURSOR, reinterpret_cast<WPARAM>(win.get()),
                     MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECK(handled == TRUE);

    App::Global().SetCropPickMode(false);
    LRESULT const ignored =
        SendMessageW(win.get(), WM_SETCURSOR, reinterpret_cast<WPARAM>(win.get()),
                     MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    CHECK(ignored != TRUE);
}

TEST_CASE("render window drops the crop drag when capture is lost", "[render][window]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    auto& state = App::Global();
    state.SetCropRect({});
    state.SetCropPickMode(true);

    SendMessageW(win.get(), WM_LBUTTONDOWN, 0, Point(300, 200));
    SendMessageW(win.get(), WM_CAPTURECHANGED, 0, 0);
    App::CropRect const after_loss = state.GetCropRect();
    SendMessageW(win.get(), WM_MOUSEMOVE, 0, Point(10, 10));

    CHECK(state.GetCropRect().w == after_loss.w);
    CHECK(state.GetCropRect().h == after_loss.h);

    state.SetCropPickMode(false);
    state.SetCropRect({});
}

TEST_CASE("scene3d look starts and ends with the right mouse button", "[render][camera]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    Scene3d::SetInputEnabled(true);

    CHECK_FALSE(Scene3d::LookActive());
    CHECK(Scene3d::HandleLookMessage(win.get(), WM_RBUTTONDOWN, 0, 0));
    CHECK(Scene3d::LookActive());
    CHECK(Scene3d::HandleLookMessage(win.get(), WM_RBUTTONUP, 0, 0));
    CHECK_FALSE(Scene3d::LookActive());

    Scene3d::SetInputEnabled(false);
}

TEST_CASE("scene3d look ignores every message while input is disabled", "[render][camera]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    Scene3d::SetInputEnabled(false);

    CHECK_FALSE(Scene3d::HandleLookMessage(win.get(), WM_RBUTTONDOWN, 0, 0));
    CHECK_FALSE(Scene3d::LookActive());
    CHECK_FALSE(Scene3d::HandleLookMessage(win.get(), WM_MOUSEMOVE, 0, 0));
}

TEST_CASE("scene3d disabling input while looking releases the cursor", "[render][camera]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    Scene3d::SetInputEnabled(true);
    REQUIRE(Scene3d::HandleLookMessage(win.get(), WM_RBUTTONDOWN, 0, 0));
    REQUIRE(Scene3d::LookActive());

    Scene3d::SetInputEnabled(false);
    CHECK_FALSE(Scene3d::LookActive());
}

TEST_CASE("scene3d capture loss ends the look", "[render][camera]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    Scene3d::SetInputEnabled(true);
    REQUIRE(Scene3d::HandleLookMessage(win.get(), WM_RBUTTONDOWN, 0, 0));

    CHECK_FALSE(Scene3d::HandleLookMessage(win.get(), WM_CAPTURECHANGED, 0, 0));
    CHECK_FALSE(Scene3d::LookActive());

    Scene3d::SetInputEnabled(false);
}

TEST_CASE("scene3d movement keys are only polled while looking", "[render][camera]") {
    LiveRenderWindow const win;
    REQUIRE(win.get() != nullptr);
    Scene3d::SetInputEnabled(true);

    Scene3d::CameraInput idle{};
    Scene3d::PollCameraInput(idle);
    CHECK_FALSE(idle.forward);
    CHECK_FALSE(idle.back);
    CHECK_FALSE(idle.fast);

    Scene3d::SetInputEnabled(false);
}

TEST_CASE("free camera integrates movement and look deltas", "[render][camera]") {
    Scene3d::FreeCamera cam;
    cam.speed = 10.0F;

    Scene3d::CameraInput in{};
    in.forward = true;
    Scene3d::UpdateFreeCamera(cam, in, 1.0F);
    CHECK(cam.z != 0.0F);

    Scene3d::FreeCamera fast = cam;
    Scene3d::CameraInput boosted = in;
    boosted.fast = true;
    float const before = cam.z;
    Scene3d::UpdateFreeCamera(fast, boosted, 1.0F);
    CHECK(std::abs(fast.z - before) > 0.0F);

    Scene3d::FreeCamera looked;
    Scene3d::CameraInput look{};
    look.look_dx = 40.0F;
    look.look_dy = 20.0F;
    Scene3d::UpdateFreeCamera(looked, look, 1.0F);
    CHECK(looked.yaw != 0.0F);
    CHECK(looked.pitch != 0.0F);
}

TEST_CASE("free camera framing places it back from the scene bounds", "[render][camera]") {
    Scene3d::FreeCamera cam;
    std::array<float, 3> const center = {10.0F, 20.0F, 30.0F};
    Scene3d::PlaceFreeCamera(cam, center.data(), 50.0F);

    CHECK(cam.x == 10.0F);
    CHECK(cam.z < 30.0F);
    CHECK(cam.speed > 0.0F);

    Scene3d::FreeCamera degenerate;
    std::array<float, 3> const origin = {0.0F, 0.0F, 0.0F};
    Scene3d::PlaceFreeCamera(degenerate, origin.data(), 0.0F);
    CHECK(degenerate.speed > 0.0F);
}
