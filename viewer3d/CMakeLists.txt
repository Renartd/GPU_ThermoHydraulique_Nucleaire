cmake_minimum_required(VERSION 3.10)
project(viewer3d)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Corrade + Magnum (version 2020)
find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED
    GL
    MeshTools
    Primitives
    SceneGraph
    Shaders
    Sdl2Application
)

add_executable(viewer3d
    main.cpp
    viewer.cpp
    viewer.h
    Application.cpp
    Application.hpp
)

target_link_libraries(viewer3d
    Corrade::Main
    Magnum::GL
    Magnum::MeshTools
    Magnum::Primitives
    Magnum::SceneGraph
    Magnum::Shaders
    Magnum::Sdl2Application
)
