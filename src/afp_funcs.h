#pragma once

#include "support/dll_loader.h"
#include <cstdint>

typedef int (*afp_set_afp_data_t)(void* data, uint64_t a2, uint64_t a3, void* ctx);
typedef void* (*afp_get_afp_data_t)();
typedef int (*afp_boot_t)(void* render_context);
typedef void (*afp_shutdown_t)();
typedef int (*afp_set_flag_t)(uint32_t flags, uint32_t mask);
typedef uint32_t (*afp_get_flag_t)();
typedef void (*afp_set_verbose_t)(uint8_t verbose, uint32_t flags);
typedef void (*afp_set_global_speed_t)(float speed);

typedef void (*afp_set_bg_color_t)(uint32_t stream_id, uint32_t rgb);
typedef void (*afp_set_priority_offset_t)(uint32_t stream_id, int offset);
typedef void (*afp_do_render_t)();
typedef void (*afp_do_update_t)(float delta_time, int type, int flags);
typedef int (*afp_render_init_t)();
typedef void (*afp_render_destroy_t)();
typedef void (*afp_do_sort_render_t)(int type, unsigned int filter);

typedef uint32_t (*afp_stream_create_t)();
typedef void (*afp_stream_set_data_t)(uint32_t stream_id, void* afp_data);
typedef void* (*afp_stream_get_work_t)(uint32_t stream_id);
typedef void (*afp_stream_set_complete_t)(uint32_t stream_id);
typedef void (*afp_set_create_level_t)(int level);
typedef int (*afp_get_create_level_t)();
typedef void (*afp_set_stream_nr_t)(int max_streams);
typedef int (*afp_stream_play_t)(uint32_t data_id, const char* data_ptr, int unk1, int unk2);
typedef int (*afp_stream_control_t)(int cmd, uint32_t stream_id);
typedef const char* (*afp_stream_get_name_t)(uint32_t stream_id);
typedef int (*afp_stream_destroy_t)(int type, uint32_t selector, int schedule);
typedef void (*afp_stream_set_speed_t)(uint32_t stream_id, float speed);
typedef void (*afp_stream_restart_t)(uint32_t stream_id);

typedef void (*afp_stream_set_matrix_t)(uint32_t stream_id, float* mat2d);
typedef void (*afp_stream_get_matrix_t)(uint32_t stream_id, float* mat2d);
typedef void (*afp_stream_set_translate_t)(uint32_t stream_id, float x, float y);

typedef int (*afp_data_get_info_t)(void* data, void* info_out);
typedef int (*afp_data_get_stream_info_t)(void* data, int index, void* info_out);
typedef int (*afp_get_data_id_by_name_t)(const char* name);

typedef int (*afp_ext_command_t)(int cmd, void* arg);

typedef int (*afp_get_layers_by_nr_t)(int category, uint32_t* out_buf, int max_out);
typedef int (*afp_get_layer_info_t)(uint32_t stream_id, void* info_buf);

typedef int (*afp_stream_play_bitmap_by_name_t)(const char* name, int unk);
typedef uint32_t (*afp_image_stream_create_t)(const void* stream_args_40, uint32_t opt);
typedef int (*afp_set_filter_t)(int layer_id, const void* filt, unsigned size);
typedef int (*afp_system_dump_layer_info_t)(int min_priority);

typedef int (*afp_mc_get_id_by_path_t)(uint32_t stream_id, const char* path);
typedef int (*afp_mc_get_relative_id_t)(int mc_id, int direction);
typedef int64_t (*afp_mc_attach_stream_t)(uint32_t mc_id, uint32_t data_id);
typedef int (*afp_mc_get_t)(int mc_id, uint32_t prop_id, intptr_t arg);
typedef int (*afp_mc_set_t)(int mc_id, uint32_t code, ...);
typedef int (*afp_play_work_load_bitmap_t)(int mc_id, const char* bitmap_name, int attach);
typedef int (*afp_play_work_load_image_t)(int mc_id, const void* image_info, int attach);
typedef int (*afp_mc_control_t)(int mc_id, uint32_t op, const char* label);
typedef int (*afp_mc_control_frame_t)(int mc_id, uint32_t op, int frame);
typedef int (*afp_mc_enumerate_children_t)(uint32_t mc_id, void* buf, int buf_size, int flags,
                                           void** out_ptr);
typedef int (*afp_set_flag_mask_t)(uint32_t stream_id, uint32_t flags, uint32_t mask);

struct AfpFuncs {
    afp_set_afp_data_t afp_set_afp_data = nullptr;
    afp_get_afp_data_t afp_get_afp_data = nullptr;
    afp_boot_t afp_boot = nullptr;
    afp_shutdown_t afp_shutdown = nullptr;
    afp_set_flag_t afp_set_flag = nullptr;
    afp_set_verbose_t afp_set_verbose = nullptr;
    afp_set_global_speed_t afp_set_global_speed = nullptr;

    afp_set_bg_color_t afp_set_bg_color = nullptr;
    afp_do_render_t afp_do_render = nullptr;
    afp_do_update_t afp_do_update = nullptr;
    afp_render_init_t afp_render_init = nullptr;
    afp_render_destroy_t afp_render_destroy = nullptr;
    afp_do_sort_render_t afp_do_sort_render = nullptr;

    afp_set_create_level_t afp_set_create_level = nullptr;
    afp_get_create_level_t afp_get_create_level = nullptr;
    afp_stream_create_t afp_stream_create = nullptr;
    afp_stream_set_data_t afp_stream_set_data = nullptr;
    afp_stream_get_work_t afp_stream_get_work = nullptr;
    afp_set_stream_nr_t afp_set_stream_nr = nullptr;
    afp_stream_play_t afp_stream_play = nullptr;
    afp_stream_control_t afp_stream_control = nullptr;
    afp_stream_get_name_t afp_stream_get_name = nullptr;
    afp_stream_destroy_t afp_stream_destroy = nullptr;
    afp_stream_set_speed_t afp_stream_set_speed = nullptr;

    afp_stream_set_matrix_t afp_stream_set_matrix = nullptr;
    afp_stream_get_matrix_t afp_stream_get_matrix = nullptr;
    afp_stream_set_translate_t afp_stream_set_translate = nullptr;

    afp_data_get_info_t afp_data_get_info = nullptr;
    afp_data_get_stream_info_t afp_data_get_stream_info = nullptr;
    afp_get_data_id_by_name_t afp_get_data_id_by_name = nullptr;

    afp_ext_command_t afp_ext_command = nullptr;

    afp_mc_get_id_by_path_t afp_mc_get_id_by_path = nullptr;
    afp_mc_get_relative_id_t afp_mc_get_relative_id = nullptr;
    afp_mc_attach_stream_t afp_mc_attach_stream = nullptr;
    afp_mc_get_t afp_mc_get = nullptr;
    afp_mc_set_t afp_mc_set = nullptr;
    afp_mc_control_t afp_mc_control = nullptr;
    afp_mc_control_frame_t afp_mc_control_frame = nullptr;
    afp_mc_enumerate_children_t afp_mc_enumerate_children = nullptr;
    afp_play_work_load_bitmap_t afp_play_work_load_bitmap = nullptr;
    afp_play_work_load_image_t afp_play_work_load_image = nullptr;
    afp_set_flag_mask_t afp_set_flag_mask = nullptr;

    afp_get_layers_by_nr_t afp_get_layers_by_nr = nullptr;
    afp_get_layer_info_t afp_get_layer_info = nullptr;
    afp_system_dump_layer_info_t afp_system_dump_layer_info = nullptr;

    afp_stream_play_bitmap_by_name_t afp_stream_play_bitmap_by_name = nullptr;
    afp_image_stream_create_t afp_image_stream_create = nullptr;
    afp_set_filter_t afp_set_filter = nullptr;

    bool Load(DllLoader& loader) {
        DLL_LOAD(loader, afp_set_afp_data, 0x000);
        DLL_LOAD(loader, afp_get_afp_data, 0x001);
        DLL_LOAD(loader, afp_boot, 0x002);
        DLL_LOAD(loader, afp_shutdown, 0x003);
        DLL_LOAD(loader, afp_set_flag, 0x005);
        DLL_LOAD(loader, afp_set_verbose, 0x008);
        DLL_LOAD(loader, afp_set_global_speed, 0x00a);
        DLL_LOAD(loader, afp_set_bg_color, 0x00b);
        DLL_LOAD(loader, afp_do_render, 0x00d);
        DLL_LOAD(loader, afp_do_update, 0x00e);
        DLL_LOAD(loader, afp_render_init, 0x00f);
        DLL_LOAD(loader, afp_render_destroy, 0x010);
        DLL_LOAD(loader, afp_do_sort_render, 0x011);
        DLL_LOAD(loader, afp_set_create_level, 0x013);
        DLL_LOAD(loader, afp_get_create_level, 0x014);
        DLL_LOAD(loader, afp_stream_create, 0x018);
        DLL_LOAD(loader, afp_stream_set_data, 0x019);
        DLL_LOAD(loader, afp_stream_get_work, 0x01a);
        DLL_LOAD(loader, afp_set_stream_nr, 0x01d);
        DLL_LOAD(loader, afp_stream_play, 0x01e);
        DLL_LOAD(loader, afp_stream_control, 0x015);
        DLL_LOAD(loader, afp_stream_get_name, 0x01f);
        DLL_LOAD(loader, afp_stream_destroy, 0x020);
        DLL_LOAD(loader, afp_stream_set_speed, 0x02a);
        DLL_LOAD(loader, afp_stream_set_matrix, 0x02c);
        DLL_LOAD(loader, afp_stream_get_matrix, 0x02d);
        DLL_LOAD(loader, afp_stream_set_translate, 0x02e);
        DLL_LOAD(loader, afp_data_get_info, 0x044);
        DLL_LOAD(loader, afp_data_get_stream_info, 0x045);
        DLL_LOAD(loader, afp_get_data_id_by_name, 0x047);
        DLL_LOAD(loader, afp_ext_command, 0x086);

        DLL_LOAD(loader, afp_mc_get_id_by_path, 0x066);
        DLL_LOAD(loader, afp_mc_get_relative_id, 0x069);
        DLL_LOAD(loader, afp_mc_control, 0x071);
        afp_mc_control_frame = reinterpret_cast<afp_mc_control_frame_t>(afp_mc_control);
        DLL_LOAD(loader, afp_mc_enumerate_children, 0x079);
        DLL_LOAD(loader, afp_mc_set, 0x072);
        DLL_LOAD(loader, afp_mc_get, 0x073);
        DLL_LOAD(loader, afp_mc_attach_stream, 0x06e);
        DLL_LOAD(loader, afp_play_work_load_bitmap, 0x087);
        DLL_LOAD(loader, afp_play_work_load_image, 0x088);
        DLL_LOAD(loader, afp_set_flag_mask, 0x037);
        DLL_LOAD(loader, afp_stream_play_bitmap_by_name, 0x021);
        DLL_LOAD(loader, afp_image_stream_create, 0x022);
        DLL_LOAD(loader, afp_set_filter, 0x032);

        DLL_LOAD(loader, afp_get_layers_by_nr, 0x04b);
        DLL_LOAD(loader, afp_get_layer_info, 0x046);
        DLL_LOAD(loader, afp_system_dump_layer_info, 0x043);

        return afp_boot && afp_shutdown && afp_render_init && afp_do_update && afp_do_sort_render &&
               afp_stream_create && afp_stream_play;
    }
};
