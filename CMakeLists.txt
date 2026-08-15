cmake_minimum_required(VERSION 3.25)
project(Renderer573 LANGUAGES CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX)

option(R573_ASAN "Build with AddressSanitizer" OFF)
if(R573_ASAN AND MSVC)
    add_compile_options(/fsanitize=address)
    add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION
                            _DISABLE_OPTIONAL_ANNOTATION)
    add_link_options(/INCREMENTAL:NO)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(R573_ARCH_SUFFIX "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(R573_ARCH_SUFFIX "32")
else()
    message(FATAL_ERROR "Unsupported pointer size ${CMAKE_SIZEOF_VOID_P}")
endif()

find_package(imgui CONFIG REQUIRED)
find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat avutil swscale)
find_package(Catch2 3 CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)

set(R573_MSVC_WARNING_FLAGS
    /W4
    /w14062 /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311
    /w14545 /w14546 /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905
    /w14906 /w14928 /w15262
    /permissive- /Zc:preprocessor /Zc:__cplusplus /Zc:enumTypes /Zc:templateScope
    /Zc:throwingNew /Zc:externConstexpr /Zc:inline
    /utf-8
    /external:W0
)
add_library(r573_warnings INTERFACE)
add_library(r573::warnings ALIAS r573_warnings)
target_compile_options(r573_warnings INTERFACE "$<$<CXX_COMPILER_ID:MSVC>:${R573_MSVC_WARNING_FLAGS}>")

add_library(r573_support STATIC
    src/support/log.cpp
    src/support/dll_loader.cpp
    src/support/env.cpp
)
target_include_directories(r573_support PUBLIC src)
target_link_libraries(r573_support PUBLIC tl::expected PRIVATE r573::warnings)

add_library(r573_formats STATIC
    src/formats/ddr_arc.cpp
    src/formats/txp2.cpp
    src/formats/lzss.cpp
    src/formats/aes.cpp
    src/formats/blowfish.cpp
    src/formats/gcz.cpp
    src/formats/sysidx.cpp
    src/formats/gcanim.cpp
    src/formats/inz.cpp
    src/formats/xfile.cpp
    src/formats/xfile_lexer.cpp
    src/formats/xfile_binary.cpp
    src/formats/dxt_decode.cpp
    src/formats/bgra_crop.cpp
    src/formats/hsl_adjust.cpp
    src/formats/frame_process.cpp
)
target_include_directories(r573_formats PUBLIC src)
target_link_libraries(r573_formats PUBLIC tl::expected PRIVATE r573::warnings)

add_library(r573_media_format STATIC
    src/media/media_format.cpp
)
target_include_directories(r573_media_format PUBLIC src)
target_link_libraries(r573_media_format PRIVATE r573::warnings)

add_library(r573_media STATIC
    src/media_sink.cpp
    src/video_encoder.cpp
    src/video_encoder_codecs.cpp
)
target_include_directories(r573_media PUBLIC src)
target_include_directories(r573_media SYSTEM PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(r573_media
    PUBLIC r573_media_format ${FFMPEG_LIBRARIES} bcrypt.lib ws2_32.lib secur32.lib mfuuid.lib
           strmiids.lib windowscodecs.lib ole32.lib
    PRIVATE r573_support r573::warnings)

add_library(r573_cli STATIC
    src/cli/cli.cpp
    src/cli/tool_command.cpp
)
target_include_directories(r573_cli PUBLIC src)
target_link_libraries(r573_cli PUBLIC r573_media_format PRIVATE r573::warnings)

add_library(r573_settings STATIC
    src/settings/settings.cpp
)
target_include_directories(r573_settings PUBLIC src)
target_link_libraries(r573_settings PRIVATE r573::warnings)

add_library(r573_state STATIC
    src/state/app_state.cpp
    src/state/boot_lifecycle.cpp
    src/state/ifs_catalog.cpp
    src/state/live_controls.cpp
    src/state/telemetry.cpp
)
target_include_directories(r573_state PUBLIC src)
target_link_libraries(r573_state PUBLIC r573_settings PRIVATE r573::warnings)

add_library(r573_loop STATIC
    src/loop/frame_diff.cpp
    src/loop/ddr_loop_detector.cpp
    src/loop/modern_loop.cpp
    src/loop/blend_loop.cpp
    src/loop/frame_pacer.cpp
    src/loop/submonitor_cycler.cpp
    src/loop/cli_autopilot.cpp
)
target_include_directories(r573_loop PUBLIC src)
target_link_libraries(r573_loop PRIVATE r573::warnings)

add_library(r573_render STATIC
    src/render/blend_map.cpp
    src/render/vertex_math.cpp
    src/render/stretch.cpp
    src/render/hsv_filter.cpp
    src/render/command_trace.cpp
    src/render/command_list.cpp
    src/render/command_diff.cpp
    src/render/prim_layout.cpp
)
target_include_directories(r573_render PUBLIC src)
target_link_libraries(r573_render PRIVATE r573::warnings)

add_library(r573_app STATIC
    src/tool_commands.cpp
    src/boot.cpp
    src/backend/backend_registry.cpp
    src/backend/scene3d_backend.cpp
    src/gc2d/gc_package.cpp
    src/gc2d/gc_host.cpp
    src/gc2d/gc_sheet.cpp
    src/gc2d/gc_render.cpp
    src/game_revision.cpp
    src/backend/afp_profiles.cpp
    src/backend/afp_family_backend.cpp
    src/backend/afp_family_frame.cpp
    src/backend/afp_capture_drivers.cpp
    src/backend/afp_shaders.cpp
    src/backend/afp_modern_backend.cpp
    src/backend/afp_ddr_backend.cpp
    src/render_loop.cpp
    src/render_loop_requests.cpp
    src/render_executor.cpp
    src/render_live.cpp
    src/game_runtime.cpp
    src/game_runtime_modern.cpp
    src/game_runtime_ddr.cpp
    src/render_seh.cpp
    src/window.cpp
    src/avs_boot.cpp
    src/afp_boot.cpp
    src/afp_packages.cpp
    src/afp_anim.cpp
    src/render_backend.cpp
    src/afp_d3d9.cpp
    src/afp_d3d9_callbacks.cpp
    src/afp_d3d9_commands.cpp
    src/afp_d3d9_textures.cpp
    src/mc_control.cpp
    src/gui/gui_window.cpp
    src/gui/gui_panels.cpp
    src/gui/gui_scene3d_panel.cpp
    src/gui/gui_gc2d_panel.cpp
    src/gui/gui_preset_panel.cpp
    src/gui/gui_preset_workspace.cpp
    src/gui/panel_registry.cpp
    src/gui/gui_style.cpp
    src/gui/gui_widgets.cpp
    src/gui/gui_scene_panel.cpp
    src/gui/gui_inspector.cpp
    src/gui/gui_timeline.cpp
    src/gui/gui_qpro_panel.cpp
    src/gui/gui_splitter.cpp
    src/gui/gui_export_panel.cpp
    src/gui/gui_loading_overlay.cpp
    src/gui/gui_setup_view.cpp
    src/gui/gui_thread.cpp
    src/ifs_inspect.cpp
    src/anim_inspect.cpp
    src/avs_xml.cpp
    src/native_dialog.cpp
    src/export.cpp
    src/game_profile.cpp
    src/arc_extract.cpp
    src/customize_extract.cpp
    src/qpro/qpro_dll.cpp
    src/qpro/qpro_scan.cpp
    src/qpro/qpro_extract.cpp
    src/qpro/qpro_detail.cpp
    src/qpro/qpro_body.cpp
    src/qpro/qpro_debug.cpp
    src/qpro/qpro_walk.cpp
    src/qpro/qpro_composite.cpp
    src/qpro/qpro_back.cpp
    src/qpro/qpro_composite_debug.cpp
    src/support/crash_report.cpp
    src/warp_device.cpp
    src/afp_ddr_geo.cpp
    src/afp_ddr_render.cpp
    src/afp_ddr_textures.cpp
    src/afp_ddr_draw.cpp
    src/afp_ddr_vtable.cpp
    src/afp_ddr_render_shape.cpp
    src/afp_ddr_boot.cpp
    src/afp_ddr_txp2.cpp
    src/afp_ddr_test.cpp
    src/game_fingerprint.cpp
    src/preset/scene_presets.cpp
    src/preset/scene_presets_iidx10.cpp
    src/preset/scene_presets_iidx11.cpp
    src/preset/preset_host.cpp
    src/preset/preset_params.cpp
    src/preset/preset_schema.cpp
    src/preset/preset_effective.cpp
    src/preset/preset_rng.cpp
    src/preset/preset_test.cpp
    src/scene3d/anim.cpp
    src/scene3d/camera.cpp
    src/scene3d/scene3d_host.cpp
    src/scene3d/scene3d_input.cpp
    src/scene3d/scene3d.cpp
    src/scene3d/scene3d_render.cpp
    src/scene3d/scene3d_test.cpp
)

target_include_directories(r573_app PUBLIC src)
target_include_directories(r573_app SYSTEM PUBLIC ${FFMPEG_INCLUDE_DIRS})

target_link_libraries(r573_app
    PUBLIC
        r573_support
        r573_formats
        r573_media
        r573_cli
        r573_settings
        r573_state
        r573_loop
        r573_render
        imgui::imgui
        ${FFMPEG_LIBRARIES}
        d3d9.lib
        bcrypt.lib
        ws2_32.lib
        secur32.lib
        mfuuid.lib
        strmiids.lib
        windowscodecs.lib
        ole32.lib
    PRIVATE
        r573::warnings
)

add_executable(renderer WIN32 src/main.cpp)
target_link_libraries(renderer PRIVATE r573_app r573::warnings)

set_target_properties(renderer PROPERTIES
    OUTPUT_NAME                      "573Renderer${R573_ARCH_SUFFIX}"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

enable_testing()
include(Catch)

add_executable(formats_tests
    tests/formats/ddr_arc_tests.cpp
    tests/formats/txp2_tests.cpp
    tests/formats/model3d_tests.cpp
    tests/formats/model3d_real_tests.cpp
    tests/formats/sysidx_tests.cpp
    tests/formats/aes_tests.cpp
    tests/formats/blowfish_tests.cpp
    tests/formats/xfile_binary_tests.cpp
    src/gc2d/gc_package.cpp
    tests/formats/frame_process_tests.cpp
    tests/formats/dxt_decode_tests.cpp
    tests/formats/bgra_crop_tests.cpp
    tests/formats/hsl_adjust_tests.cpp
    tests/formats/gcanim_tests.cpp
)
target_link_libraries(formats_tests PRIVATE Catch2::Catch2WithMain r573_formats r573_support r573::warnings)
catch_discover_tests(formats_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(game_tests
    tests/game/game_profile_tests.cpp
    tests/game/mc_control_tests.cpp
    tests/game/scene_preset_tests.cpp
    src/game_profile.cpp
    src/game_revision.cpp
    src/game_fingerprint.cpp
    src/backend/afp_profiles.cpp
    src/preset/scene_presets.cpp
    src/preset/scene_presets_iidx10.cpp
    src/preset/scene_presets_iidx11.cpp
    src/preset/preset_params.cpp
    src/preset/preset_schema.cpp
    src/preset/preset_effective.cpp
    src/preset/preset_rng.cpp
    src/mc_control.cpp
)
target_include_directories(game_tests PRIVATE src)
target_link_libraries(game_tests PRIVATE Catch2::Catch2WithMain r573_support r573::warnings)
catch_discover_tests(game_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(cli_tests
    tests/cli/cli_tests.cpp
)
target_link_libraries(cli_tests PRIVATE Catch2::Catch2WithMain r573_cli r573::warnings)
catch_discover_tests(cli_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(settings_tests
    tests/settings/settings_tests.cpp
)
target_link_libraries(settings_tests PRIVATE Catch2::Catch2WithMain r573_settings r573::warnings)
catch_discover_tests(settings_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(media_format_tests
    tests/media/media_format_tests.cpp
)
target_link_libraries(media_format_tests PRIVATE Catch2::Catch2WithMain r573_media_format
                                                 r573::warnings)
catch_discover_tests(media_format_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(state_tests
    tests/state/app_state_tests.cpp
    tests/state/fifo_queue_tests.cpp
)
target_link_libraries(state_tests PRIVATE Catch2::Catch2WithMain r573_state r573_media_format
                                             r573::warnings)
catch_discover_tests(state_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(support_tests
    tests/support/support_tests.cpp
)
target_link_libraries(support_tests PRIVATE Catch2::Catch2WithMain r573_support r573::warnings)
catch_discover_tests(support_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(loop_tests
    tests/loop/frame_diff_tests.cpp
    tests/loop/ddr_loop_detector_tests.cpp
    tests/loop/modern_loop_tests.cpp
    tests/loop/blend_loop_tests.cpp
    tests/loop/frame_pacer_tests.cpp
    tests/loop/submonitor_cycler_tests.cpp
    tests/loop/cli_autopilot_tests.cpp
)
target_link_libraries(loop_tests PRIVATE Catch2::Catch2WithMain r573_loop r573::warnings)
catch_discover_tests(loop_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(render_tests
    tests/render/blend_map_tests.cpp
    tests/render/vertex_math_tests.cpp
    tests/render/hsv_filter_tests.cpp
    tests/render/command_trace_tests.cpp
    tests/render/command_list_tests.cpp
    tests/render/command_diff_tests.cpp
    tests/render/prim_layout_tests.cpp
    tests/render/stretch_tests.cpp
)
target_link_libraries(render_tests PRIVATE Catch2::Catch2WithMain r573_render r573::warnings)
catch_discover_tests(render_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(command_stream_tests
    tests/render/command_stream_gate_tests.cpp
    src/afp_d3d9_callbacks.cpp
    src/afp_d3d9_commands.cpp
    src/render_executor.cpp
)
target_include_directories(command_stream_tests PRIVATE src)
target_compile_definitions(command_stream_tests
    PRIVATE R573_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/render/goldens")
target_link_libraries(command_stream_tests PRIVATE Catch2::Catch2WithMain r573_render
                                                   r573_support r573::warnings)
catch_discover_tests(command_stream_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(pixel_golden_tests
    tests/render/pixel_golden_tests.cpp
    src/afp_d3d9_callbacks.cpp
    src/afp_d3d9_commands.cpp
    src/render_backend.cpp
    src/render_executor.cpp
    src/warp_device.cpp
)
target_include_directories(pixel_golden_tests PRIVATE src)
target_compile_definitions(pixel_golden_tests
    PRIVATE R573_LOCAL_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/local_baselines")
target_link_libraries(pixel_golden_tests PRIVATE Catch2::Catch2WithMain r573_render r573_support
                                                 d3d9 r573::warnings)
catch_discover_tests(pixel_golden_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(local_dll_tests
    tests/local/dll_contract_tests.cpp
    tests/local/iidx10_screen_data_tests.cpp
    src/avs_boot.cpp
    src/avs_xml.cpp
    src/game_profile.cpp
    src/ifs_inspect.cpp
    src/qpro/qpro_dll.cpp
    src/gc2d/gc_package.cpp
    src/preset/scene_presets.cpp
    src/preset/scene_presets_iidx10.cpp
    src/preset/scene_presets_iidx11.cpp
    src/preset/preset_params.cpp
    src/preset/preset_schema.cpp
    src/preset/preset_effective.cpp
    src/preset/preset_rng.cpp
)
target_include_directories(local_dll_tests PRIVATE src)
target_link_libraries(local_dll_tests PRIVATE Catch2::Catch2WithMain r573_support r573_state
                                              r573_formats r573::warnings)
catch_discover_tests(local_dll_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS local_dll)

add_executable(export_capture_tests
    tests/export/export_capture_tests.cpp
    src/export.cpp
)
target_include_directories(export_capture_tests PRIVATE src)
target_include_directories(export_capture_tests SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(export_capture_tests PRIVATE Catch2::Catch2WithMain r573_media r573_state
                                                   r573_loop r573_formats r573_support
                                                   r573::warnings)
catch_discover_tests(export_capture_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(media_encode_tests
    tests/media/media_encode_tests.cpp
)
target_include_directories(media_encode_tests PRIVATE src)
target_include_directories(media_encode_tests SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(media_encode_tests PRIVATE Catch2::Catch2WithMain r573_media r573_support
                                                 r573::warnings)
catch_discover_tests(media_encode_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(gui_tests
    tests/gui/gui_test_harness.cpp
    tests/gui/setup_view_tests.cpp
    tests/gui/ready_view_tests.cpp
    tests/gui/shell_tests.cpp
    tests/gui/inspector_tests.cpp
    tests/gui/browse_panel_tests.cpp
    tests/gui/scene_panel_tests.cpp
    tests/gui/export_panel_tests.cpp
    tests/gui/timeline_tests.cpp
    tests/gui/qpro_panel_tests.cpp
    tests/gui/host_panel_tests.cpp
    tests/gui/preset_panel_tests.cpp
    tests/gui/host_live_tests.cpp
    tests/gui/gui_mock_assets.cpp
    tests/gui/gui_gpu.cpp
    tests/gui/gui_pixels.cpp
    tests/gui/window_tests.cpp
    tests/gui/tooltip_tests.cpp
    tests/gui/input_mode_tests.cpp
    tests/gui/render_window_tests.cpp
    tests/gui/scenario_tests.cpp
)
target_include_directories(gui_tests PRIVATE tests/gui src/gui)
target_link_libraries(gui_tests PRIVATE Catch2::Catch2WithMain r573_app r573::warnings)
catch_discover_tests(gui_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

get_directory_property(R573_ALL_TARGETS BUILDSYSTEM_TARGETS)
foreach(r573_target IN LISTS R573_ALL_TARGETS)
    get_target_property(r573_target_type ${r573_target} TYPE)
    if(r573_target_type STREQUAL "EXECUTABLE" OR r573_target_type STREQUAL "STATIC_LIBRARY")
        get_target_property(r573_target_libs ${r573_target} LINK_LIBRARIES)
        if(NOT r573_target_libs MATCHES "r573(::|_)warnings")
            message(FATAL_ERROR
                "target '${r573_target}' does not link r573::warnings - every compiled target must")
        endif()
    endif()
endforeach()
