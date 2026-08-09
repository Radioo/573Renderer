#include "gui_pixels.h"

#include "gui_layout_constants.h"
#include "imgui.h"
#include "gui_window.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"
#include "state/telemetry.h"

#include "warp_device.h"

#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <string>

namespace {

void DrainThreadMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

class LiveWindow {
public:
    LiveWindow() : ok_((DrainThreadMessages(), Gui::Init(window_, GetModuleHandleW(nullptr)))) {}
    ~LiveWindow() {
        if (ok_) Gui::Shutdown(window_);
        DrainThreadMessages();
    }
    LiveWindow(const LiveWindow&) = delete;
    LiveWindow& operator=(const LiveWindow&) = delete;
    LiveWindow(LiveWindow&&) = delete;
    LiveWindow& operator=(LiveWindow&&) = delete;

    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] Gui::Window& get() { return window_; }

private:
    Gui::Window window_;
    bool ok_ = false;
};

}

TEST_CASE("gui window creates a device and exposes its HWND", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());
    CHECK(win.get().hwnd != nullptr);
    CHECK(win.get().device != nullptr);
    CHECK(Gui::GetHwnd() == win.get().hwnd);
}

TEST_CASE("gui window clamps its minimum track size to the layout constants", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());

    MINMAXINFO mmi = {};
    mmi.ptMinTrackSize.x = 1;
    mmi.ptMinTrackSize.y = 1;
    SendMessageW(win.get().hwnd, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&mmi));

    RECT expected = {0, 0, Gui::kMinClientW, Gui::kMinClientH};
    AdjustWindowRect(&expected, WS_OVERLAPPEDWINDOW, FALSE);
    CHECK(mmi.ptMinTrackSize.x == expected.right - expected.left);
    CHECK(mmi.ptMinTrackSize.y == expected.bottom - expected.top);
}

TEST_CASE("gui window swallows the GDI background erase", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());
    CHECK(SendMessageW(win.get().hwnd, WM_ERASEBKGND, 0, 0) == 1);
}

TEST_CASE("gui window swallows the ALT key menu", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());
    CHECK(SendMessageW(win.get().hwnd, WM_SYSCOMMAND, SC_KEYMENU, 0) == 0);
}

TEST_CASE("gui window services a live resize from inside the message handler", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());

    SendMessageW(win.get().hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(900, 700));
    CHECK_FALSE(win.get().resize_pending);

    win.get().resize_pending = false;
    SendMessageW(win.get().hwnd, WM_SIZE, SIZE_MINIMIZED, MAKELPARAM(0, 0));
    CHECK_FALSE(win.get().resize_pending);
}

TEST_CASE("gui window paints on WM_PAINT and validates the region", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());

    InvalidateRect(win.get().hwnd, nullptr, TRUE);
    SendMessageW(win.get().hwnd, WM_PAINT, 0, 0);

    RECT update = {};
    CHECK(GetUpdateRect(win.get().hwnd, &update, FALSE) == 0);
}

TEST_CASE("gui window pump keeps running until WM_QUIT", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());

    CHECK(Gui::PumpAndRender(win.get()));

    PostQuitMessage(0);
    CHECK_FALSE(Gui::PumpAndRender(win.get()));
}

TEST_CASE("gui window recovers from a lost device", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());

    win.get().device_lost = true;
    CHECK(Gui::PumpAndRender(win.get()));
    CHECK_FALSE(win.get().device_lost);
}

TEST_CASE("gui window renders the setup view onto the backbuffer", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());
    App::Global().SetBootState(App::BootState::WaitingForDir);

    REQUIRE(Gui::PumpAndRender(win.get()));

    GuiTest::Framebuffer fb;
    REQUIRE(GuiTest::CaptureBackBuffer(win.get().device, fb));
    CHECK(fb.width > 0);
    CHECK(fb.height > 0);
    CHECK(GuiTest::CountPixels(fb, GuiTest::kShellClearColor, 8) > (fb.width * fb.height) / 20);
}

TEST_CASE("gui window paints the per-profile accent into the ready view", "[gui][window]") {
    LiveWindow win;
    if (!win.ok()) SKIP("no D3D9 device available: " << WarpD3D9::LastError());
    App::Global().SetBootState(App::BootState::Ready);
    App::Global().SetActiveBackendId("afp_modern");
    App::Status ready;
    ready.scene_loaded = true;
    ready.current_ifs_path = "bg_accent.ifs";
    App::Global().SetStatus(ready);

    App::Global().SetGameProfileSlug("sdvx7");
    REQUIRE(Gui::PumpAndRender(win.get()));
    GuiTest::Rgb const sdvx_button = GuiTest::StyleColor(ImGuiCol_Button);
    GuiTest::Framebuffer sdvx;
    REQUIRE(GuiTest::CaptureBackBuffer(win.get().device, sdvx));

    App::Global().SetGameProfileSlug("ddrworld");
    REQUIRE(Gui::PumpAndRender(win.get()));
    GuiTest::Rgb const ddr_button = GuiTest::StyleColor(ImGuiCol_Button);
    GuiTest::Framebuffer ddr;
    REQUIRE(GuiTest::CaptureBackBuffer(win.get().device, ddr));

    REQUIRE_FALSE(GuiTest::SameColor(sdvx_button, ddr_button, 4));
    CHECK(GuiTest::CountPixels(sdvx, sdvx_button, 4) > 0);
    CHECK(GuiTest::CountPixels(sdvx, ddr_button, 4) == 0);
    CHECK(GuiTest::CountPixels(ddr, ddr_button, 4) > 0);
    CHECK(GuiTest::CountPixels(ddr, sdvx_button, 4) == 0);

    App::Global().SetGameProfileSlug({});
    App::Global().SetStatus({});
    App::Global().SetBootState(App::BootState::WaitingForDir);
}
