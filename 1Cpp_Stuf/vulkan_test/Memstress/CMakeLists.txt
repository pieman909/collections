cmake_minimum_required(VERSION 3.10)
project(GPU_Stress_Test)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
add_executable(StressTest main.cpp)
target_link_libraries(StressTest Vulkan::Vulkan)
