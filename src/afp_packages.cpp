#include "afp_boot.h"
#include "afp_funcs.h"
#include "afpu_funcs.h"
#include "avs_funcs.h"
#include "engine_session.h"
#include "avs_boot.h"
#include "avs_xml.h"
#include "ifs_inspect.h"
#include "support/log.h"
#include "game_profile.h"
#include "render_backend.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "app_globals.h"

namespace {

void LogPackageDirs(const AvsFuncs& avs) {
    auto opendir = avs.avs_fs_opendir;
    auto readdir = avs.avs_fs_readdir;
    auto closedir = avs.avs_fs_closedir;
    if ((opendir == nullptr) || (readdir == nullptr) || (closedir == nullptr)) return;

    const char* dirs_to_list[] = {"/afp/packages",     "/afp/packages/geo",   "/afp/packages/vg",
                                  "/afp/packages/afp", "/afp/packages/magic", nullptr};
    for (int d = 0; dirs_to_list[d] != nullptr; d++) {
        int const dir = opendir(dirs_to_list[d]);
        if (dir > 0) {
            LOG("AFP", "Contents of %s:", dirs_to_list[d]);
            for (int i = 0; i < 30; i++) {
                const char* entry = readdir(dir);
                if (entry == nullptr) break;
                LOG("AFP", "  - %s", entry);
            }
            closedir(dir);
        } else {
            LOG("AFP", "Cannot open dir: %s", dirs_to_list[d]);
        }
    }
    const char* try_paths[] = {"/afp/packages/tex/texturelist.xml",
                               "/afp/packages/tex/sub_customize_bg001.png",
                               "/afp/packages/tex/tex000",
                               "/afp/packages/texturelist.xml",
                               "/afp/packages/tex",
                               nullptr};
    for (int p = 0; try_paths[p] != nullptr; p++) {
        int const fd = avs.avs_fs_open(try_paths[p], 1, 420);
        LOG("AFP", "open('%s') = %d (0x%08x)", try_paths[p], fd, (unsigned int)fd);
        if (fd > 0) {
            uint8_t hdr[16] = {};
            int const rd = avs.avs_fs_read(fd, hdr, 16);
            printf("[AFP]   read %d bytes: ", rd);
            for (int j = 0; j < (rd > 16 ? 16 : rd); j++)
                printf("%02x ", hdr[j]);
            printf("\n");
            avs.avs_fs_close(fd);
        }
    }
}

int ReadNgpPackage(AfpuFuncs& afpu, const std::string& pkg_hint) {
    LOG("AFP", "Trying afpu_ngp_read_local (hint='%s')...", pkg_hint.c_str());
    int ngp_ret = 0;
    auto try_name = [&](const char* n) {
        int const r = afpu.afpu_ngp_read_local(n, "/afp/packages", 0);
        LOG("AFP", "  afpu_ngp_read_local('%s') = %d (0x%08x)", n, r, (unsigned int)r);
        if (r > 0) ngp_ret = r;
    };
    if (!pkg_hint.empty()) try_name(pkg_hint.c_str());
    if (ngp_ret <= 0) try_name("title");
    if (ngp_ret <= 0) try_name("fcombo00");
    if (ngp_ret <= 0) try_name("sub_customize_bg001");
    LOG("AFP", "afpu_ngp_read_local final: %d (0x%08x)", ngp_ret, (unsigned int)ngp_ret);
    return ngp_ret;
}

std::vector<std::string> BuildMasterCandidates(const AvsFuncs& avs, const std::string& pkg_hint) {
    std::vector<std::string> name_storage;
    auto push_unique = [&](const std::string& n) {
        if (n.empty()) return;
        for (auto& e : name_storage)
            if (e == n) return;
        name_storage.push_back(n);
    };
    if (!pkg_hint.empty()) push_unique(pkg_hint);
    for (const char* n : {"title", "1p_fullcombo", "2p_fullcombo", "top"})
        push_unique(std::string(n));
    auto tree = AvsXml::LoadFromFile(avs, "/afp/packages/afp/afplist.xml");
    if (tree) {
        std::vector<std::string> from_xml;
        AvsXml::GatherMatchAttr(avs, tree, "afplist/afp", "name", from_xml);
        size_t const before = name_storage.size();
        for (auto& n : from_xml)
            push_unique(n);
        LOG("AFP",
            "afplist.xml lists %zu animations - added %zu "
            "as fallback master candidates",
            from_xml.size(), name_storage.size() - before);
        std::string all;
        for (auto& n : from_xml) {
            all += n;
            all += ' ';
        }
        LOG("AFP", "afplist names: %s", all.c_str());
    }
    return name_storage;
}

void TryPlayMasterAnimation(EngineSession& es, uint32_t pkg_id,
                            const std::vector<std::string>& names) {
    AfpFuncs const& afp = es.afp;
    auto get_afp_info = es.afpu.afpu_afp_get_info_in_package;
    auto set_flag_mask = afp.afp_set_flag_mask;
    for (const std::string& name : names) {
        if (get_afp_info == nullptr) break;

        uint64_t info[8] = {};
        int const info_ret = get_afp_info(info, pkg_id, name.c_str());
        LOG("AFP", "afpu_afp_get_info('%s') = %d, info: %llx %llx %llx %llx", name.c_str(),
            info_ret, info[0], info[1], info[2], info[3]);

        if (info_ret < 0) continue;

        auto data_id = (uint32_t)info[3];
        void* data_ptr = (void*)info[2];

        auto inner_level = (afp.afp_get_create_level != nullptr) ? afp.afp_get_create_level() : 0;
        if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(0);

        LOG("AFP", "afp_stream_play(data_id=0x%x, data_ptr=%p)...", data_id, data_ptr);
        int const r = afp.afp_stream_play(data_id, (const char*)data_ptr, 0, 0);
        LOG("AFP", "  returned: 0x%08x", (unsigned)r);

        if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(inner_level);

        if (r >= 0) {
            es.stream_id = (uint32_t)r;
            es.anim_name = name;

            LOG("AFP", "Master animation '%s' playing! stream=0x%08x", name.c_str(), (unsigned)r);

            if (set_flag_mask != nullptr) set_flag_mask((uint32_t)r, 512, 512);

            if (afp.afp_stream_set_speed != nullptr) afp.afp_stream_set_speed((uint32_t)r, 1.0F);

            break;
        }
    }
}

void TryPlayBitmapFallback(EngineSession& es, const std::string& pkg_hint) {
    auto play_bitmap = es.afp.afp_stream_play_bitmap_by_name;
    if (play_bitmap == nullptr) return;
    std::vector<const char*> bmp_names;
    if (!pkg_hint.empty()) bmp_names.push_back(pkg_hint.c_str());
    bmp_names.push_back("sub_customize_bg001");
    bmp_names.push_back("sub_customize_bg");
    for (const char* n : bmp_names) {
        int const id = play_bitmap(n, 0);
        if (id > 0) {
            es.stream_id = id;
            LOG("AFP", "Bitmap '%s' playing! id=0x%08x", n, id);
            break;
        }
    }
}

}

bool AfpManager::LoadPackages(EngineSession& es, const std::string& pkg_hint) {
    AfpFuncs const& afp = es.afp;
    AfpuFuncs& afpu = es.afpu;
    AvsFuncs const& avs = es.avs;
    LOG("AFP", "Loading packages...");

    LogPackageDirs(avs);

    int const ngp_ret = ReadNgpPackage(afpu, pkg_hint);

    int const mt_ret = afpu.afpu_ngp_mounttable_load();
    LOG("AFP", "afpu_ngp_mounttable_load returned: %d", mt_ret);

    if (afpu.afpu_get_loaded_package_count != nullptr) {
        LOG("AFP", "Loaded packages: %d", afpu.afpu_get_loaded_package_count());
    }

    uint32_t pkg_id = (ngp_ret > 0) ? (uint32_t)ngp_ret : 0;
    if (pkg_id == 0U) {
        pkg_id = afpu.afpuloc_get_first_package_id();
    }
    LOG("AFP", "Using package ID: 0x%08x", pkg_id);

    if ((pkg_id != 0U) && pkg_id != 0xFFFFFFFE) {
        int const streams_ret = afpu.afpu_package_open_streams(pkg_id);
        LOG("AFP", "afpu_package_open_streams returned: %d (0x%08x)", streams_ret,
            (unsigned int)streams_ret);

        if (afpu.afpuloc_package_has_animation != nullptr) {
            int const has_anim = afpu.afpuloc_package_has_animation(pkg_id);
            LOG("AFP", "Package has animation: %d", has_anim);
        }

        if (afpu.afpu_package_dump != nullptr) afpu.afpu_package_dump();

        es.pkg_id = pkg_id;

        auto old_level = (afp.afp_get_create_level != nullptr) ? afp.afp_get_create_level() : 0;
        if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(0);

        std::vector<std::string> const names = BuildMasterCandidates(avs, pkg_hint);
        TryPlayMasterAnimation(es, pkg_id, names);

        if (es.stream_id == 0xFFFFFFFC || (int)es.stream_id < 0) {
            TryPlayBitmapFallback(es, pkg_hint);
        }

        if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(old_level);
    }

    return true;
}

namespace {

struct BootIfs {
    const char* rel_path;
    const char* pkg_name;
};

bool LoadOneBootIfs(EngineSession& es, const std::string& data_dir, const BootIfs& b, int index) {
    namespace fs = std::filesystem;
    AfpuFuncs const& afpu = es.afpu;
    AvsFuncs& avs = es.avs;

    fs::path const full = fs::path(data_dir) / b.rel_path;
    if (!fs::exists(full)) {
        LOG("AFP", "Boot IFS missing: %s - skipping", full.string().c_str());
        return false;
    }

    char mountpoint[64];
    snprintf(mountpoint, sizeof(mountpoint), "/afp_boot_%d", index);

    std::string const vfs_src = std::string("/data_root/") + b.rel_path;
    if (!AvsManager::MountIfsImage(avs, mountpoint, vfs_src)) return true;

    int pkg_id = afpu.afpu_ngp_read_local(b.pkg_name, mountpoint, 0);
    LOG("AFP", "  boot IFS '%s' @ %s -> pkg_id=0x%08x", b.pkg_name, mountpoint, (unsigned)pkg_id);
    if (pkg_id <= 0) return true;

    auto boot_filters = IfsInspect::ReadAtlasFilters(avs, mountpoint);
    for (auto& af : boot_filters)
        AfpD3D9::EnqueueAtlasFilter(af.mag_filter_d3d, af.min_filter_d3d);
    if (!boot_filters.empty()) {
        LOG("AFP", "  '%s': %zu atlas filter(s) queued", b.pkg_name, boot_filters.size());
    }

    if (afpu.afpu_package_open_streams != nullptr) afpu.afpu_package_open_streams((uint32_t)pkg_id);

    es.persistent_pkg_ids.push_back((uint32_t)pkg_id);
    return true;
}

}

void AfpManager::LoadBootIfses(EngineSession& es, const std::string& data_dir) {
    AfpFuncs const& afp = es.afp;
    AvsFuncs& avs = es.avs;
    static const BootIfs kBootIfses[] = {
        {.rel_path = "graphic/1/common.ifs", .pkg_name = "common"},
        {.rel_path = "graphic/1/common_j.ifs", .pkg_name = "common_j"},
        {.rel_path = "graphic/1/gameparts.ifs", .pkg_name = "gameparts"},
        {.rel_path = "graphic/1/gameparts_j.ifs", .pkg_name = "gameparts_j"},
        {.rel_path = "graphic/1/graph.ifs", .pkg_name = "graph"},
        {.rel_path = "graphic/1/graph_j.ifs", .pkg_name = "graph_j"},
        {.rel_path = "graphic/1/mdata.ifs", .pkg_name = "mdata"},
        {.rel_path = "graphic/1/dlbg.ifs", .pkg_name = "dlbg"},
        {.rel_path = "graphic/1/sub_common.ifs", .pkg_name = "sub_common"},
        {.rel_path = "graphic/1/sub_premium_area.ifs", .pkg_name = "sub_premium_area"},
        {.rel_path = "graphic/1/sub_premium_area_j.ifs", .pkg_name = "sub_premium_area_j"},
        {.rel_path = "graphic/qp_main2.ifs", .pkg_name = "qp_main2"},
        {.rel_path = "graphic/1/led.ifs", .pkg_name = "led"},
    };

    if (!AvsManager::MountFsRoot(avs, "/data_root", data_dir)) {
        LOG("AFP", "LoadBootIfses: could not mount /data_root, aborting");
        return;
    }

    int const saved_level = (afp.afp_get_create_level != nullptr) ? afp.afp_get_create_level() : 0;
    if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(1);
    LOG("AFP", "LoadBootIfses: create_level 1 (persistent streams)");

    int index = 0;
    for (const auto& b : kBootIfses) {
        if (LoadOneBootIfs(es, data_dir, b, index)) index++;
    }

    LOG("AFP", "LoadBootIfses: %zu persistent packages registered", es.persistent_pkg_ids.size());

    if (afp.afp_set_create_level != nullptr) afp.afp_set_create_level(saved_level);
    LOG("AFP", "LoadBootIfses: create_level restored to %d", saved_level);

    AfpD3D9::MarkPersistentBoundary();
}

void AfpManager::DestroySceneStreams(AfpFuncs& afp) {
    if (afp.afp_stream_destroy == nullptr) return;
    auto destroy_one = [&](uint32_t sid) {
        if (sid == 0xFFFFFFFC || (int)sid < 0) return;
        if (afp.afp_stream_control) {
            LOG("AFP", "stream_control(5, 0x%x)", sid);
            afp.afp_stream_control(5, sid);
        }
        LOG("AFP", "stream_destroy(5, 0x%x, 0)", sid);
        afp.afp_stream_destroy(5, sid, 0);
    };
    destroy_one(g_engine.stream_id);
    for (uint32_t const sid : g_engine.extra_streams)
        destroy_one(sid);
    LOG("AFP", "stream_destroy(8, 0, 0) -- sweep priority=0 Table A streams");
    int const n = afp.afp_stream_destroy(8, 0, 0);
    LOG("AFP", "  returned %d", n);
}

void AfpManager::UnloadPackages(EngineSession& es) {
    AfpFuncs& afp = es.afp;
    AfpuFuncs const& afpu = es.afpu;
    if (!es.afp_booted) return;

    UnloadAllCompanions(es);

    auto enum_streams = [&](const char* label) {
        if (!afp.afp_get_layers_by_nr) return;
        uint32_t buf[512] = {};
        int n = afp.afp_get_layers_by_nr(0, buf, 512);
        LOG("AFP", "[%s] afp_get_layers_by_nr(category=0) returned %d", label, n);
        for (int i = 0; i < n && i < 32; i++) {
            LOG("AFP", "  [%s] stream_id=0x%08x", label, buf[i]);
        }
        n = afp.afp_get_layers_by_nr(-1, buf, 512);
        LOG("AFP", "[%s] afp_get_layers_by_nr(category=-1) returned %d", label, n);
    };

    if (afp.afp_stream_destroy != nullptr) {
        enum_streams("pre-destroy");

        DestroySceneStreams(afp);

        enum_streams("post-sweep");

        HMODULE afp_core = GetModuleHandleA("afp-core.dll");
        const uintptr_t table_b_off = GameProfile::ActiveOffsets().afp_table_b_count;
        if ((afp_core != nullptr) && (table_b_off != 0U)) {
            auto* table_b_count =
                reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(afp_core) + table_b_off);
            LOG("AFP", "[post-sweep] Table B count = %u", (unsigned)*table_b_count);
        }

        if (afp.afp_system_dump_layer_info != nullptr) {
            LOG("AFP", "[post-sweep] afp_system_dump_layer_info(0):");
            afp.afp_system_dump_layer_info(0);
        }
    }

    if ((afpu.afpu_package_control != nullptr) && (es.pkg_id != 0U) && es.pkg_id != 0xFFFFFFFE) {
        LOG("AFP", "package_control(6, 0x%x, 0)", es.pkg_id);
        afpu.afpu_package_control(6, es.pkg_id, nullptr);
    }

    AfpD3D9::ResetAllTextures();

    es.extra_streams.clear();
    es.stream_id = 0xFFFFFFFC;
    es.anim_name.clear();
    es.pkg_id = 0;
}
