cmake_minimum_required(VERSION 3.25)
project(IfsEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_compile_definitions(NOMINMAX)

set(R573_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
include("${R573_ROOT}/cmake/r573_shared_sources.cmake")
list(TRANSFORM R573_EDITOR_SUPPORT_SOURCES PREPEND "${R573_ROOT}/")
list(TRANSFORM R573_EDITOR_FORMATS_SOURCES PREPEND "${R573_ROOT}/")
list(TRANSFORM R573_EDITOR_DOCUMENT_SOURCES PREPEND "${R573_ROOT}/")
list(TRANSFORM R573_EDITOR_PREVIEW_SOURCES PREPEND "${R573_ROOT}/")

find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)
find_package(qtadvanceddocking-qt6 CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(unofficial-hash-library CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_program(R573_FLATC flatc REQUIRED)

set(R573_PREVIEW_SCHEMA "${R573_ROOT}/src/preview/preview_host.fbs")
set(R573_PREVIEW_GENERATED "${CMAKE_BINARY_DIR}/generated/preview_host_generated.h")
add_custom_command(
    OUTPUT "${R573_PREVIEW_GENERATED}"
    COMMAND "${R573_FLATC}" --cpp --scoped-enums -o "${CMAKE_BINARY_DIR}/generated"
            "${R573_PREVIEW_SCHEMA}"
    DEPENDS "${R573_PREVIEW_SCHEMA}"
    COMMENT "Generating the preview host protocol from preview_host.fbs"
    VERBATIM
)
add_custom_target(editor_preview_schema DEPENDS "${R573_PREVIEW_GENERATED}")

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

add_executable(ifs_editor WIN32
    src/editor_main.cpp
    src/editor_window.cpp
    src/editor_files.cpp
    src/editor_host.cpp
    src/editor_project.cpp
    src/editor_keys.cpp
    src/editor_clips.cpp
    src/editor_stage.cpp
    src/editor_key_selection.cpp
    src/editor_ease_dialog.cpp
    src/editor_playback.cpp
    src/editor_layout.cpp
    src/editor_timeline.cpp
    src/editor_viewport.cpp
)
target_include_directories(ifs_editor PRIVATE src)
target_link_libraries(ifs_editor PRIVATE editor_shared Qt6::Widgets ads::qtadvanceddocking-qt6
                                       d3d9.lib)

function(r573_deploy_qt_plugin target plugin)
    get_target_property(plugin_type ${plugin} QT_PLUGIN_TYPE)
    set(plugin_dir "$<TARGET_FILE_DIR:${target}>/${plugin_type}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${plugin_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${plugin}>" "${plugin_dir}"
        VERBATIM
    )
endfunction()

r573_deploy_qt_plugin(ifs_editor Qt6::QWindowsIntegrationPlugin)
r573_deploy_qt_plugin(ifs_editor Qt6::QModernWindowsStylePlugin)

add_executable(editor_widget_tests
    tests/editor_widget_tests.cpp
    src/editor_timeline.cpp
    src/editor_viewport.cpp
    src/editor_ease_dialog.cpp
)
target_include_directories(editor_widget_tests PRIVATE src)
target_link_libraries(editor_widget_tests PRIVATE editor_shared Qt6::Widgets Catch2::Catch2)
r573_deploy_qt_plugin(editor_widget_tests Qt6::QMinimalIntegrationPlugin)
