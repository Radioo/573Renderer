cmake_minimum_required(VERSION 3.25)
if(NOT PROJECT_NAME)
    project(IfsEditor LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_compile_definitions(NOMINMAX)

set(R573_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
include("${R573_ROOT}/cmake/r573_shared_sources.cmake")
list(TRANSFORM R573_EDITOR_SUPPORT_SOURCES PREPEND "${R573_ROOT}/")
list(TRANSFORM R573_EDITOR_FORMATS_SOURCES PREPEND "${R573_ROOT}/")
list(TRANSFORM R573_EDITOR_DOCUMENT_SOURCES PREPEND "${R573_ROOT}/")
list(TRANSFORM R573_EDITOR_PREVIEW_SOURCES PREPEND "${R573_ROOT}/")

find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets Svg)
find_package(qtadvanceddocking-qt6 CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(unofficial-hash-library CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_program(R573_FLATC flatc REQUIRED)

set(R573_PREVIEW_SCHEMA "${R573_ROOT}/src/preview/preview_host.fbs")
set(R573_PREVIEW_GENERATED "${CMAKE_BINARY_DIR}/generated/preview_host_generated.h")
if(TARGET r573_preview_schema)
    add_custom_target(editor_preview_schema DEPENDS r573_preview_schema)
else()
    add_custom_command(
        OUTPUT "${R573_PREVIEW_GENERATED}"
        COMMAND "${R573_FLATC}" --cpp --scoped-enums -o "${CMAKE_BINARY_DIR}/generated"
                "${R573_PREVIEW_SCHEMA}"
        DEPENDS "${R573_PREVIEW_SCHEMA}"
        COMMENT "Generating the preview host protocol from preview_host.fbs"
        VERBATIM
    )
    add_custom_target(editor_preview_schema DEPENDS "${R573_PREVIEW_GENERATED}")
endif()

add_library(editor_shared STATIC
    ${R573_EDITOR_SUPPORT_SOURCES}
    ${R573_EDITOR_FORMATS_SOURCES}
    ${R573_EDITOR_DOCUMENT_SOURCES}
    ${R573_EDITOR_PREVIEW_SOURCES}
)
add_dependencies(editor_shared editor_preview_schema)
target_include_directories(editor_shared PUBLIC "${R573_ROOT}/src" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(editor_shared
    PUBLIC
        tl::expected
        flatbuffers::flatbuffers
    PRIVATE
        unofficial::hash-library
        nlohmann_json::nlohmann_json
)

set(R573_EDITOR_APP_SOURCES
    src/editor_window.cpp
    src/editor_commands.cpp
    src/editor_menus.cpp
    src/editor_depth_menu.cpp
    src/editor_refusals.cpp
    src/editor_shell.cpp
    src/editor_search.cpp
    src/editor_inspector.cpp
    src/editor_selection_bar.cpp
    src/editor_popover.cpp
    src/editor_notices.cpp
    src/editor_package_panel.cpp
    src/editor_stage_bar.cpp
    src/editor_theme.cpp
    src/editor_icons.cpp
    src/editor_rows.cpp
    src/editor_panel_tabs.cpp
    src/editor_start_screen.cpp
    src/editor_tool_strip.cpp
    src/editor_timeline_bar.cpp
    src/editor_inspector_panel.cpp
    src/editor_files.cpp
    src/editor_host.cpp
    src/editor_project.cpp
    src/editor_keys.cpp
    src/editor_clips.cpp
    src/editor_stage.cpp
    src/editor_spans.cpp
    src/editor_hidden.cpp
    src/editor_timeline_menu.cpp
    src/editor_snapshot.cpp
    src/editor_images.cpp
    src/editor_library.cpp
    src/editor_history.cpp
    src/editor_filter.cpp
    src/editor_graph.cpp
    src/editor_animations.cpp
    src/editor_key_selection.cpp
    src/editor_drift_sheet.cpp
    src/editor_ease_editor.cpp
    src/editor_playback.cpp
    src/editor_layout.cpp
    src/editor_timeline.cpp
    src/editor_timeline_paint.cpp
    src/editor_viewport.cpp
)

add_executable(ifs_editor WIN32
    src/editor_main.cpp
    ${R573_EDITOR_APP_SOURCES}
)
target_include_directories(ifs_editor PRIVATE src)
target_link_libraries(ifs_editor PRIVATE editor_shared Qt6::Widgets Qt6::Svg ads::qtadvanceddocking-qt6
                                       d3d9.lib)

set(R573_VCPKG_EXE "${R573_ROOT}/vendor/vcpkg/vcpkg.exe")
set(R573_VCPKG_BIN "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin")

function(r573_deploy_qt_plugin target plugin)
    get_target_property(plugin_type ${plugin} QT_PLUGIN_TYPE)
    set(plugin_dir "$<TARGET_FILE_DIR:${target}>/${plugin_type}")
    set(beside "$<TARGET_FILE_DIR:${target}>/${target}_$<TARGET_FILE_NAME:${plugin}>")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${plugin_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${plugin}>" "${plugin_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${plugin}>" "${beside}"
        COMMAND "${R573_VCPKG_EXE}" z-applocal "--target-binary=${beside}"
                "--installed-bin-dir=${R573_VCPKG_BIN}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${beside}"
        VERBATIM
    )
endfunction()

r573_deploy_qt_plugin(ifs_editor Qt6::QWindowsIntegrationPlugin)
r573_deploy_qt_plugin(ifs_editor Qt6::QModernWindowsStylePlugin)
r573_deploy_qt_plugin(ifs_editor Qt6::QJpegPlugin)
r573_deploy_qt_plugin(ifs_editor Qt6::QSvgPlugin)
r573_deploy_qt_plugin(ifs_editor Qt6::QSvgIconPlugin)

add_executable(ifs_editor_shot
    src/editor_shot_main.cpp
    ${R573_EDITOR_APP_SOURCES}
)
target_include_directories(ifs_editor_shot PRIVATE src)
target_link_libraries(ifs_editor_shot PRIVATE editor_shared Qt6::Widgets Qt6::Svg ads::qtadvanceddocking-qt6
                                            d3d9.lib)
r573_deploy_qt_plugin(ifs_editor_shot Qt6::QJpegPlugin)
r573_deploy_qt_plugin(ifs_editor_shot Qt6::QSvgPlugin)
r573_deploy_qt_plugin(ifs_editor_shot Qt6::QSvgIconPlugin)

add_executable(editor_widget_tests
    tests/editor_widget_tests.cpp
    tests/editor_dialog_widget_tests.cpp
    tests/editor_graph_widget_tests.cpp
    tests/editor_timeline_widget_tests.cpp
    tests/editor_search_widget_tests.cpp
    src/editor_search.cpp
    src/editor_inspector.cpp
    src/editor_filter.cpp
    src/editor_icons.cpp
    src/editor_theme.cpp
    src/editor_rows.cpp
    src/editor_graph.cpp
    src/editor_timeline.cpp
    src/editor_timeline_paint.cpp
    src/editor_viewport.cpp
    src/editor_drift_sheet.cpp
    src/editor_ease_editor.cpp
)
target_include_directories(editor_widget_tests PRIVATE src)
target_link_libraries(editor_widget_tests PRIVATE editor_shared Qt6::Widgets Qt6::Svg
                                                   Catch2::Catch2)
r573_deploy_qt_plugin(editor_widget_tests Qt6::QMinimalIntegrationPlugin)
r573_deploy_qt_plugin(editor_widget_tests Qt6::QJpegPlugin)
r573_deploy_qt_plugin(editor_widget_tests Qt6::QSvgPlugin)
r573_deploy_qt_plugin(editor_widget_tests Qt6::QSvgIconPlugin)

add_executable(editor_window_tests
    tests/editor_window_tests.cpp
    tests/editor_live_window_tests.cpp
    tests/editor_panel_window_tests.cpp
    tests/editor_key_window_tests.cpp
    tests/editor_depth_window_tests.cpp
    tests/editor_command_window_tests.cpp
    tests/editor_bar_window_tests.cpp
    tests/editor_shell_window_tests.cpp
    tests/editor_search_window_tests.cpp
    tests/editor_inspector_window_tests.cpp
    tests/editor_selection_window_tests.cpp
    src/editor_window.cpp
    src/editor_commands.cpp
    src/editor_menus.cpp
    src/editor_depth_menu.cpp
    src/editor_refusals.cpp
    src/editor_shell.cpp
    src/editor_search.cpp
    src/editor_inspector.cpp
    src/editor_selection_bar.cpp
    src/editor_popover.cpp
    src/editor_notices.cpp
    src/editor_package_panel.cpp
    src/editor_stage_bar.cpp
    src/editor_theme.cpp
    src/editor_icons.cpp
    src/editor_rows.cpp
    src/editor_panel_tabs.cpp
    src/editor_start_screen.cpp
    src/editor_tool_strip.cpp
    src/editor_timeline_bar.cpp
    src/editor_inspector_panel.cpp
    src/editor_files.cpp
    src/editor_host.cpp
    src/editor_project.cpp
    src/editor_keys.cpp
    src/editor_clips.cpp
    src/editor_stage.cpp
    src/editor_spans.cpp
    src/editor_hidden.cpp
    src/editor_timeline_menu.cpp
    src/editor_snapshot.cpp
    src/editor_images.cpp
    src/editor_library.cpp
    src/editor_history.cpp
    src/editor_filter.cpp
    src/editor_graph.cpp
    src/editor_animations.cpp
    src/editor_key_selection.cpp
    src/editor_drift_sheet.cpp
    src/editor_ease_editor.cpp
    src/editor_playback.cpp
    src/editor_layout.cpp
    src/editor_timeline.cpp
    src/editor_timeline_paint.cpp
    src/editor_viewport.cpp
)
target_include_directories(editor_window_tests PRIVATE src "${R573_ROOT}/tests/document")
target_link_libraries(editor_window_tests PRIVATE editor_shared Qt6::Widgets Qt6::Svg
                                                  ads::qtadvanceddocking-qt6 d3d9.lib Catch2::Catch2)
r573_deploy_qt_plugin(editor_window_tests Qt6::QMinimalIntegrationPlugin)
r573_deploy_qt_plugin(editor_window_tests Qt6::QJpegPlugin)
r573_deploy_qt_plugin(editor_window_tests Qt6::QSvgPlugin)
r573_deploy_qt_plugin(editor_window_tests Qt6::QSvgIconPlugin)
