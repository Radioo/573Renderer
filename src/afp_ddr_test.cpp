#include <cstdint>
#include <excpt.h>
#include <cstddef>
#include <system_error>
#include <ios>
#include <optional>
#include <cstdlib>
#include <cstdio>
#include "afp_ddr_test.h"
#include "afp_ddr.h"
#include "formats/ddr_arc.h"
#include "avs_boot.h"
#include "window.h"
#include "render_backend.h"
#include "app_globals.h"
#include "support/env.h"
#include "support/log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace DdrTest {

namespace {
LONG CALLBACK CrashHandler(PEXCEPTION_POINTERS ep) {
    DWORD const code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_PRIV_INSTRUCTION || code == 0x80000003) {
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        HMODULE m = nullptr;
        char name[MAX_PATH] = {0};
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(addr), &m);
        if (m != nullptr) GetModuleFileNameA(m, name, MAX_PATH);
        const char* base = name;
        for (const char* p = name; *p != 0; ++p)
            if (*p == '\\' || *p == '/') base = p + 1;
        unsigned long long const off =
            (m != nullptr) ? (unsigned long long)((uintptr_t)addr - (uintptr_t)m) : 0;
        void* bad = (code == EXCEPTION_ACCESS_VIOLATION)
                        ? reinterpret_cast<void*>(ep->ExceptionRecord->ExceptionInformation[1])
                        : nullptr;
        LOG("CRASH", "code=0x%08lx at %p  module=%s base=%p off=0x%llx  data=%p", code, addr,
            base[0] ? base : "?", (void*)m, off, bad);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
}

namespace {

namespace fs = std::filesystem;

int BootDdrTestStack(const std::string& modules_dir) {
    SetDllDirectoryA(modules_dir.c_str());

    if (!g_avs_dll.Load((modules_dir + "\\libavs-win64.dll").c_str())) return 2;
    if (!g_afp_dll.Load((modules_dir + "\\libafp-win64.dll").c_str())) return 2;
    if (!g_afpu_dll.Load((modules_dir + "\\libafputils-win64.dll").c_str())) return 2;
    if (!g_avs.Load(g_avs_dll)) {
        LOG("DDR-T", "avs func resolve failed");
        return 3;
    }

    if (!AvsManager::Boot(g_avs)) {
        LOG("DDR-T", "AVS boot failed");
        return 4;
    }

    g_d3d.width = 1280;
    g_d3d.height = 720;
    HWND hwnd = AppWindow::Create(1280, 720);
    if ((hwnd == nullptr) || !g_d3d.Init(hwnd)) {
        LOG("DDR-T", "D3D init failed");
        return 5;
    }

    if (!DdrAfp::Boot(g_afp_dll, g_afpu_dll, g_d3d)) {
        LOG("DDR-T", "DDR AFP boot failed");
        return 6;
    }
    return 0;
}

bool LoadArcAsIfs(const std::string& apath) {
    std::string inner;
    std::vector<uint8_t> ifs = DdrArc::ExtractFirstIfs(apath, inner);
    if (ifs.empty()) {
        LOG("DDR-T", "arc has no .ifs: %s", apath.c_str());
        return false;
    }
    std::string base = inner;
    size_t const slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    std::string pkg = base;
    if (pkg.size() > 4 && pkg.ends_with(".ifs")) pkg.resize(pkg.size() - 4);
    fs::path const tmpdir = fs::temp_directory_path() / "573ddr";
    std::error_code ec;
    fs::create_directories(tmpdir, ec);
    fs::path const tmp_ifs = tmpdir / base;
    {
        std::ofstream o(tmp_ifs, std::ios::binary | std::ios::trunc);
        o.write(reinterpret_cast<const char*>(ifs.data()), (std::streamsize)ifs.size());
    }
    LOG("DDR-T", "load arc '%s' inner='%s' (%zu bytes) pkg='%s'", apath.c_str(), inner.c_str(),
        ifs.size(), pkg.c_str());
    if (g_avs.avs_fs_umount != nullptr) {
        g_avs.avs_fs_umount("/afp/packages");
        g_avs.avs_fs_umount("/data");
    }
    return DdrAfp::LoadIfs(g_avs, g_avs_dll, tmp_ifs.string(), pkg);
}

struct SelfDiffTracker {
    bool on = Support::EnvFlag("DDR_SELFDIFF");
    int base = Support::EnvInt("DDR_SELFDIFF_BASE").value_or(600);
    int per = Support::EnvInt("DDR_SELFDIFF_PERIOD").value_or(123);
    std::vector<uint8_t> ref;
    double min_mad = 1e9;
    int min_k = -1;
    int min_frame = -1;

    void Sample(int i) {
        if (!on || i < base || per <= 0 || ((i - base) % per) != 0) return;
        std::vector<uint8_t> cur;
        int rw = 0;
        int rh = 0;
        if (!g_d3d.ReadOffscreenBGRA(cur, rw, rh)) return;
        if (i == base) {
            ref = cur;
            LOG("DDR-SD", "baseline frame=%d %dx%d period=%d", i, rw, rh, per);
            return;
        }
        if (ref.size() != cur.size() || cur.empty()) return;
        unsigned long long tot = 0;
        size_t n = 0;
        for (size_t p = 0; p + 2 < cur.size(); p += 16) {
            tot += (unsigned)abs((int)cur[p] - (int)ref[p]);
            tot += (unsigned)abs((int)cur[p + 1] - (int)ref[p + 1]);
            tot += (unsigned)abs((int)cur[p + 2] - (int)ref[p + 2]);
            n += 3;
        }
        double const mad = (n != 0U) ? (double)tot / (double)n : 0.0;
        int const k = (i - base) / per;
        if (mad < min_mad) {
            min_mad = mad;
            min_k = k;
            min_frame = i;
        }
        LOG("DDR-SD", "k=%d frame=%d offset=%d MAD=%.4f", k, i, i - base, mad);
    }

    void LogBest() const {
        if (!on || min_k < 0) return;
        LOG("DDR-SD",
            "BEST loop: k=%d frame=%d offset=%d MAD=%.4f (MAD~0 => clean visible loop = offset)",
            min_k, min_frame, min_frame - base, min_mad);
    }
};

}

int Run(const std::string& modules_dir, const std::string& arc_path, const std::string& out_png,
        int frames) {
    PVOID veh = AddVectoredExceptionHandler(1, CrashHandler);
    (void)veh;
    LOG("DDR-T", "ddr-test: modules=%s arc=%s out=%s frames=%d", modules_dir.c_str(),
        arc_path.c_str(), out_png.c_str(), frames);

    int const boot_rc = BootDdrTestStack(modules_dir);
    if (boot_rc != 0) return boot_rc;

    std::optional<std::string> pre = Support::EnvVar("DDR_PRELOAD_ARC");
    if (pre && !pre->empty()) {
        LOG("DDR-T", "preloading companion arc: %s", pre->c_str());
        if (!LoadArcAsIfs(*pre)) LOG("DDR-T", "companion preload failed (continuing)");
    }

    if (!LoadArcAsIfs(arc_path)) {
        LOG("DDR-T", "LoadIfs failed");
        return 8;
    }

    frames = std::max(frames, 1);
    int const cap_from = Support::EnvInt("DDR_CAPTURE_FROM").value_or(-1);
    int const cap_to = Support::EnvInt("DDR_CAPTURE_TO").value_or(-1);
    std::string seq_dir = out_png;
    size_t const sl = seq_dir.find_last_of("/\\");
    seq_dir = (sl == std::string::npos) ? std::string(".") : seq_dir.substr(0, sl);

    SelfDiffTracker sd;

    for (int i = 0; i < frames; i++) {
        AppWindow::PumpMessages();
        g_d3d.BeginFrame();
        DdrAfp::RenderFrame(1.0F / 60.0F);
        sd.Sample(i);
        if (cap_from >= 0 && i >= cap_from && i <= cap_to) {
            char p[512];
            snprintf(p, sizeof(p), "%s/seq_%04d.png", seq_dir.c_str(), i);
            D3D9State_RequestScreenshot(p);
        } else if (i == frames - 1) {
            D3D9State_RequestScreenshot(out_png.c_str());
        }
        g_d3d.EndFrame();
    }
    LOG("DDR-T", "rendered %d frames, final draw_count via log", frames);
    sd.LogBest();

    g_d3d.Shutdown();
    AvsManager::Shutdown(g_avs);
    return 0;
}

}
