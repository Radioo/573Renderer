#pragma once

#include "support/dll_loader.h"
#include <cstdint>

typedef int (*afpu_boot_t)(void* config, void* afp_data);
typedef void (*afpu_shutdown_t)();
typedef int (*afpu_set_flag_t)(uint32_t flags, uint32_t mask);
typedef int (*afpu_set_config_t)(int type, ...);
typedef void* (*afpu_get_context_t)();
typedef int (*afpu_ext_command_t)(int cmd, void* arg);

typedef int (*afpu_ngp_mounttable_load_t)();
typedef int (*afpu_ngp_mounttable_load_from_property_t)(void* prop);
typedef int (*afpu_ngp_packages_exist_t)();
typedef int (*afpu_ngp_read_local_t)(const char* name, const char* path, int flags);
typedef int (*afpu_ngp_read_by_mounttable_t)();
typedef int (*afpu_ngp_detect_format_t)();

typedef uint32_t (*afpu_package_get_count_t)();
typedef const char* (*afpu_package_get_name_by_index_t)(int index);
typedef int (*afpu_package_find_by_name_t)(const char* name);
typedef int (*afpu_package_open_streams_t)(uint32_t pkg_id);
typedef uint32_t (*afpuloc_get_first_package_id_t)();
typedef uint32_t (*afpuloc_get_package_id_t)(const char* name);
typedef int (*afpu_get_loaded_package_count_t)();
typedef int (*afpuloc_package_has_animation_t)(uint32_t pkg_id);

typedef uint32_t (*afpuloc_get_package_info_t)(uint32_t pkg_id, int sel);
typedef const char* (*afpuloc_get_version_string_t)(uint32_t pkg_id, int sel);

typedef int (*afpu_package_control_t)(int cmd, uint32_t pkg_id, const char* reserved);

typedef int (*afpuloc_get_texture_info_by_id_t)(uint32_t tex_id, void* info_out);
typedef uint32_t (*afpuloc_get_texture_data_size_t)(uint32_t tex_id);

typedef int (*afpu_afp_get_info_in_package_t)(void* info, uint32_t pkg_id, const char* name);
typedef int (*afpu_image_lookup_t)(void* out_image_info_64, uint32_t pkg_id, const char* name);
typedef int (*afpu_image_to_stream_args_t)(void* out_stream_args_40, const void* image_info_64);
typedef int (*afpu_texture_get_bpp_t)(int format_id);

typedef uint32_t (*afpu_image_find_t)(const char* name);
typedef int (*afpu_image_get_info_t)(const char* name, void* info_out);

typedef int (*afpu_render_init_t)(void* render_context);
typedef int (*afpu_render_reset_t)();
typedef int (*afpu_render_info_t)();
typedef int (*afpu_render_flush_t)();

typedef int (*afpu_swap_data_t)(void* txp2_data);
typedef int (*afpu_texture_needs_swap_t)(void* txp2_data);

typedef int (*afpu_package_dump_t)();

struct AfpuFuncs {
    afpu_boot_t afpu_boot = nullptr;
    afpu_shutdown_t afpu_shutdown = nullptr;
    afpu_set_flag_t afpu_set_flag = nullptr;
    afpu_set_config_t afpu_set_config = nullptr;
    afpu_get_context_t afpu_get_context = nullptr;
    afpu_ext_command_t afpu_ext_command = nullptr;

    afpu_ngp_mounttable_load_t afpu_ngp_mounttable_load = nullptr;
    afpu_ngp_mounttable_load_from_property_t afpu_ngp_mounttable_load_from_property = nullptr;
    afpu_ngp_packages_exist_t afpu_ngp_packages_exist = nullptr;
    afpu_ngp_read_local_t afpu_ngp_read_local = nullptr;
    afpu_ngp_read_by_mounttable_t afpu_ngp_read_by_mounttable = nullptr;
    afpu_ngp_detect_format_t afpu_ngp_detect_format = nullptr;

    afpu_package_get_count_t afpu_package_get_count = nullptr;
    afpu_package_get_name_by_index_t afpu_package_get_name_by_index = nullptr;
    afpu_package_find_by_name_t afpu_package_find_by_name = nullptr;
    afpu_package_open_streams_t afpu_package_open_streams = nullptr;
    afpuloc_get_first_package_id_t afpuloc_get_first_package_id = nullptr;
    afpuloc_get_package_id_t afpuloc_get_package_id = nullptr;
    afpu_get_loaded_package_count_t afpu_get_loaded_package_count = nullptr;
    afpuloc_package_has_animation_t afpuloc_package_has_animation = nullptr;
    afpu_package_control_t afpu_package_control = nullptr;
    afpuloc_get_package_info_t afpuloc_get_package_info = nullptr;
    afpuloc_get_version_string_t afpuloc_get_version_string = nullptr;

    afpuloc_get_texture_info_by_id_t afpuloc_get_texture_info_by_id = nullptr;
    afpuloc_get_texture_data_size_t afpuloc_get_texture_data_size = nullptr;
    afpu_texture_get_bpp_t afpu_texture_get_bpp = nullptr;

    afpu_image_find_t afpu_image_find = nullptr;
    afpu_image_get_info_t afpu_image_get_info = nullptr;

    afpu_render_init_t afpu_render_init = nullptr;
    afpu_render_reset_t afpu_render_reset = nullptr;
    afpu_render_info_t afpu_render_info = nullptr;
    afpu_render_flush_t afpu_render_flush = nullptr;

    afpu_package_dump_t afpu_package_dump = nullptr;

    afpu_afp_get_info_in_package_t afpu_afp_get_info_in_package = nullptr;
    afpu_image_lookup_t afpu_image_lookup = nullptr;
    afpu_image_to_stream_args_t afpu_image_to_stream_args = nullptr;

    bool Load(DllLoader& loader) {
        DLL_LOAD(loader, afpu_boot, 0x000);
        DLL_LOAD(loader, afpu_shutdown, 0x001);
        DLL_LOAD(loader, afpu_set_flag, 0x003);
        DLL_LOAD(loader, afpu_set_config, 0x005);
        DLL_LOAD(loader, afpu_get_context, 0x07f);
        DLL_LOAD(loader, afpu_ext_command, 0x07e);
        DLL_LOAD(loader, afpu_ngp_mounttable_load, 0x01a);
        DLL_LOAD(loader, afpu_ngp_mounttable_load_from_property, 0x01b);
        DLL_LOAD(loader, afpu_ngp_packages_exist, 0x01c);
        DLL_LOAD(loader, afpu_ngp_read_local, 0x022);
        DLL_LOAD(loader, afpu_ngp_read_by_mounttable, 0x023);
        DLL_LOAD(loader, afpu_ngp_detect_format, 0x026);
        DLL_LOAD(loader, afpu_package_get_count, 0x01e);
        DLL_LOAD(loader, afpu_package_get_name_by_index, 0x01f);
        DLL_LOAD(loader, afpu_package_find_by_name, 0x020);
        DLL_LOAD(loader, afpu_package_open_streams, 0x03a);
        DLL_LOAD(loader, afpuloc_get_first_package_id, 0x038);
        DLL_LOAD(loader, afpuloc_get_package_id, 0x037);
        DLL_LOAD(loader, afpu_get_loaded_package_count, 0x034);
        DLL_LOAD(loader, afpuloc_package_has_animation, 0x03b);
        DLL_LOAD(loader, afpu_package_control, 0x036);
        DLL_LOAD(loader, afpuloc_get_package_info, 0x03f);
        DLL_LOAD(loader, afpuloc_get_version_string, 0x03e);
        DLL_LOAD(loader, afpuloc_get_texture_info_by_id, 0x040);
        DLL_LOAD(loader, afpuloc_get_texture_data_size, 0x042);
        DLL_LOAD(loader, afpu_afp_get_info_in_package, 0x062);
        DLL_LOAD(loader, afpu_image_lookup, 0x046);
        DLL_LOAD(loader, afpu_image_to_stream_args, 0x04c);
        DLL_LOAD(loader, afpu_texture_get_bpp, 0x079);
        DLL_LOAD(loader, afpu_image_find, 0x043);
        DLL_LOAD(loader, afpu_image_get_info, 0x045);
        DLL_LOAD(loader, afpu_render_init, 0x070);
        DLL_LOAD(loader, afpu_render_reset, 0x071);
        DLL_LOAD(loader, afpu_render_info, 0x072);
        DLL_LOAD(loader, afpu_render_flush, 0x074);
        DLL_LOAD(loader, afpu_package_dump, 0x068);

        return afpu_boot && afpu_shutdown && afpu_render_init && afpu_ngp_read_local &&
               afpu_package_open_streams;
    }
};
