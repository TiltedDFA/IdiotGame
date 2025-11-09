cmake_minimum_required(VERSION 3.31)
project(IdiotGame LANGUAGES CXX)

# Allow FetchContent_Populate for header-only deps we do NOT want to configure.
if (POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------- Helpers ----------------
function(set_target_warnings tgt)
    if (MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(link_platform_bits tgt)
    # Optional libbacktrace for nicer stacktraces (if present)
    find_library(LIBBACKTRACE backtrace)
    if (LIBBACKTRACE)
        target_link_libraries(${tgt} PUBLIC ${LIBBACKTRACE})
    endif()

    # MinGW (GNU libstdc++) may need stdc++exp for <format>/<print>
    if (MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(${tgt} PUBLIC stdc++exp)
    endif()
endfunction()

include(FetchContent)

# ---------------- GTest ----------------
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------- FlatBuffers ----------------
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC ON  CACHE BOOL "" FORCE)
FetchContent_Declare(
        flatbuffers
        URL https://github.com/google/flatbuffers/archive/refs/tags/v24.3.25.zip
)
FetchContent_MakeAvailable(flatbuffers)

if (TARGET flatbuffers AND NOT TARGET flatbuffers::flatbuffers)
    add_library(flatbuffers::flatbuffers ALIAS flatbuffers)
endif()
if (TARGET flatc AND NOT TARGET flatbuffers::flatc)
    add_executable(flatbuffers::flatc ALIAS flatc)
endif()

# ---- Schema codegen (C++ headers) ----
set(DURAK_FBS_SCHEMAS
        src/schemas/durak_net.fbs
)

set(DURAK_FBS_OUT_DIR "${CMAKE_BINARY_DIR}/generated/flatbuffers")
file(MAKE_DIRECTORY "${DURAK_FBS_OUT_DIR}")

set(DURAK_FBS_GENERATED_HEADERS "")
foreach(FBS ${DURAK_FBS_SCHEMAS})
    get_filename_component(_name "${FBS}" NAME_WE)
    set(_in  "${CMAKE_CURRENT_SOURCE_DIR}/${FBS}")
    set(_out "${DURAK_FBS_OUT_DIR}/${_name}_generated.h")
    list(APPEND DURAK_FBS_GENERATED_HEADERS "${_out}")

    add_custom_command(
            OUTPUT  "${_out}"
            COMMAND $<TARGET_FILE:flatc>
            --cpp
            --scoped-enums
            --no-prefix
            --filename-suffix "_generated"
            -o "${DURAK_FBS_OUT_DIR}"
            "${_in}"
            DEPENDS "${_in}" flatc
            VERBATIM
            COMMENT "flatc: ${FBS} -> ${_out}"
    )
endforeach()

add_custom_target(durak_fbs_codegen
        DEPENDS ${DURAK_FBS_GENERATED_HEADERS}
)

add_library(durak_fbs INTERFACE)
add_dependencies(durak_fbs durak_fbs_codegen)
target_include_directories(durak_fbs INTERFACE "${DURAK_FBS_OUT_DIR}")
target_link_libraries(durak_fbs INTERFACE flatbuffers::flatbuffers)

# ---- Copy generated headers into src tree for direct includes ----
set(DURAK_FBS_SRC_COPY_DIR "${CMAKE_SOURCE_DIR}/src/generated/flatbuffers")
file(MAKE_DIRECTORY "${DURAK_FBS_SRC_COPY_DIR}")

set(DURAK_FBS_SRC_COPIES "")
foreach(FBS ${DURAK_FBS_SCHEMAS})
    get_filename_component(_name "${FBS}" NAME_WE)
    set(_gen "${DURAK_FBS_OUT_DIR}/${_name}_generated.h")
    set(_dst "${DURAK_FBS_SRC_COPY_DIR}/${_name}_generated.h")
    list(APPEND DURAK_FBS_SRC_COPIES "${_dst}")

    add_custom_command(
            OUTPUT  "${_dst}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_gen}" "${_dst}"
            DEPENDS "${_gen}"
            VERBATIM
            COMMENT "Copy FlatBuffers header to src: ${_name}_generated.h"
    )
    set_source_files_properties("${_dst}" PROPERTIES GENERATED TRUE)
endforeach()

add_custom_target(durak_fbs_src_copy
        DEPENDS ${DURAK_FBS_SRC_COPIES}
)
add_dependencies(durak_fbs_src_copy durak_fbs_codegen)

# ---------------- WebSocket++ + standalone Asio (FETCH-ONLY) ----------------
# We populate sources without configuring their (old) CMake projects.
FetchContent_Declare(
        websocketpp
        URL https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.zip
)
FetchContent_Declare(
        asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-0.zip
)

# Populate sources only (no add_subdirectory)
FetchContent_Populate(websocketpp)
FetchContent_Populate(asio)

# Threads
find_package(Threads REQUIRED)

# Provide a clean interface target that exports includes/defs/libs
add_library(durak_netdeps INTERFACE)
target_include_directories(durak_netdeps INTERFACE
        "${websocketpp_SOURCE_DIR}"
        "${asio_SOURCE_DIR}/asio/include"
)
target_compile_definitions(durak_netdeps INTERFACE
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_THREAD_
        _WEBSOCKETPP_CPP11_CHRONO_
        _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
)
if (WIN32)
    target_compile_definitions(durak_netdeps INTERFACE _WIN32_WINNT=0x0601)
    target_link_libraries(durak_netdeps INTERFACE ws2_32 mswsock)
endif()
target_link_libraries(durak_netdeps INTERFACE Threads::Threads)

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
        src/net/codec.hpp
        src/net/RemotePlayer.hpp
)

set(DURAK_CORE_SOURCES
        src/core/ClassicRules.cpp
        src/core/Game.cpp
        src/core/RandomAi.cpp
        src/core/Judge.cpp
        src/net/codec.cpp
        src/net/RemotePlayer.cpp
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
target_link_libraries(durak_core PUBLIC durak_fbs durak_netdeps)
add_dependencies(durak_core durak_fbs_codegen durak_fbs_src_copy)

add_library(durak_debug STATIC
        ${DURAK_DEBUG_SOURCES}
        ${DURAK_DEBUG_HEADERS}
)
target_include_directories(durak_debug PUBLIC src)
target_link_libraries(durak_debug PUBLIC durak_core)
target_compile_features(durak_debug PUBLIC cxx_std_23)
set_target_warnings(durak_debug)
link_platform_bits(durak_debug)
add_dependencies(durak_debug durak_fbs_src_copy)

# ---------------- App(s) ----------------
add_executable(IdiotGame ${APP_SOURCES})
target_link_libraries(IdiotGame PRIVATE durak_core durak_debug)
set_target_warnings(IdiotGame)
link_platform_bits(IdiotGame)
add_dependencies(IdiotGame durak_fbs_src_copy)

# (Optional) Stage A apps if/when you add the sources:
 add_executable(DurakServer src/DurakServerMain.cpp)
 target_link_libraries(DurakServer PRIVATE durak_core durak_debug)
 if(MINGW)
    target_link_options(DurakServer PRIVATE -static-libstdc++ -static-libgcc)
 endif()
 set_target_warnings(DurakServer)
 link_platform_bits(DurakServer)
 add_dependencies(DurakServer durak_fbs_src_copy)

 add_executable(netai src/NetAIClientMain.cpp)
 target_link_libraries(netai PRIVATE durak_core)
 set_target_warnings(netai)
 link_platform_bits(netai)
 add_dependencies(netai durak_fbs_src_copy)

# ---------------- Tests ----------------
include(GoogleTest)

set(DURAK_TEST_SOURCES
        src/tests/selfplay.cpp
        src/tests/selfplay_6p.cpp
        src/tests/CodecRandAi.cpp
)

function(durak_add_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE durak_core durak_debug GTest::gtest_main)
    target_include_directories(${test_name} PRIVATE src)
    set_target_warnings(${test_name})
    link_platform_bits(${test_name})
    gtest_discover_tests(${test_name})
    add_dependencies(${test_name} durak_fbs_src_copy)
endfunction()

foreach(TSRC IN LISTS DURAK_TEST_SOURCES)
    get_filename_component(TNAME "${TSRC}" NAME_WE)
    durak_add_test("test_${TNAME}" "${TSRC}")
endforeach()
