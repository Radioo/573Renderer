#include "avs_boot.h"
#include "avs_funcs.h"
#include "support/dll_loader.h"
#include "support/log.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>

namespace {
void* g_avs_heap = nullptr;
}
static const int AVS_HEAP_SIZE = 128 * 1024 * 1024;
namespace {
bool g_avs_booted = false;
}

namespace {
void __cdecl avs_log_writer(const char* chars, int nchars, void* ctx) {
    (void)ctx;
    if ((chars != nullptr) && nchars > 0) {
        fwrite(chars, 1, nchars, stdout);
        if (nchars > 0 && chars[nchars - 1] != '\n') fputc('\n', stdout);
        fflush(stdout);
        static FILE* af = nullptr;
        if (af == nullptr) fopen_s(&af, "avs_out.log", "w");
        if (af != nullptr) {
            fwrite(chars, 1, nchars, af);
            if (chars[nchars - 1] != '\n') fputc('\n', af);
            fflush(af);
        }
    }
}
}

bool AvsManager::Boot(AvsFuncs& avs) {
    LOG("AVS", "Booting AVS...");

    g_avs_heap = malloc(AVS_HEAP_SIZE);
    if (g_avs_heap == nullptr) {
        LOG("AVS", "Failed to allocate %d MB heap", AVS_HEAP_SIZE / (1024 * 1024));
        return false;
    }
    LOG("AVS", "Allocated %d MB heap at %p", AVS_HEAP_SIZE / (1024 * 1024), g_avs_heap);

    uint8_t prop_buf[4096];
    memset(prop_buf, 0, sizeof(prop_buf));

    auto* prop = avs.property_create(7, prop_buf, 0xD18);
    LOG("AVS", "property_create(7, buf, 0xD18) = %p", prop);
    if (prop == nullptr) {
        LOG("AVS", "Failed to create config property tree, trying NULL config");
        avs.avs_boot(nullptr, g_avs_heap, AVS_HEAP_SIZE, nullptr,
                     reinterpret_cast<void*>(avs_log_writer), nullptr);
    } else {
        char cwd[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, cwd);
        for (char* p = cwd; *p != 0; p++)
            if (*p == '\\') *p = '/';
        LOG("AVS", "Root device: %s", cwd);
        avs.property_node_create(prop, nullptr, 0xB, "/config/fs/root/device", cwd);
        avs.property_node_create(prop, nullptr, 0x5, "/config/fs/nr_mountpoint", 256);
        avs.property_node_create(prop, nullptr, 0x5, "/config/fs/nr_filedesc", 4096);
        avs.property_node_create(prop, nullptr, 0x5, "/config/thread/nr_mutex", 256);
        avs.property_node_create(prop, nullptr, 0xB, "/config/log/level", "misc");

        auto* config_node = avs.property_search(prop, nullptr, "/config");
        LOG("AVS", "property_search('/config') = %p", config_node);

        LOG("AVS", "Calling avs_boot(config=%p, heap=%p, size=0x%x)...", config_node, g_avs_heap,
            AVS_HEAP_SIZE);
        avs.avs_boot(static_cast<void*>(config_node), g_avs_heap, AVS_HEAP_SIZE, nullptr,
                     reinterpret_cast<void*>(avs_log_writer), nullptr);

        avs.property_destroy(prop);
    }

    LOG("AVS", "avs_boot returned");

    if ((avs.avs_is_active != nullptr) && (avs.avs_is_active() != 0)) {
        LOG("AVS", "AVS is active!");
    } else {
        LOG("AVS", "WARNING: avs_is_active returned false");
    }

    g_avs_booted = true;
    return true;
}

bool AvsManager::MountIfs(AvsFuncs& avs, [[maybe_unused]] DllLoader& avs_dll,
                          const std::string& ifs_path) {
    LOG("AVS", "Mounting IFS: %s", ifs_path.c_str());

    char abs_path[MAX_PATH];
    GetFullPathNameA(ifs_path.c_str(), MAX_PATH, abs_path, nullptr);
    std::string native_path = abs_path;
    for (auto& c : native_path)
        if (c == '\\') c = '/';

    std::string const ifs_dir = native_path.substr(0, native_path.find_last_of('/'));
    std::string const ifs_name = native_path.substr(native_path.find_last_of('/') + 1);
    LOG("AVS", "IFS dir: %s, file: %s", ifs_dir.c_str(), ifs_name.c_str());

    int ret = avs.avs_fs_mount("/data", ifs_dir.c_str(), "fs", "vf=0,posix=1");
    LOG("AVS", "avs_fs_mount(\"/data\", \"%s\", \"fs\") = %d (0x%08x)", ifs_dir.c_str(), ret,
        (unsigned int)ret);
    if (ret >= 0) {
        LOG("AVS", "Data directory mounted at /data");

        std::string const ifs_vfs_path = std::string("/data/") + ifs_name;
        ret = avs.avs_fs_mount("/afp/packages", ifs_vfs_path.c_str(), "imagefs", nullptr);
        LOG("AVS", "avs_fs_mount(\"/afp/packages\", \"%s\", \"imagefs\") = %d (0x%08x)",
            ifs_vfs_path.c_str(), ret, (unsigned int)ret);
        if (ret >= 0) {
            LOG("AVS", "IFS mounted at /afp/packages!");
            return true;
        }
    }

    ret = avs.avs_fs_mount("/data", native_path.c_str(), "imagefs", nullptr);
    LOG("AVS", "avs_fs_mount(\"/data\", \"%s\", \"imagefs\") = %d (0x%08x)", native_path.c_str(),
        ret, (unsigned int)ret);
    if (ret >= 0) return true;

    auto dump = avs.avs_fs_dump_mountpoint;
    if (dump != nullptr) {
        LOG("AVS", "Dumping mount points:");
        dump();
    }

    LOG("AVS", "WARNING: IFS mount failed. Will continue but AFP won't find packages.");
    return false;
}

bool AvsManager::MountFsRoot(AvsFuncs& avs, const std::string& vfs_mountpoint,
                             const std::string& host_dir) {
    char abs_path[MAX_PATH];
    GetFullPathNameA(host_dir.c_str(), MAX_PATH, abs_path, nullptr);
    std::string native = abs_path;
    for (auto& c : native)
        if (c == '\\') c = '/';
    int const ret = avs.avs_fs_mount(vfs_mountpoint.c_str(), native.c_str(), "fs", "vf=0,posix=1");
    if (ret < 0) {
        LOG("AVS", "MountFsRoot('%s', '%s') failed: 0x%08x", vfs_mountpoint.c_str(), native.c_str(),
            (unsigned)ret);
        return false;
    }
    LOG("AVS", "MountFsRoot('%s' <- '%s') ok", vfs_mountpoint.c_str(), native.c_str());
    return true;
}

bool AvsManager::MountIfsImage(AvsFuncs& avs, const std::string& mountpoint,
                               const std::string& vfs_ifs_path) {
    int const ret = avs.avs_fs_mount(mountpoint.c_str(), vfs_ifs_path.c_str(), "imagefs", nullptr);
    if (ret < 0) {
        LOG("AVS", "MountIfsImage('%s' <- '%s') failed: 0x%08x", mountpoint.c_str(),
            vfs_ifs_path.c_str(), (unsigned)ret);
        return false;
    }
    LOG("AVS", "MountIfsImage('%s' <- '%s') ok", mountpoint.c_str(), vfs_ifs_path.c_str());
    return true;
}

void AvsManager::Shutdown(AvsFuncs& avs) {
    if (g_avs_booted) {
        avs.avs_shutdown();
        g_avs_booted = false;
    }
    if (g_avs_heap != nullptr) {
        free(g_avs_heap);
        g_avs_heap = nullptr;
    }
}

bool AvsManager::IsBooted() {
    return g_avs_booted;
}
