cmake_minimum_required(VERSION 3.20)

project(DeutschTelex VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

set(DEUTSCHTELEX_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${DEUTSCHTELEX_GENERATED_DIR}")
configure_file(
    src/version.h.in
    "${DEUTSCHTELEX_GENERATED_DIR}/deutschtelex_version.h"
    @ONLY
)

add_library(deutschtelex_version INTERFACE)
target_include_directories(deutschtelex_version
    INTERFACE
        "${DEUTSCHTELEX_GENERATED_DIR}"
)

function(deutschtelex_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(deutschtelex_config
    src/config/settings_store.cpp
)
target_include_directories(deutschtelex_config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(deutschtelex_config PUBLIC cxx_std_20)
deutschtelex_enable_warnings(deutschtelex_config)

add_library(deutschtelex_core
    src/core/transform_engine.cpp
)

target_include_directories(deutschtelex_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(deutschtelex_core PUBLIC cxx_std_20)
deutschtelex_enable_warnings(deutschtelex_core)

if(WIN32)
    enable_language(RC)
    configure_file(
        src/resources/deutschtelex.rc.in
        "${DEUTSCHTELEX_GENERATED_DIR}/deutschtelex.rc"
        @ONLY
    )
    configure_file(
        installer/DeutschTelex.iss.in
        "${DEUTSCHTELEX_GENERATED_DIR}/DeutschTelex.iss"
        @ONLY
    )

    add_library(deutschtelex_win32
        src/win32/foreground_app.cpp
        src/win32/input_injector.cpp
        src/win32/key_decoder.cpp
        src/win32/keyboard_hook.cpp
        src/win32/mouse_reset_hook.cpp
        src/win32/startup_registration.cpp
    )

    target_include_directories(deutschtelex_win32
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(deutschtelex_win32 PUBLIC cxx_std_20)
    target_compile_definitions(deutschtelex_win32 PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(deutschtelex_win32 PUBLIC deutschtelex_core User32 Advapi32)
    deutschtelex_enable_warnings(deutschtelex_win32)

    add_library(deutschtelex_app
        src/app/settings_window.cpp
        src/app/tray_app.cpp
    )
    target_include_directories(deutschtelex_app
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(deutschtelex_app PUBLIC cxx_std_20)
    target_link_libraries(deutschtelex_app PUBLIC deutschtelex_config deutschtelex_win32
        deutschtelex_version Shell32 Ole32)
    deutschtelex_enable_warnings(deutschtelex_app)

    add_executable(DeutschTelex WIN32
        src/main.cpp
        "${DEUTSCHTELEX_GENERATED_DIR}/deutschtelex.rc"
    )
    target_compile_features(DeutschTelex PRIVATE cxx_std_20)
    target_link_libraries(DeutschTelex PRIVATE deutschtelex_app)
    if(MINGW)
        target_link_options(DeutschTelex PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
    deutschtelex_enable_warnings(DeutschTelex)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(deutschtelex_version_tests
        tests/version_tests.cpp
    )
    target_link_libraries(deutschtelex_version_tests PRIVATE deutschtelex_version)
    target_compile_features(deutschtelex_version_tests PRIVATE cxx_std_20)
    deutschtelex_enable_warnings(deutschtelex_version_tests)
    add_test(NAME release_metadata COMMAND deutschtelex_version_tests)

    add_executable(deutschtelex_config_tests
        tests/settings_store_tests.cpp
    )
    target_link_libraries(deutschtelex_config_tests PRIVATE deutschtelex_config)
    target_compile_features(deutschtelex_config_tests PRIVATE cxx_std_20)
    deutschtelex_enable_warnings(deutschtelex_config_tests)
    add_test(NAME settings_store COMMAND deutschtelex_config_tests)

    add_executable(deutschtelex_core_tests
        tests/transform_engine_tests.cpp
    )

    target_link_libraries(deutschtelex_core_tests PRIVATE deutschtelex_core)
    target_compile_features(deutschtelex_core_tests PRIVATE cxx_std_20)
    deutschtelex_enable_warnings(deutschtelex_core_tests)

    add_test(NAME transform_engine COMMAND deutschtelex_core_tests)

    add_executable(deutschtelex_core_property_tests
        tests/transform_engine_property_tests.cpp
    )
    target_link_libraries(deutschtelex_core_property_tests PRIVATE deutschtelex_core)
    target_compile_features(deutschtelex_core_property_tests PRIVATE cxx_std_20)
    deutschtelex_enable_warnings(deutschtelex_core_property_tests)
    add_test(NAME transform_engine_properties COMMAND deutschtelex_core_property_tests)

    if(WIN32)
        add_executable(deutschtelex_win32_tests
            tests/win32_component_tests.cpp
        )
        target_compile_features(deutschtelex_win32_tests PRIVATE cxx_std_20)
        target_link_libraries(deutschtelex_win32_tests PRIVATE deutschtelex_app)
        deutschtelex_enable_warnings(deutschtelex_win32_tests)

        add_test(NAME win32_components COMMAND deutschtelex_win32_tests)
        add_test(NAME hook_lifecycle COMMAND DeutschTelex --hook-smoke-test)
    endif()
endif()
