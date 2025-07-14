cmake_minimum_required(VERSION 3.10)
project(VulkanSieve)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
add_executable(VulkanSieve main.cpp)
target_link_libraries(VulkanSieve Vulkan::Vulkan)

message(STATUS "Don't forget to compile the shader: glslc sieve.comp -o sieve.spv")
