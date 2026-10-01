cmake_minimum_required(VERSION 3.15)
project(EscapeFromBea CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/lang.cpp
    src/maze.cpp
    src/assets.cpp
    src/player.cpp
    src/enemy.cpp
    src/ui.cpp
    src/cinematic.cpp
    src/game.cpp
    src/save.cpp 
)

add_executable(escape_from_bea ${SOURCES})
target_include_directories(escape_from_bea PRIVATE src)

find_package(raylib QUIET)
if(raylib_FOUND)
    target_link_libraries(escape_from_bea PRIVATE raylib)
else()
    find_library(RAYLIB_LIB raylib REQUIRED)
    find_path(RAYLIB_INC raylib.h REQUIRED)
    target_include_directories(escape_from_bea PRIVATE ${RAYLIB_INC})
    target_link_libraries(escape_from_bea PRIVATE ${RAYLIB_LIB})
endif()

if(WIN32)
    target_link_libraries(escape_from_bea PRIVATE winmm)
endif()
