cmake_minimum_required(VERSION 3.10)
project(viewer3d)

set(CMAKE_CXX_STANDARD 14)

find_package(Corrade REQUIRED Containers Utility)
find_package(Magnum REQUIRED GL MeshTools Shaders Primitives Trade SceneGraph Sdl2Application)

file(GLOB SOURCES "*.cpp")

add_executable(viewer3d ${SOURCES})

target_include_directories(viewer3d PRIVATE .)

target_link_libraries(viewer3d PRIVATE
    Corrade::Containers
    Corrade::Utility
    Magnum::GL
    Magnum::MeshTools
    Magnum::Shaders
    Magnum::Primitives
    Magnum::Trade
    Magnum::SceneGraph
    Magnum::Sdl2Application
)
