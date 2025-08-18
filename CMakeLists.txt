cmake_minimum_required(VERSION 3.31)
project(IdiotGame)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
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
)

add_executable(TestSelfPlay
        src/tests/RandomAi.cpp
        src/tests/RandomAi.hpp
)


target_compile_options(IdiotGame PRIVATE -Wall -Wextra -pedantic -std=c++23)
if (MINGW)
    target_link_libraries(IdiotGame PRIVATE stdc++exp)
endif()

include(GoogleTest)
gtest_discover_tests(TestSelfPlay)