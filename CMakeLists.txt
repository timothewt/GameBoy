cmake_minimum_required(VERSION 3.10)
project(gameboy)

set(CMAKE_CXX_STANDARD 20)

# Find SDL2 via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(${SDL2_INCLUDE_DIRS} src include)
link_directories(${SDL2_LIBRARY_DIRS})

# Automatically include all .cpp files in src/
file(GLOB SOURCES src/*.cpp)

add_executable(gameboy ${SOURCES})
target_link_libraries(gameboy ${SDL2_LIBRARIES})

