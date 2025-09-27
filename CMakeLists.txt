cmake_minimum_required(VERSION 3.31)
project(IdiotGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------- GTest ----------------
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------- Common helpers ----------------
function(set_target_warnings tgt)
    if (MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(link_platform_bits tgt)
    # Optional libbacktrace (Linux/others) for std::stacktrace pretty output
    find_library(LIBBACKTRACE backtrace)
    if (LIBBACKTRACE)
        target_link_libraries(${tgt} PUBLIC ${LIBBACKTRACE})
    endif()

    # MinGW (GNU libstdc++) may need stdc++exp for <format>/<print>
    if (MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(${tgt} PUBLIC stdc++exp)
    endif()
endfunction()

# ---------------- Source lists (explicit for CLion) ----------------
set(DURAK_CORE_HEADERS
        src/core/Actions.hpp
        src/core/ClassicRules.hpp
        src/core/Exception.hpp
        src/core/Game.hpp
        src/core/OmegaException.hpp
        src/core/Player.hpp
        src/core/Rules.hpp
        src/core/State.hpp
        src/core/Types.hpp
        src/core/Util.hpp
        src/core/RandomAi.hpp
        src/core/Judge.hpp
)

set(DURAK_CORE_SOURCES
        src/core/ClassicRules.cpp
        src/core/Game.cpp
        src/core/RandomAi.cpp
        src/core/Judge.cpp
)

set(DURAK_DEBUG_HEADERS
        src/debug/AuditLogger.hpp
        src/debug/Inspector.hpp
        src/debug/Invariants.hpp
        src/debug/RecordingPlayer.hpp
)

set(DURAK_DEBUG_SOURCES
        src/debug/AuditLogger.cpp
)

set(APP_SOURCES
        src/main.cpp
)

# ---------------- Libraries ----------------
add_library(durak_core STATIC
        ${DURAK_CORE_SOURCES}
        ${DURAK_CORE_HEADERS}
)
target_include_directories(durak_core PUBLIC src)
target_compile_features(durak_core PUBLIC cxx_std_23)
set_target_warnings(durak_core)
link_platform_bits(durak_core)

add_library(durak_debug STATIC
        ${DURAK_DEBUG_SOURCES}
        ${DURAK_DEBUG_HEADERS}
)
target_include_directories(durak_debug PUBLIC src)
target_link_libraries(durak_debug PUBLIC durak_core)
target_compile_features(durak_debug PUBLIC cxx_std_23)
set_target_warnings(durak_debug)
link_platform_bits(durak_debug)

# ---------------- App ----------------
add_executable(IdiotGame ${APP_SOURCES})
target_link_libraries(IdiotGame PRIVATE durak_core durak_debug)
set_target_warnings(IdiotGame)

# ---------------- Tests (explicit lists + one-liner helper) ----------------
include(GoogleTest)

# Add your test source files here explicitly so CLion indexes them:
set(DURAK_TEST_SOURCES
        src/tests/selfplay.cpp
        src/tests/selfplay_6p.cpp
)

function(durak_add_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE durak_core durak_debug GTest::gtest_main)
    target_include_directories(${test_name} PRIVATE src)
    set_target_warnings(${test_name})
    link_platform_bits(${test_name})
    gtest_discover_tests(${test_name})
endfunction()

# One target per test source (clear in CLion, zero boilerplate elsewhere)
foreach(TSRC IN LISTS DURAK_TEST_SOURCES)
    get_filename_component(TNAME "${TSRC}" NAME_WE)
    durak_add_test("test_${TNAME}" "${TSRC}")
endforeach()
