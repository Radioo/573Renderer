find_package(Boost REQUIRED COMPONENTS interprocess)

set(R573_ENCODE_SCHEMA "${CMAKE_SOURCE_DIR}/src/encode/encode_host.fbs")
set(R573_ENCODE_GENERATED "${CMAKE_BINARY_DIR}/generated/encode_host_generated.h")
add_custom_command(
    OUTPUT "${R573_ENCODE_GENERATED}"
    COMMAND "${R573_FLATC}" --cpp --scoped-enums -o "${CMAKE_BINARY_DIR}/generated" "${R573_ENCODE_SCHEMA}"
    DEPENDS "${R573_ENCODE_SCHEMA}"
    COMMENT "Generating the encoder protocol from encode_host.fbs"
    VERBATIM
)
add_custom_target(r573_encode_schema DEPENDS "${R573_ENCODE_GENERATED}")

add_library(r573_encode_client STATIC
    src/encode/frame_section.cpp
    src/encode/encode_client.cpp
)
add_dependencies(r573_encode_client r573_encode_schema)
target_include_directories(r573_encode_client PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(r573_encode_client
    PUBLIC
        r573_preview_protocol
        r573_media_format
        r573_support
        Boost::interprocess
        flatbuffers::flatbuffers
    PRIVATE
        r573::warnings
)

if(R573_ARCH_SUFFIX STREQUAL "32")
    target_compile_definitions(r573_encode_client PUBLIC R573_REMOTE_ENCODER)
    target_link_libraries(r573_app PUBLIC r573_encode_client)
    return()
endif()

add_library(r573_encode_session STATIC
    src/encode/host/encode_session.cpp
)
target_link_libraries(r573_encode_session PUBLIC r573_encode_client r573_media
                                          PRIVATE r573::warnings)

add_executable(encode_host
    src/encode/host/encode_host_main.cpp
)
target_link_libraries(encode_host PRIVATE r573_encode_session r573::warnings)
set_target_properties(encode_host PROPERTIES
    OUTPUT_NAME                      "573Encoder"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

add_executable(encode_host_tests
    tests/encode/encode_host_tests.cpp
)
target_link_libraries(encode_host_tests PRIVATE Catch2::Catch2WithMain r573_encode_session
                                                r573::warnings)
target_compile_definitions(encode_host_tests PRIVATE R573_ENCODE_HOST_EXE="$<TARGET_FILE:encode_host>")
add_dependencies(encode_host_tests encode_host)
catch_discover_tests(encode_host_tests DISCOVERY_MODE POST_BUILD PROPERTIES LABELS ci)
