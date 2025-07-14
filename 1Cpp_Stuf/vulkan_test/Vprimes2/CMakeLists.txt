cmake_minimum_required(VERSION 3.10)
project(VulkanPrimality)

set(CMAKE_CXX_STANDARD 17)

# Find the Vulkan package
find_package(Vulkan REQUIRED)

add_executable(VulkanPrimality main.cpp)

# Link the Vulkan library to your executable
target_link_libraries(VulkanPrimality Vulkan::Vulkan)

# Add an instruction to notify the user about shader compilation
message(STATUS "Don't forget to compile the shader: glslc primality.comp -o primality.spv")