cmake_minimum_required(VERSION 3.20)

project(DeutschTelex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

function(deutschtelex_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

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
    add_library(deutschtelex_win32
        src/win32/input_injector.cpp
        src/win32/key_decoder.cpp
        src/win32/keyboard_hook.cpp
    )

    target_include_directories(deutschtelex_win32
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(deutschtelex_win32 PUBLIC cxx_std_20)
    target_compile_definitions(deutschtelex_win32 PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(deutschtelex_win32 PUBLIC deutschtelex_core User32)
    deutschtelex_enable_warnings(deutschtelex_win32)

    add_executable(DeutschTelex src/main.cpp)
    target_compile_features(DeutschTelex PRIVATE cxx_std_20)
    target_link_libraries(DeutschTelex PRIVATE deutschtelex_win32)
    if(MINGW)
        target_link_options(DeutschTelex PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    deutschtelex_enable_warnings(DeutschTelex)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(deutschtelex_core_tests
        tests/transform_engine_tests.cpp
    )

    target_link_libraries(deutschtelex_core_tests PRIVATE deutschtelex_core)
    target_compile_features(deutschtelex_core_tests PRIVATE cxx_std_20)
    deutschtelex_enable_warnings(deutschtelex_core_tests)

    add_test(NAME transform_engine COMMAND deutschtelex_core_tests)

    if(WIN32)
        add_executable(deutschtelex_win32_tests
            tests/win32_component_tests.cpp
        )
        target_compile_features(deutschtelex_win32_tests PRIVATE cxx_std_20)
        target_link_libraries(deutschtelex_win32_tests PRIVATE deutschtelex_win32)
        deutschtelex_enable_warnings(deutschtelex_win32_tests)

        add_test(NAME win32_components COMMAND deutschtelex_win32_tests)
        add_test(NAME hook_lifecycle COMMAND DeutschTelex --hook-smoke-test)
    endif()
endif()
