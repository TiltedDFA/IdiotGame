cmake_minimum_required(VERSION 3.31)
project(IdiotGame)

set(CMAKE_CXX_STANDARD 23)

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

if (MINGW)
    target_link_libraries(IdiotGame PRIVATE stdc++exp)
endif()