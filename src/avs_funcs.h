#pragma once

#include "support/dll_loader.h"
#include <cstdint>

typedef void* T_PROPERTY;
typedef void* T_PROPERTY_NODE;

typedef void (*avs_boot_t)(void* config_node, void* heap_buffer, int heap_size, void* log_callback,
                           void* log_userdata, void* extra);
typedef void (*avs_boot_split_heap_t)(void* config_node, void* std_heap, int std_heap_size,
                                      void* avs_heap, int avs_heap_size, void* log_writer,
                                      void* log_userdata);
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

typedef void* (*avs_cstream_create_t)(int type);
typedef int (*avs_cstream_execute_t)(void* ctx);
typedef int (*avs_cstream_finish_t)(void* ctx);
typedef void (*avs_cstream_destroy_t)(void* ctx);

typedef void (*std_setenv_t)(const char* key, const char* value);

struct AvsOrdinals {
    int avs_boot;
    int avs_shutdown;
    int avs_is_active;

    int avs_filesys_imagefs;
    int avs_fs_addfs;
    int avs_fs_mount;
    int avs_fs_umount;
    int avs_fs_open;
    int avs_fs_read;
    int avs_fs_lseek;
    int avs_fs_close;
    int avs_fs_fstat;
    int avs_fs_opendir;
    int avs_fs_readdir;
    int avs_fs_closedir;
    int avs_fs_dump_mountpoint;

    int avs_gheap_allocate;
    int avs_gheap_free;

    int property_create;
    int property_destroy;
    int property_search;
    int property_node_create;
    int property_node_refer;
    int property_psmap_import;
    int property_insert_read;
    int property_node_traversal;
    int property_node_name;
    int property_read_query_memsize;
    int property_read_query_memsize_long;

    int log_boot;
    int log_body_info;
    int log_body_warning;
    int log_body_misc;

    bool boot_takes_split_heaps;
    bool log_writer_ctx_first;
    bool log_level_is_u32;

    int avs_cstream_create;
    int avs_cstream_execute;
    int avs_cstream_finish;
    int avs_cstream_destroy;
};

constexpr AvsOrdinals kAvsOrdinals217 = {
    .avs_boot = 0x129,
    .avs_shutdown = 0x12a,
    .avs_is_active = 0x12d,
    .avs_filesys_imagefs = 0x158,
    .avs_fs_addfs = 0x048,
    .avs_fs_mount = 0x04b,
    .avs_fs_umount = 0x04c,
    .avs_fs_open = 0x04e,
    .avs_fs_read = 0x051,
    .avs_fs_lseek = 0x04f,
    .avs_fs_close = 0x055,
    .avs_fs_fstat = 0x062,
    .avs_fs_opendir = 0x05c,
    .avs_fs_readdir = 0x05d,
    .avs_fs_closedir = 0x05e,
    .avs_fs_dump_mountpoint = 0x068,
    .avs_gheap_allocate = 0x02f,
    .avs_gheap_free = 0x031,
    .property_create = 0x090,
    .property_destroy = 0x091,
    .property_search = 0x0a1,
    .property_node_create = 0x0a2,
    .property_node_refer = 0x0af,
    .property_psmap_import = 0x0b2,
    .property_insert_read = 0x094,
    .property_node_traversal = 0x0a6,
    .property_node_name = 0x0a7,
    .property_read_query_memsize = 0x0b0,
    .property_read_query_memsize_long = 0x0b1,
    .log_boot = 0x170,
    .log_body_info = 0x17c,
    .log_body_warning = 0x17b,
    .log_body_misc = 0x17d,
    .boot_takes_split_heaps = false,
    .log_writer_ctx_first = false,
    .log_level_is_u32 = false,
};

constexpr AvsOrdinals kAvsOrdinals2161 = {
    .avs_boot = 0x11a,
    .avs_shutdown = 0x11b,
    .avs_is_active = 0x11c,
    .avs_filesys_imagefs = 0x14d,
    .avs_fs_addfs = 0x033,
    .avs_fs_mount = 0x036,
    .avs_fs_umount = 0x037,
    .avs_fs_open = 0x039,
    .avs_fs_read = 0x03c,
    .avs_fs_lseek = 0x03a,
    .avs_fs_close = 0x040,
    .avs_fs_fstat = 0x04d,
    .avs_fs_opendir = 0x047,
    .avs_fs_readdir = 0x048,
    .avs_fs_closedir = 0x049,
    .avs_fs_dump_mountpoint = 0x053,
    .avs_gheap_allocate = 0x183,
    .avs_gheap_free = 0x178,
    .property_create = 0x07b,
    .property_destroy = 0x07c,
    .property_search = 0x08c,
    .property_node_create = 0x08d,
    .property_node_refer = 0x09a,
    .property_psmap_import = 0x09d,
    .property_insert_read = 0x07f,
    .property_node_traversal = 0x091,
    .property_node_name = 0x092,
    .property_read_query_memsize = 0x09b,
    .property_read_query_memsize_long = 0x09c,
    .log_boot = 0x15e,
    .log_body_info = 0x16a,
    .log_body_warning = 0x169,
    .log_body_misc = 0x16b,
    .boot_takes_split_heaps = false,
    .log_writer_ctx_first = false,
    .log_level_is_u32 = false,
};

constexpr AvsOrdinals kAvsOrdinals2158 = {
    .avs_boot = 0x0aa,
    .avs_shutdown = 0x01d,
    .avs_is_active = 0x012,
    .avs_filesys_imagefs = 0x095,
    .avs_fs_addfs = 0x12d,
    .avs_fs_mount = 0x0ce,
    .avs_fs_umount = 0x0a2,
    .avs_fs_open = 0x090,
    .avs_fs_read = 0x10d,
    .avs_fs_lseek = 0x04d,
    .avs_fs_close = 0x11f,
    .avs_fs_fstat = 0x0c3,
    .avs_fs_opendir = 0x0f0,
    .avs_fs_readdir = 0x0bb,
    .avs_fs_closedir = 0x0b8,
    .avs_fs_dump_mountpoint = 0x0e9,
    .avs_gheap_allocate = 0x169,
    .avs_gheap_free = 0x16a,
    .property_create = 0x126,
    .property_destroy = 0x13c,
    .property_search = 0x12e,
    .property_node_create = 0x02c,
    .property_node_refer = 0x009,
    .property_psmap_import = 0x005,
    .property_insert_read = 0x09a,
    .property_node_traversal = 0x046,
    .property_node_name = 0x049,
    .property_read_query_memsize = 0x0ff,
    .property_read_query_memsize_long = 0x02b,
    .log_boot = 0x04e,
    .log_body_info = 0x0dc,
    .log_body_warning = 0x018,
    .log_body_misc = 0x075,
    .boot_takes_split_heaps = true,
    .log_writer_ctx_first = false,
    .log_level_is_u32 = false,
};

constexpr AvsOrdinals kAvsOrdinals2134 = {
    .avs_boot = 0x0f4,
    .avs_shutdown = 0x154,
    .avs_is_active = 0x07d,
    .avs_filesys_imagefs = 0x05a,
    .avs_fs_addfs = 0x11f,
    .avs_fs_mount = 0x09c,
    .avs_fs_umount = 0x06e,
    .avs_fs_open = 0x0b6,
    .avs_fs_read = 0x139,
    .avs_fs_lseek = 0x00f,
    .avs_fs_close = 0x11b,
    .avs_fs_fstat = 0x0d0,
    .avs_fs_opendir = 0x0dd,
    .avs_fs_readdir = 0x086,
    .avs_fs_closedir = 0x087,
    .avs_fs_dump_mountpoint = 0x0c8,
    .avs_gheap_allocate = 0x155,
    .avs_gheap_free = 0x0ba,
    .property_create = 0x107,
    .property_destroy = 0x10f,
    .property_search = 0x0fb,
    .property_node_create = 0x143,
    .property_node_refer = 0x113,
    .property_psmap_import = 0x068,
    .property_insert_read = 0x016,
    .property_node_traversal = 0x05f,
    .property_node_name = 0x106,
    .property_read_query_memsize = 0x066,
    .property_read_query_memsize_long = 0x091,
    .log_boot = 0x14c,
    .log_body_info = 0x15a,
    .log_body_warning = 0x0e1,
    .log_body_misc = 0x02d,
    .boot_takes_split_heaps = true,
    .log_writer_ctx_first = true,
    .log_level_is_u32 = true,
    .avs_cstream_create = 0x118,
    .avs_cstream_execute = 0x078,
    .avs_cstream_finish = 0x130,
    .avs_cstream_destroy = 0x12b,
};

struct AvsFuncs {
    avs_boot_t avs_boot = nullptr;
    avs_boot_split_heap_t avs_boot_split_heap = nullptr;
    avs_cstream_create_t avs_cstream_create = nullptr;
    avs_cstream_execute_t avs_cstream_execute = nullptr;
    avs_cstream_finish_t avs_cstream_finish = nullptr;
    avs_cstream_destroy_t avs_cstream_destroy = nullptr;
    bool boot_takes_split_heaps = false;
    bool log_writer_ctx_first = false;
    bool log_level_is_u32 = false;
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

    bool Load(DllLoader& loader, const AvsOrdinals& ord = kAvsOrdinals217) {
        DLL_LOAD(loader, avs_boot, ord.avs_boot);
        avs_boot_split_heap = reinterpret_cast<avs_boot_split_heap_t>(avs_boot);
        boot_takes_split_heaps = ord.boot_takes_split_heaps;
        log_writer_ctx_first = ord.log_writer_ctx_first;
        log_level_is_u32 = ord.log_level_is_u32;
        if (ord.avs_cstream_create != 0) {
            DLL_LOAD(loader, avs_cstream_create, ord.avs_cstream_create);
            DLL_LOAD(loader, avs_cstream_execute, ord.avs_cstream_execute);
            DLL_LOAD(loader, avs_cstream_finish, ord.avs_cstream_finish);
            DLL_LOAD(loader, avs_cstream_destroy, ord.avs_cstream_destroy);
        }
        DLL_LOAD(loader, avs_shutdown, ord.avs_shutdown);
        DLL_LOAD(loader, avs_is_active, ord.avs_is_active);
        DLL_LOAD(loader, avs_filesys_imagefs, ord.avs_filesys_imagefs);
        DLL_LOAD(loader, avs_fs_addfs, ord.avs_fs_addfs);
        DLL_LOAD(loader, avs_fs_mount, ord.avs_fs_mount);
        DLL_LOAD(loader, avs_fs_umount, ord.avs_fs_umount);
        DLL_LOAD(loader, avs_fs_open, ord.avs_fs_open);
        DLL_LOAD(loader, avs_fs_read, ord.avs_fs_read);
        DLL_LOAD(loader, avs_fs_lseek, ord.avs_fs_lseek);
        DLL_LOAD(loader, avs_fs_close, ord.avs_fs_close);
        DLL_LOAD(loader, avs_fs_fstat, ord.avs_fs_fstat);
        DLL_LOAD(loader, avs_fs_opendir, ord.avs_fs_opendir);
        DLL_LOAD(loader, avs_fs_readdir, ord.avs_fs_readdir);
        DLL_LOAD(loader, avs_fs_closedir, ord.avs_fs_closedir);
        DLL_LOAD(loader, avs_fs_dump_mountpoint, ord.avs_fs_dump_mountpoint);
        DLL_LOAD(loader, avs_gheap_allocate, ord.avs_gheap_allocate);
        DLL_LOAD(loader, avs_gheap_free, ord.avs_gheap_free);
        DLL_LOAD(loader, property_create, ord.property_create);
        DLL_LOAD(loader, property_destroy, ord.property_destroy);
        DLL_LOAD(loader, property_search, ord.property_search);
        DLL_LOAD(loader, property_node_create, ord.property_node_create);
        DLL_LOAD(loader, property_node_refer, ord.property_node_refer);
        DLL_LOAD(loader, property_psmap_import, ord.property_psmap_import);
        DLL_LOAD(loader, property_insert_read, ord.property_insert_read);
        DLL_LOAD(loader, property_node_traversal, ord.property_node_traversal);
        DLL_LOAD(loader, property_node_name, ord.property_node_name);
        DLL_LOAD(loader, property_read_query_memsize, ord.property_read_query_memsize);
        DLL_LOAD(loader, property_read_query_memsize_long, ord.property_read_query_memsize_long);
        DLL_LOAD(loader, log_boot, ord.log_boot);
        DLL_LOAD(loader, log_body_info, ord.log_body_info);
        DLL_LOAD(loader, log_body_warning, ord.log_body_warning);
        DLL_LOAD(loader, log_body_misc, ord.log_body_misc);

        return avs_boot && avs_shutdown && avs_fs_mount && avs_fs_addfs && avs_filesys_imagefs &&
               property_create;
    }
};
