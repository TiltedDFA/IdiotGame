cmake_minimum_required(VERSION 3.31)
project(IdiotGame)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(IdiotGame
        src/core/OmegaException.hpp
        src/main.cpp
        src/core/Util.hpp
        src/core/Types.hpp
        src/core/Exception.hpp
        src/core/Actions.hpp
        src/core/State.hpp
        src/core/Player.hpp
        src/core/Rules.hpp
        src/core/Game.hpp
        src/core/ClassicRules.cpp
        src/core/ClassicRules.hpp
        src/core/Game.cpp
        src/debug/Inspector.hpp
        src/debug/Invariants.hpp
        src/debug/AuditLogger.cpp
        src/debug/AuditLogger.hpp
)

add_executable(TestSelfPlay
        src/core/OmegaException.hpp
        src/core/Util.hpp
        src/core/Types.hpp
        src/core/Exception.hpp
        src/core/Actions.hpp
        src/core/State.hpp
        src/core/Player.hpp
        src/core/Rules.hpp
        src/core/Game.hpp
        src/core/ClassicRules.cpp
        src/core/ClassicRules.hpp
        src/core/Game.cpp
        src/debug/Inspector.hpp
        src/debug/Invariants.hpp
        src/debug/AuditLogger.cpp
        src/debug/AuditLogger.hpp
        src/core/RandomAi.cpp
        src/core/RandomAi.hpp
        src/tests/selfplay.cpp
)


target_compile_options(IdiotGame PRIVATE -Wall -Wextra -pedantic -std=c++23)
if (MINGW)
    target_link_libraries(IdiotGame PRIVATE stdc++exp)
    target_link_libraries(TestSelfPlay PRIVATE stdc++exp)
endif()

target_link_libraries(TestSelfPlay PRIVATE GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(TestSelfPlay)