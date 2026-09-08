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
find_package(nlohmann_json CONFIG REQUIRED)

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
    src/support/math/float_trig.cpp
)
if(MSVC)
    set_source_files_properties(src/support/math/float_trig.cpp
        PROPERTIES COMPILE_OPTIONS "/fp:precise")
endif()
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
    src/media/movie_source.cpp
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

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(R573_LAYER_VERDICTS "${CMAKE_BINARY_DIR}/generated/preset_layer_verdicts.cpp")
add_custom_command(
    OUTPUT "${R573_LAYER_VERDICTS}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/ci/gen_layer_verdicts.py"
            "${CMAKE_SOURCE_DIR}/docs/preset_layers.md" "${R573_LAYER_VERDICTS}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/ci/gen_layer_verdicts.py"
            "${CMAKE_SOURCE_DIR}/docs/preset_layers.md"
    COMMENT "Generating preset layer verdicts from docs/preset_layers.md"
    VERBATIM
)

add_library(r573_app STATIC
    "${R573_LAYER_VERDICTS}"
    src/tool_commands.cpp
    src/boot.cpp
    src/backend/backend_registry.cpp
    src/backend/scene3d_backend.cpp
    src/backend/preset_clear_color.cpp
    src/backend/preset_command_apply.cpp
    src/editor/clip_form.cpp
    src/editor/clip_problems.cpp
    src/editor/clip_summary.cpp
    src/editor/command_palette.cpp
    src/editor/document_edits.cpp
    src/editor/preset_editor_state.cpp
    src/editor/timeline_drag.cpp
    src/editor/timeline_lanes.cpp
    src/editor/timeline_edits.cpp
    src/editor/timeline_view.cpp
    src/editor/tween_edits.cpp
    src/editor/curve_geometry.cpp
    src/editor/options_model.cpp
    src/editor/library_model.cpp
    src/editor/export_range.cpp
    src/editor/frame_inspector_model.cpp
    src/gc2d/gc_package.cpp
    src/gc2d/gc_host.cpp
    src/gc2d/gc_sheet.cpp
    src/gc2d/gc_render.cpp
    src/gc2d/gc_sprite.cpp
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
    src/gui/gui_preset_library.cpp
    src/gui/gui_preset_library_actions.cpp
    src/gui/timeline/gui_tl_frame_inspector.cpp
    src/gui/panel_registry.cpp
    src/gui/gui_style.cpp
    src/gui/gui_dpi.cpp
    src/gui/gui_widgets.cpp
    src/gui/gui_scene_panel.cpp
    src/gui/gui_inspector.cpp
    src/gui/gui_timeline.cpp
    src/gui/timeline/gui_tl_editor.cpp
    src/gui/timeline/gui_tl_transport.cpp
    src/gui/timeline/gui_tl_ruler.cpp
    src/gui/timeline/gui_tl_tracks.cpp
    src/gui/timeline/gui_tl_clips.cpp
    src/gui/timeline/gui_tl_drag.cpp
    src/gui/timeline/gui_tl_colors.cpp
    src/gui/timeline/gui_tl_forms.cpp
    src/gui/timeline/gui_tl_asset_picker.cpp
    src/gui/timeline/gui_tl_preview.cpp
    src/gui/timeline/gui_tl_palette.cpp
    src/gui/timeline/gui_tl_clip_modal.cpp
    src/gui/timeline/gui_tl_clip_tab.cpp
    src/gui/timeline/gui_tl_tween_tab.cpp
    src/gui/timeline/gui_tl_curve.cpp
    src/gui/timeline/gui_tl_options.cpp
    src/gui/timeline/gui_tl_option_modal.cpp
    src/gui/timeline/gui_tl_doc_modal.cpp
    src/gui/timeline/gui_tl_problems.cpp
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
    src/preset/preset_host.cpp
    src/preset/preset_host_push.cpp
    src/preset/preset_preview.cpp
    src/preset/preset_rng.cpp
    src/preset/preset_test.cpp
    src/preset/preset_tools.cpp
    src/preset/eval/eval_tween.cpp
    src/preset/eval/eval_models.cpp
    src/preset/eval/eval_sprites.cpp
    src/preset/eval/eval_camera_lights.cpp
    src/preset/eval/eval_scene.cpp
    src/preset/eval/eval_particles.cpp
    src/preset/eval/eval_resolve.cpp
    src/preset/eval/eval_poly.cpp
    src/preset/eval/frame_report.cpp
    src/preset/eval/eval_ease.cpp
    src/preset/eval/eval_emit.cpp
    src/preset/eval/preset_evaluator.cpp
    src/preset/defaults/defaults.cpp
    src/preset/defaults/defaults_build.cpp
    src/preset/defaults/iidx10_defaults.cpp
    src/preset/defaults/iidx11_defaults.cpp
    src/preset/defaults/iidx11_select_defaults.cpp
    src/preset/defaults/iidx11_ending_a_defaults.cpp
    src/preset/defaults/iidx11_ending_b_defaults.cpp
    src/preset/defaults/iidx12_defaults.cpp
    src/preset/defaults/iidx12_mode_defaults.cpp
    src/preset/defaults/iidx12_dan_defaults.cpp
    src/preset/defaults/iidx12_music_defaults.cpp
    src/preset/defaults/iidx12_ending_defaults.cpp
    src/preset/defaults/iidx12_2d_defaults.cpp
    src/preset/doc/preset_enum_names.cpp
    src/preset/doc/preset_fields.cpp
    src/preset/doc/preset_fields_2d.cpp
    src/preset/doc/preset_fields_3d.cpp
    src/preset/doc/preset_json.cpp
    src/preset/doc/preset_registry.cpp
    src/preset/doc/preset_validate.cpp
    src/scene3d/anim.cpp
    src/scene3d/atlas.cpp
    src/scene3d/camera.cpp
    src/scene3d/poly_draw.cpp
    src/scene3d/poly_vertex.cpp
    src/scene3d/scene3d_host.cpp
    src/scene3d/scene3d_input.cpp
    src/scene3d/scene3d_merge.cpp
    src/scene3d/scene3d.cpp
    src/scene3d/scene3d_fog.cpp
    src/scene3d/scene3d_material.cpp
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
    src/gc2d/gc_sprite.cpp
    tests/formats/gc_sprite_tests.cpp
    tests/formats/frame_process_tests.cpp
    tests/formats/dxt_decode_tests.cpp
    tests/formats/bgra_crop_tests.cpp
    tests/formats/hsl_adjust_tests.cpp
    tests/formats/gcanim_tests.cpp
    tests/formats/scene3d_atlas_tests.cpp
    tests/formats/scene3d_fog_tests.cpp
    tests/formats/scene3d_material_tests.cpp
    tests/formats/scene3d_anim_tests.cpp
    src/scene3d/anim.cpp
    src/scene3d/atlas.cpp
    src/scene3d/scene3d_fog.cpp
    src/scene3d/scene3d_material.cpp
)
target_link_libraries(formats_tests PRIVATE Catch2::Catch2WithMain r573_formats r573_support r573::warnings)
catch_discover_tests(formats_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

add_executable(game_tests
    tests/game/game_profile_tests.cpp
    tests/game/mc_control_tests.cpp
    tests/game/preset_json_tests.cpp
    tests/game/preset_validate_tests.cpp
    tests/game/preset_defaults_golden_tests.cpp
    tests/game/preset_golden_format.cpp
    tests/game/preset_push_text.cpp
    tests/game/preset_legacy_view.cpp
    tests/game/preset_eval_tests.cpp
    tests/game/preset_poly_tests.cpp
    tests/game/poly_vertex_tests.cpp
    tests/game/preset_option_tests.cpp
    tests/game/preset_defaults_tests.cpp
    tests/game/preset_defaults_2d_tests.cpp
    tests/game/preset_registry_tests.cpp
    tests/game/eval_tween_tests.cpp
    tests/game/preset_host_tests.cpp
    tests/game/preset_preview_tests.cpp
    tests/game/preset_host_stubs.cpp
    tests/editor/clip_form_tests.cpp
    tests/editor/clip_problems_tests.cpp
    tests/editor/timeline_editor_tests.cpp
    tests/editor/tween_edits_tests.cpp
    tests/editor/curve_geometry_tests.cpp
    tests/editor/options_model_tests.cpp
    tests/editor/library_model_tests.cpp
    tests/editor/export_range_tests.cpp
    tests/editor/frame_inspector_model_tests.cpp
    src/backend/preset_clear_color.cpp
    src/backend/preset_command_apply.cpp
    src/editor/clip_form.cpp
    src/editor/clip_problems.cpp
    src/editor/clip_summary.cpp
    src/editor/command_palette.cpp
    src/editor/document_edits.cpp
    src/editor/preset_editor_state.cpp
    src/editor/timeline_drag.cpp
    src/editor/timeline_lanes.cpp
    src/editor/timeline_edits.cpp
    src/editor/timeline_view.cpp
    src/editor/tween_edits.cpp
    src/editor/curve_geometry.cpp
    src/editor/options_model.cpp
    src/editor/library_model.cpp
    src/editor/export_range.cpp
    src/editor/frame_inspector_model.cpp
    src/preset/preset_host.cpp
    src/preset/preset_host_push.cpp
    src/preset/preset_preview.cpp
    src/gc2d/gc_sprite.cpp
    src/scene3d/scene3d_merge.cpp
    src/scene3d/scene3d_fog.cpp
    src/scene3d/scene3d_material.cpp
    src/game_profile.cpp
    src/game_revision.cpp
    src/game_fingerprint.cpp
    src/backend/afp_profiles.cpp
    src/preset/preset_rng.cpp
    src/preset/eval/eval_tween.cpp
    src/preset/eval/eval_models.cpp
    src/preset/eval/eval_sprites.cpp
    src/preset/eval/eval_camera_lights.cpp
    src/preset/eval/eval_scene.cpp
    src/preset/eval/eval_particles.cpp
    src/preset/eval/eval_resolve.cpp
    src/preset/eval/eval_poly.cpp
    src/preset/eval/frame_report.cpp
    src/preset/eval/eval_ease.cpp
    src/preset/eval/eval_emit.cpp
    src/preset/eval/preset_evaluator.cpp
    src/preset/defaults/defaults.cpp
    src/preset/defaults/defaults_build.cpp
    src/preset/defaults/iidx10_defaults.cpp
    src/preset/defaults/iidx11_defaults.cpp
    src/preset/defaults/iidx11_select_defaults.cpp
    src/preset/defaults/iidx11_ending_a_defaults.cpp
    src/preset/defaults/iidx11_ending_b_defaults.cpp
    src/preset/defaults/iidx12_defaults.cpp
    src/preset/defaults/iidx12_mode_defaults.cpp
    src/preset/defaults/iidx12_dan_defaults.cpp
    src/preset/defaults/iidx12_music_defaults.cpp
    src/preset/defaults/iidx12_ending_defaults.cpp
    src/preset/defaults/iidx12_2d_defaults.cpp
    src/preset/doc/preset_enum_names.cpp
    src/preset/doc/preset_fields.cpp
    src/preset/doc/preset_fields_2d.cpp
    src/preset/doc/preset_fields_3d.cpp
    src/preset/doc/preset_json.cpp
    src/preset/doc/preset_registry.cpp
    src/preset/preset_tools.cpp
    src/preset/doc/preset_validate.cpp
    src/scene3d/poly_vertex.cpp
    src/mc_control.cpp
)
target_include_directories(game_tests PRIVATE src)
target_compile_definitions(game_tests
    PRIVATE R573_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/game/fixtures")
target_link_libraries(game_tests PRIVATE Catch2::Catch2WithMain r573_support r573_formats
                                         nlohmann_json::nlohmann_json r573::warnings)
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
    tests/support/float_trig_tests.cpp
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
    src/preset/defaults/defaults.cpp
    src/preset/defaults/defaults_build.cpp
    src/preset/defaults/iidx10_defaults.cpp
    src/preset/defaults/iidx11_defaults.cpp
    src/preset/defaults/iidx11_select_defaults.cpp
    src/preset/defaults/iidx11_ending_a_defaults.cpp
    src/preset/defaults/iidx11_ending_b_defaults.cpp
    src/preset/defaults/iidx12_defaults.cpp
    src/preset/defaults/iidx12_mode_defaults.cpp
    src/preset/defaults/iidx12_dan_defaults.cpp
    src/preset/defaults/iidx12_music_defaults.cpp
    src/preset/defaults/iidx12_ending_defaults.cpp
    src/preset/defaults/iidx12_2d_defaults.cpp
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

add_executable(media_decode_tests
    tests/media/movie_decode_tests.cpp
)
target_include_directories(media_decode_tests PRIVATE src)
target_include_directories(media_decode_tests SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(media_decode_tests PRIVATE Catch2::Catch2WithMain r573_media r573_support
                                                 r573::warnings)
catch_discover_tests(media_decode_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)

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
    tests/gui/library_tests.cpp
    tests/gui/frame_inspector_tests.cpp
    tests/gui/clip_modal_tests.cpp
    tests/gui/modal_layout_tests.cpp
    tests/gui/dpi_scaling_tests.cpp
    tests/gui/timeline_editor_tests.cpp
    tests/gui/tween_ui_tests.cpp
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
