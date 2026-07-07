cmake_minimum_required(VERSION 3.25)
project(Renderer573 LANGUAGES CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project must be built as 64-bit (x64)")
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
    src/formats/dxt_decode.cpp
    src/formats/bgra_crop.cpp
    src/formats/hsl_adjust.cpp
    src/formats/frame_process.cpp
)
target_include_directories(r573_formats PUBLIC src)
target_link_libraries(r573_formats PRIVATE r573::warnings)

add_library(r573_media_format STATIC
    src/media/media_format.cpp
)
target_include_directories(r573_media_format PUBLIC src)
target_link_libraries(r573_media_format PRIVATE r573::warnings)

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
    src/render/hsv_filter.cpp
    src/render/command_trace.cpp
    src/render/command_list.cpp
    src/render/command_diff.cpp
)
target_include_directories(r573_render PUBLIC src)
target_link_libraries(r573_render PRIVATE r573::warnings)

add_executable(renderer WIN32
    src/main.cpp
    src/tool_commands.cpp
    src/boot.cpp
    src/render_loop.cpp
    src/render_loop_requests.cpp
    src/render_executor.cpp
    src/render_live.cpp
    src/game_runtime.cpp
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
    src/gui/gui_renderer_tab.cpp
    src/gui/gui_qpro_panel.cpp
    src/gui/gui_splitter.cpp
    src/gui/gui_live_controls.cpp
    src/gui/gui_export_panel.cpp
    src/gui/gui_loading_overlay.cpp
    src/gui/gui_setup_view.cpp
    src/gui/gui_thread.cpp
    src/ifs_inspect.cpp
    src/avs_xml.cpp
    src/native_dialog.cpp
    src/export.cpp
    src/export_ddr.cpp
    src/video_encoder.cpp
    src/video_encoder_codecs.cpp
    src/media_sink.cpp
    src/game_profile.cpp
    src/arc_extract.cpp
    src/customize_extract.cpp
    src/qpro_dll.cpp
    src/qpro_scan.cpp
    src/qpro_extract.cpp
    src/qpro_walk.cpp
    src/qpro_composite.cpp
    src/qpro_back.cpp
    src/qpro_composite_debug.cpp
    src/afp_ddr_render.cpp
    src/afp_ddr_boot.cpp
    src/afp_ddr_test.cpp
)

target_compile_options(renderer PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

target_include_directories(renderer PRIVATE src)
target_include_directories(renderer SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})

target_link_libraries(renderer PRIVATE
    r573::warnings
    r573_support
    r573_formats
    r573_media_format
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
)

set_target_properties(renderer PROPERTIES
    OUTPUT_NAME                      "573Renderer"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

enable_testing()
include(Catch)

add_executable(formats_tests
    tests/formats/ddr_arc_tests.cpp
    tests/formats/frame_process_tests.cpp
    tests/formats/dxt_decode_tests.cpp
    tests/formats/bgra_crop_tests.cpp
    tests/formats/hsl_adjust_tests.cpp
)
target_link_libraries(formats_tests PRIVATE Catch2::Catch2WithMain r573_formats r573::warnings)
catch_discover_tests(formats_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

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
target_link_libraries(state_tests PRIVATE Catch2::Catch2WithMain r573_state r573::warnings)
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
                                                   r573_support)
catch_discover_tests(command_stream_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(pixel_golden_tests
    tests/render/pixel_golden_tests.cpp
    src/afp_d3d9_callbacks.cpp
    src/afp_d3d9_commands.cpp
    src/render_executor.cpp
    src/warp_device.cpp
)
target_include_directories(pixel_golden_tests PRIVATE src)
target_compile_definitions(pixel_golden_tests
    PRIVATE R573_LOCAL_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/local_baselines")
target_link_libraries(pixel_golden_tests PRIVATE Catch2::Catch2WithMain r573_render r573_support
                                                 d3d9)
catch_discover_tests(pixel_golden_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(local_dll_tests
    tests/local/dll_contract_tests.cpp
    src/avs_boot.cpp
    src/avs_xml.cpp
    src/game_profile.cpp
    src/ifs_inspect.cpp
    src/qpro_dll.cpp
)
target_include_directories(local_dll_tests PRIVATE src)
target_link_libraries(local_dll_tests PRIVATE Catch2::Catch2WithMain r573_support r573_state)
catch_discover_tests(local_dll_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS local_dll)

add_executable(media_encode_tests
    tests/media/media_encode_tests.cpp
    src/media_sink.cpp
    src/video_encoder.cpp
    src/video_encoder_codecs.cpp
)
target_include_directories(media_encode_tests PRIVATE src)
target_include_directories(media_encode_tests SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(media_encode_tests PRIVATE Catch2::Catch2WithMain r573_media_format
                                                 r573_support ${FFMPEG_LIBRARIES} bcrypt.lib
                                                 ws2_32.lib secur32.lib mfuuid.lib strmiids.lib
                                                 windowscodecs.lib ole32.lib)
catch_discover_tests(media_encode_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)
