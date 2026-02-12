cmake_minimum_required(VERSION 3.10)
project(vulkan_compute_demo)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

add_executable(vulkan_compute
    src/main.cpp
)

target_include_directories(vulkan_compute PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vulkan_compute PRIVATE ${Vulkan_LIBRARIES})
