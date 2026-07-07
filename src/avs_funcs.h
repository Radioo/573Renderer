#pragma once

#include "support/dll_loader.h"
#include <cstdint>

typedef void* T_PROPERTY;
typedef void* T_PROPERTY_NODE;

typedef void (*avs_boot_t)(void* config_node, void* heap_buffer, int heap_size, void* log_callback,
                           void* log_userdata, void* extra);
typedef void (*avs_shutdown_t)();
typedef int (*avs_is_active_t)();

typedef void* (*avs_filesys_imagefs_t)();
typedef int (*avs_fs_addfs_t)(void* filesys);
typedef int (*avs_fs_mount_t)(const char* mountpoint, const char* fsroot, const char* fstype,
                              const char* options);
typedef int (*avs_fs_umount_t)(const char* mountpoint);
typedef int (*avs_fs_open_t)(const char* path, int flags, int mode);
typedef int (*avs_fs_read_t)(int desc, void* buf, int size);
typedef int (*avs_fs_lseek_t)(int desc, int offset, int whence);
typedef void (*avs_fs_close_t)(int desc);
typedef int (*avs_fs_fstat_t)(int desc, void* stat_buf);
typedef int (*avs_fs_opendir_t)(const char* path);
typedef const char* (*avs_fs_readdir_t)(int desc);
typedef void (*avs_fs_closedir_t)(int desc);
typedef void (*avs_fs_dump_mountpoint_t)();

typedef void* (*avs_gheap_allocate_t)(int flags, size_t size, int tag);
typedef void (*avs_gheap_free_t)(void* ptr);

typedef int (*avs_reader_fn)(int desc, void* buf, int size);

typedef T_PROPERTY* (*property_create_t)(int flags, void* buf, unsigned int size);
typedef void (*property_destroy_t)(T_PROPERTY* prop);
typedef T_PROPERTY_NODE* (*property_search_t)(T_PROPERTY* prop, T_PROPERTY_NODE* node,
                                              const char* path);
typedef T_PROPERTY_NODE* (*property_node_create_t)(T_PROPERTY* prop, T_PROPERTY_NODE* parent,
                                                   int type, const char* name, ...);
typedef int (*property_node_refer_t)(T_PROPERTY* prop, T_PROPERTY_NODE* node, const char* path,
                                     int type, void* data, unsigned int size);
typedef int (*property_mem_write_t)(T_PROPERTY* prop, void* buf, int size);
typedef int (*property_insert_read_t)(T_PROPERTY* prop, T_PROPERTY_NODE* node, avs_reader_fn reader,
                                      int ctx);
typedef int (*property_psmap_import_t)(T_PROPERTY* prop, T_PROPERTY_NODE* node, void* psmap);
typedef int (*property_psmap_export_t)(T_PROPERTY* prop, T_PROPERTY_NODE* node, void* psmap);

typedef int (*property_read_query_memsize_t)(avs_reader_fn reader, int ctx,
                                             unsigned int* out_node_count, int* out_reserved);
typedef int (*property_read_query_memsize_long_t)(avs_reader_fn reader, int ctx,
                                                  unsigned int* out_node_count, int* out_reserved,
                                                  void* extra_40_bytes);

typedef T_PROPERTY_NODE* (*property_node_traversal_t)(T_PROPERTY_NODE* node, int direction);
typedef int (*property_node_name_t)(T_PROPERTY_NODE* node, char* out_buf, unsigned int buf_size);

typedef void (*log_boot_t)(void* config);
typedef void (*log_body_info_t)(const char* tag, const char* fmt, ...);
typedef void (*log_body_warning_t)(const char* tag, const char* fmt, ...);
typedef void (*log_body_misc_t)(const char* tag, const char* fmt, ...);

typedef void (*std_setenv_t)(const char* key, const char* value);

struct AvsFuncs {
    avs_boot_t avs_boot = nullptr;
    avs_shutdown_t avs_shutdown = nullptr;
    avs_is_active_t avs_is_active = nullptr;

    avs_filesys_imagefs_t avs_filesys_imagefs = nullptr;
    avs_fs_addfs_t avs_fs_addfs = nullptr;

    avs_fs_mount_t avs_fs_mount = nullptr;
    avs_fs_umount_t avs_fs_umount = nullptr;
    avs_fs_open_t avs_fs_open = nullptr;
    avs_fs_read_t avs_fs_read = nullptr;
    avs_fs_lseek_t avs_fs_lseek = nullptr;
    avs_fs_close_t avs_fs_close = nullptr;
    avs_fs_fstat_t avs_fs_fstat = nullptr;
    avs_fs_opendir_t avs_fs_opendir = nullptr;
    avs_fs_readdir_t avs_fs_readdir = nullptr;
    avs_fs_closedir_t avs_fs_closedir = nullptr;
    avs_fs_dump_mountpoint_t avs_fs_dump_mountpoint = nullptr;

    avs_gheap_allocate_t avs_gheap_allocate = nullptr;
    avs_gheap_free_t avs_gheap_free = nullptr;

    property_create_t property_create = nullptr;
    property_destroy_t property_destroy = nullptr;
    property_search_t property_search = nullptr;
    property_node_create_t property_node_create = nullptr;
    property_node_refer_t property_node_refer = nullptr;
    property_psmap_import_t property_psmap_import = nullptr;
    property_read_query_memsize_t property_read_query_memsize = nullptr;
    property_read_query_memsize_long_t property_read_query_memsize_long = nullptr;
    property_insert_read_t property_insert_read = nullptr;
    property_node_traversal_t property_node_traversal = nullptr;
    property_node_name_t property_node_name = nullptr;

    log_boot_t log_boot = nullptr;
    log_body_info_t log_body_info = nullptr;
    log_body_warning_t log_body_warning = nullptr;
    log_body_misc_t log_body_misc = nullptr;

    bool Load(DllLoader& loader) {
        DLL_LOAD(loader, avs_boot, 0x129);
        DLL_LOAD(loader, avs_shutdown, 0x12a);
        DLL_LOAD(loader, avs_is_active, 0x12d);
        DLL_LOAD(loader, avs_filesys_imagefs, 0x158);
        DLL_LOAD(loader, avs_fs_addfs, 0x048);
        DLL_LOAD(loader, avs_fs_mount, 0x04b);
        DLL_LOAD(loader, avs_fs_umount, 0x04c);
        DLL_LOAD(loader, avs_fs_open, 0x04e);
        DLL_LOAD(loader, avs_fs_read, 0x051);
        DLL_LOAD(loader, avs_fs_lseek, 0x04f);
        DLL_LOAD(loader, avs_fs_close, 0x055);
        DLL_LOAD(loader, avs_fs_fstat, 0x062);
        DLL_LOAD(loader, avs_fs_opendir, 0x05c);
        DLL_LOAD(loader, avs_fs_readdir, 0x05d);
        DLL_LOAD(loader, avs_fs_closedir, 0x05e);
        DLL_LOAD(loader, avs_fs_dump_mountpoint, 0x068);
        DLL_LOAD(loader, avs_gheap_allocate, 0x02f);
        DLL_LOAD(loader, avs_gheap_free, 0x031);
        DLL_LOAD(loader, property_create, 0x090);
        DLL_LOAD(loader, property_destroy, 0x091);
        DLL_LOAD(loader, property_search, 0x0a1);
        DLL_LOAD(loader, property_node_create, 0x0a2);
        DLL_LOAD(loader, property_node_refer, 0x0af);
        DLL_LOAD(loader, property_psmap_import, 0x0b2);
        DLL_LOAD(loader, property_insert_read, 0x094);
        DLL_LOAD(loader, property_node_traversal, 0x0a6);
        DLL_LOAD(loader, property_node_name, 0x0a7);
        DLL_LOAD(loader, property_read_query_memsize, 0x0b0);
        DLL_LOAD(loader, property_read_query_memsize_long, 0x0b1);
        DLL_LOAD(loader, log_boot, 0x170);
        DLL_LOAD(loader, log_body_info, 0x17c);
        DLL_LOAD(loader, log_body_warning, 0x17b);
        DLL_LOAD(loader, log_body_misc, 0x17d);

        return avs_boot && avs_shutdown && avs_fs_mount && avs_fs_addfs && avs_filesys_imagefs &&
               property_create;
    }
};
