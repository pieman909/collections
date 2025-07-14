cmake_minimum_required(VERSION 3.16)
project(DevEnvironment VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(DevEnvironment main.cpp)

target_link_libraries(DevEnvironment PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# For Windows: to add application icon and metadata
if(WIN32)
    set_target_properties(DevEnvironment PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
