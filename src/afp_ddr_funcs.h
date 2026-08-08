#pragma once

#include "support/dll_loader.h"
#include <cstdint>

#define DDR_LOAD(loader, field) field = (loader).GetFunc<decltype(field)>(0, #field)

typedef int (*ddr_afp_boot_t)(void* render_params);
typedef int (*ddr_afp_boot_with_heap_t)(void* heap, int heap_size, void* render_params);
typedef void (*ddr_afp_shutdown_t)();
typedef void (*ddr_afp_set_stream_max_nr_t)(int max_streams);
typedef void (*ddr_afp_set_policy_t)(int policy);
typedef void (*ddr_afp_system_set_attribute_t)(int mask, int value);
typedef int (*ddr_afp_render_init_t)();
typedef void (*ddr_afp_render_finish_t)();
typedef void (*ddr_afp_render_destroy_t)();
typedef void (*ddr_afp_do_render_t)(float dt, int type, unsigned int id);
typedef int (*ddr_afp_do_display_t)(int type, unsigned int id);
typedef int (*ddr_afp_set_create_level_t)(int level);
typedef int (*ddr_afp_get_create_level_t)();
typedef int (*ddr_afp_ext_command_t)(int cmd, void* arg);
typedef int (*ddr_afp_get_data_id_by_name_t)(const char* name);
typedef uint32_t (*ddr_afp_stream_do_create_t)(int data_id, void* a2, void* a3);
typedef uint32_t (*ddr_afp_layer_create_with_property_t)(uint32_t stream_id, const char* path,
                                                         intptr_t a3, const void* a4);
typedef int (*ddr_afp_id_is_valid_t)(int type, uint32_t id);
typedef void (*ddr_afp_layer_set_priority_t)(uint32_t layer_id, int priority);
typedef int (*ddr_afp_layer_set_attribute_t)(uint32_t layer_id, int mask, int value);
typedef int (*ddr_afp_layer_mc_refer_t)(uint32_t layer_id, const char* path);
typedef int (*ddr_afp_mc_get_param_t)(uint32_t mc_id, uint32_t code, ...);
typedef int (*ddr_afp_mc_op_t)(uint32_t mc_id, int op, ...);
typedef int (*ddr_afp_mc_op_frame_t)(uint32_t mc_id, int op, int frame);
typedef int (*ddr_afp_layer_stop_t)(uint32_t layer_id);
typedef int (*ddr_afp_layer_play_t)(uint32_t layer_id, float rate);
typedef int (*ddr_afp_layer_get_info_t)(uint32_t layer_id, void* out_info, intptr_t a3);
typedef int (*ddr_afp_stream_get_info_t)(uint32_t stream_id, void* out_info);

typedef void (*ddr_afp_render_all_t)(float dt);
typedef int (*ddr_afp_display_all_t)();
typedef int (*ddr_afp_display_layer_t)(uint32_t layer_id);
typedef int (*ddr_afp_layer_is_valid_t)(uint32_t layer_id);
typedef int (*ddr_afp_layer_destroy_t)(uint32_t layer_id);
typedef int (*ddr_afp_stream_destroy_call_t)(uint32_t stream_id);
typedef const char* (*ddr_afp_get_version_t)();

typedef uint32_t (*ddr_afp_stream_create_call_t)(const void* afp_data);
typedef int (*ddr_afp_stream_set_name_call_t)(uint32_t stream_id, const char* name);
typedef uint32_t (*ddr_afp_layer_create_t)(uint32_t stream_id, const char* layer_name);
typedef int (*ddr_afp_check_src_t)(void* afp_data, const void* byteorder_info);

struct AfpDdrFuncs {
    ddr_afp_boot_t afp_boot = nullptr;
    ddr_afp_shutdown_t afp_shutdown = nullptr;
    ddr_afp_set_stream_max_nr_t afp_set_stream_max_nr = nullptr;
    ddr_afp_set_policy_t afp_set_policy = nullptr;
    ddr_afp_system_set_attribute_t afp_system_set_attribute = nullptr;
    ddr_afp_render_init_t afp_render_init = nullptr;
    ddr_afp_render_finish_t afp_render_finish = nullptr;
    ddr_afp_render_destroy_t afp_render_destroy = nullptr;
    ddr_afp_do_render_t afp_do_render = nullptr;
    ddr_afp_do_display_t afp_do_display = nullptr;
    ddr_afp_set_create_level_t afp_set_create_level = nullptr;
    ddr_afp_get_create_level_t afp_get_create_level = nullptr;
    ddr_afp_ext_command_t afp_ext_command = nullptr;
    ddr_afp_get_data_id_by_name_t afp_get_data_id_by_name = nullptr;
    ddr_afp_stream_do_create_t afp_stream_do_create = nullptr;
    ddr_afp_layer_create_with_property_t afp_layer_create_with_property = nullptr;
    ddr_afp_id_is_valid_t afp_id_is_valid = nullptr;
    ddr_afp_layer_set_priority_t afp_layer_set_priority = nullptr;
    ddr_afp_layer_set_attribute_t afp_layer_set_attribute = nullptr;
    ddr_afp_layer_mc_refer_t afp_layer_mc_refer = nullptr;
    ddr_afp_mc_get_param_t afp_mc_get_param = nullptr;
    ddr_afp_mc_op_t afp_mc_op = nullptr;
    ddr_afp_mc_op_frame_t afp_mc_op_frame = nullptr;
    ddr_afp_layer_stop_t afp_layer_stop = nullptr;
    ddr_afp_layer_play_t afp_layer_play = nullptr;
    ddr_afp_layer_get_info_t afp_layer_get_info = nullptr;
    ddr_afp_stream_get_info_t afp_stream_get_info = nullptr;

    ddr_afp_render_all_t afp_render_all = nullptr;
    ddr_afp_display_all_t afp_display_all = nullptr;
    ddr_afp_display_layer_t afp_display_layer = nullptr;
    ddr_afp_layer_is_valid_t afp_layer_is_valid = nullptr;
    ddr_afp_layer_destroy_t afp_layer_destroy = nullptr;
    ddr_afp_stream_destroy_call_t afp_stream_destroy_call = nullptr;
    ddr_afp_get_version_t afp_get_version = nullptr;

    ddr_afp_stream_create_call_t afp_stream_create_call = nullptr;
    ddr_afp_stream_set_name_call_t afp_stream_set_name_call = nullptr;
    ddr_afp_layer_create_t afp_layer_create = nullptr;
    ddr_afp_check_src_t afp_check_src = nullptr;

    [[nodiscard]] bool HasSplitRenderApi() const {
        return afp_do_render == nullptr && afp_render_all != nullptr;
    }

    [[nodiscard]] int BootEngine(void* heap, int heap_size, void* render_params) const {
        if (HasSplitRenderApi()) {
            auto boot_with_heap = reinterpret_cast<ddr_afp_boot_with_heap_t>(afp_boot);
            return boot_with_heap(heap, heap_size, render_params);
        }
        return afp_boot(render_params);
    }

    void RenderAll(float dt, int mode) const {
        if (afp_do_render != nullptr) {
            afp_do_render(dt, mode, 0);
        } else if (afp_render_all != nullptr) {
            afp_render_all(dt);
        }
    }

    void DisplayLayer(uint32_t layer_id) const {
        if (afp_do_display != nullptr) {
            afp_do_display(5, layer_id);
        } else if (afp_display_layer != nullptr) {
            afp_display_layer(layer_id);
        }
    }

    [[nodiscard]] bool HasTxp2PackageApi() const {
        return afp_stream_create_call != nullptr && afp_layer_create != nullptr;
    }

    [[nodiscard]] bool HasLayerCreate() const {
        return afp_layer_create_with_property != nullptr || afp_layer_create != nullptr;
    }

    [[nodiscard]] uint32_t LayerCreate(uint32_t stream_id, const char* name) const {
        if (afp_layer_create_with_property != nullptr) {
            return afp_layer_create_with_property(stream_id, name, 0, nullptr);
        }
        if (afp_layer_create != nullptr) return afp_layer_create(stream_id, name);
        return 0;
    }

    [[nodiscard]] bool LayerValid(uint32_t layer_id) const {
        if (afp_id_is_valid != nullptr) return afp_id_is_valid(5, layer_id) >= 0;
        if (afp_layer_is_valid != nullptr) return afp_layer_is_valid(layer_id) >= 0;
        return true;
    }

    bool Load(DllLoader& loader) {
        DDR_LOAD(loader, afp_boot);
        DDR_LOAD(loader, afp_shutdown);
        DDR_LOAD(loader, afp_set_stream_max_nr);
        DDR_LOAD(loader, afp_set_policy);
        DDR_LOAD(loader, afp_system_set_attribute);
        DDR_LOAD(loader, afp_render_init);
        DDR_LOAD(loader, afp_render_finish);
        DDR_LOAD(loader, afp_render_destroy);
        DDR_LOAD(loader, afp_do_render);
        DDR_LOAD(loader, afp_do_display);
        DDR_LOAD(loader, afp_set_create_level);
        DDR_LOAD(loader, afp_get_create_level);
        DDR_LOAD(loader, afp_ext_command);
        DDR_LOAD(loader, afp_get_data_id_by_name);
        DDR_LOAD(loader, afp_stream_do_create);
        DDR_LOAD(loader, afp_layer_create_with_property);
        DDR_LOAD(loader, afp_id_is_valid);
        DDR_LOAD(loader, afp_layer_set_priority);
        DDR_LOAD(loader, afp_layer_set_attribute);
        DDR_LOAD(loader, afp_layer_mc_refer);
        DDR_LOAD(loader, afp_mc_get_param);
        DDR_LOAD(loader, afp_mc_op);
        afp_mc_op_frame = reinterpret_cast<ddr_afp_mc_op_frame_t>(afp_mc_op);
        DDR_LOAD(loader, afp_layer_stop);
        DDR_LOAD(loader, afp_layer_play);
        DDR_LOAD(loader, afp_layer_get_info);
        DDR_LOAD(loader, afp_stream_get_info);

        DDR_LOAD(loader, afp_render_all);
        DDR_LOAD(loader, afp_display_all);
        DDR_LOAD(loader, afp_display_layer);
        DDR_LOAD(loader, afp_layer_is_valid);
        DDR_LOAD(loader, afp_layer_destroy);
        DDR_LOAD(loader, afp_stream_destroy_call);
        DDR_LOAD(loader, afp_get_version);
        DDR_LOAD(loader, afp_stream_create_call);
        DDR_LOAD(loader, afp_stream_set_name_call);
        DDR_LOAD(loader, afp_layer_create);
        DDR_LOAD(loader, afp_check_src);

        const bool can_render = (afp_do_render != nullptr) || (afp_render_all != nullptr);
        const bool can_display = (afp_do_display != nullptr) || (afp_display_layer != nullptr);
        return (afp_boot != nullptr) && can_render && can_display;
    }
};

typedef int (*ddr_afpu_boot_t)(void* config, void* afpu_cfg_callbacks);
typedef int (*ddr_afpu_boot_with_heap_t)(void* config, void* heap, int heap_size);
typedef void (*ddr_afpu_shutdown_t)();
typedef int (*ddr_afpu_set_afp_render_params_t)(void* render_params);
typedef int (*ddr_afpu_set_render_params_t)(void* afpu_cfg_callbacks);
typedef int (*ddr_afpu_system_set_parameter_t)(int key, int value);
typedef int (*ddr_afpu_system_set_attributes_t)(int mask, int value);
typedef int (*ddr_afpu_set_config_t)(int type, int value);
typedef int (*ddr_afpu_ext_command_t)(int cmd, void* arg);
typedef int (*ddr_afpu_do_create_stream_all_t)(void* a1, void* a2);
typedef int (*ddr_afpu_ngp_read_data_t)(const char* name, const char* path, int flags);
typedef int (*ddr_afpu_destroy_package_data_t)(uint32_t pkg_id);
typedef uint32_t (*ddr_afpu_get_afp_id_t)(const char* name);
struct DdrAfpInfo {
    uint32_t field0;
    uint32_t field1;
    void* reserved;
    const char* name;
    uint32_t stream_id;
};

typedef int (*ddr_afpu_get_afp_info_at_package_t)(DdrAfpInfo* out_info, uint32_t data_id,
                                                  const char* clip_name);
typedef int (*ddr_afpu_get_afp_info_from_index_at_package_t)(DdrAfpInfo* out_info, uint32_t data_id,
                                                             int index);
typedef int (*ddr_afpu_get_texture_bind_id_t)(unsigned int afp_tex_id);

typedef int (*ddr_afpu_ngp_read_t)(const char* name, const char* path);
typedef int (*ddr_afpu_create_stream_all_t)(int package_id);

struct AfpuDdrFuncs {
    ddr_afpu_boot_t afpu_boot = nullptr;
    ddr_afpu_shutdown_t afpu_shutdown = nullptr;
    ddr_afpu_set_afp_render_params_t afpu_set_afp_render_params = nullptr;
    ddr_afpu_set_render_params_t afpu_set_render_params = nullptr;
    ddr_afpu_system_set_parameter_t afpu_system_set_parameter = nullptr;
    ddr_afpu_system_set_attributes_t afpu_system_set_attributes = nullptr;
    ddr_afpu_set_config_t afpu_set_config = nullptr;
    ddr_afpu_ext_command_t afpu_ext_command = nullptr;
    ddr_afpu_do_create_stream_all_t afpu_do_create_stream_all = nullptr;
    ddr_afpu_ngp_read_data_t afpu_ngp_read_data = nullptr;
    ddr_afpu_destroy_package_data_t afpu_destroy_package_data = nullptr;
    ddr_afpu_get_afp_id_t afpu_get_afp_id = nullptr;
    ddr_afpu_get_afp_info_at_package_t afpu_get_afp_info_at_package = nullptr;
    ddr_afpu_get_afp_info_from_index_at_package_t afpu_get_afp_info_from_index_at_package = nullptr;
    ddr_afpu_get_texture_bind_id_t afpu_get_texture_bind_id = nullptr;

    ddr_afpu_ngp_read_t afpu_ngp_read = nullptr;
    ddr_afpu_create_stream_all_t afpu_create_stream_all = nullptr;

    [[nodiscard]] bool BootTakesHeap() const {
        return afpu_ngp_read_data == nullptr && afpu_ngp_read != nullptr;
    }

    [[nodiscard]] int BootEngine(void* config, void* afpu_cfg_callbacks, void* heap,
                                 int heap_size) const {
        if (BootTakesHeap()) {
            auto boot_with_heap = reinterpret_cast<ddr_afpu_boot_with_heap_t>(afpu_boot);
            return boot_with_heap(config, heap, heap_size);
        }
        return afpu_boot(config, afpu_cfg_callbacks);
    }

    [[nodiscard]] int ReadPackage(const char* name, const char* path) const {
        if (afpu_ngp_read_data != nullptr) return afpu_ngp_read_data(name, path, 0);
        if (afpu_ngp_read != nullptr) return afpu_ngp_read(name, path);
        return -1;
    }

    [[nodiscard]] int CreateStreamsForPackage(int package_id) const {
        if (afpu_do_create_stream_all != nullptr) {
            return afpu_do_create_stream_all(
                reinterpret_cast<void*>(static_cast<intptr_t>(package_id)),
                reinterpret_cast<void*>(static_cast<intptr_t>(1)));
        }
        if (afpu_create_stream_all != nullptr) return afpu_create_stream_all(package_id);
        return -1;
    }

    bool Load(DllLoader& loader) {
        DDR_LOAD(loader, afpu_boot);
        DDR_LOAD(loader, afpu_shutdown);
        DDR_LOAD(loader, afpu_set_afp_render_params);
        DDR_LOAD(loader, afpu_set_render_params);
        DDR_LOAD(loader, afpu_system_set_parameter);
        DDR_LOAD(loader, afpu_system_set_attributes);
        DDR_LOAD(loader, afpu_set_config);
        DDR_LOAD(loader, afpu_ext_command);
        DDR_LOAD(loader, afpu_do_create_stream_all);
        DDR_LOAD(loader, afpu_ngp_read_data);
        DDR_LOAD(loader, afpu_destroy_package_data);
        DDR_LOAD(loader, afpu_get_afp_id);
        DDR_LOAD(loader, afpu_get_afp_info_at_package);
        DDR_LOAD(loader, afpu_get_afp_info_from_index_at_package);
        DDR_LOAD(loader, afpu_get_texture_bind_id);
        DDR_LOAD(loader, afpu_ngp_read);
        DDR_LOAD(loader, afpu_create_stream_all);
        return afpu_boot && afpu_set_afp_render_params && afpu_set_render_params;
    }
};
