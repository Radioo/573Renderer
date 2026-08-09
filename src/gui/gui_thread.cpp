#include "gui_thread.h"
#include "gui_window.h"
#include "../state/app_state.h"
#include "../support/log.h"

#include <atomic>
#include <future>
#include <thread>
#include <utility>

namespace GuiThread {

namespace {
std::thread g_thread;
}
namespace {
std::atomic<bool> g_running{false};
}

namespace {
void ThreadMain(HINSTANCE hinst, std::promise<bool> init_result) {
    Gui::Window w{};
    if (!Gui::Init(w, hinst)) {
        LOG("GuiThread", "Gui::Init failed - GUI thread exiting");
        g_running = false;
        init_result.set_value(false);
        return;
    }
    init_result.set_value(true);

    LOG("GuiThread", "GUI thread started (HWND=%p, TID=%lu)", w.hwnd, GetCurrentThreadId());

    auto& state = App::Global();
    while (g_running.load(std::memory_order_acquire) &&
           !state.ShouldExit().load(std::memory_order_acquire)) {
        if (!Gui::PumpAndRender(w)) {
            LOG("GuiThread", "GUI window closed - GUI thread stopping "
                             "(renderer continues headlessly)");
            break;
        }
        Sleep(16);
    }

    Gui::Shutdown(w);
    g_running = false;
    LOG("GuiThread", "GUI thread exited");
}
}

bool Start(HINSTANCE hinst) {
    if (g_running.exchange(true)) {
        return true;
    }
    std::promise<bool> init_promise;
    std::future<bool> init_ok = init_promise.get_future();
    g_thread = std::thread(&ThreadMain, hinst, std::move(init_promise));
    if (!init_ok.get()) {
        if (g_thread.joinable()) g_thread.join();
        return false;
    }
    return true;
}

void Stop() {
    g_running = false;
    if (g_thread.joinable()) g_thread.join();
}

}
