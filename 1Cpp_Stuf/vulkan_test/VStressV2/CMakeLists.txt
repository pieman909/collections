cmake_minimum_required(VERSION 3.12)
project(GraphicsStressTest)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED) # Find the GLFW library

add_executable(StressTest main.cpp)

# Link both Vulkan and GLFW
target_link_libraries(StressTest Vulkan::Vulkan glfw)

