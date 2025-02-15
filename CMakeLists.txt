cmake_minimum_required(VERSION 3.18)
project(AI-Digi-Ellie LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add nlohmann/json
add_subdirectory(vendor/json)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/inference.cpp
    src/conversation.cpp
    src/system_control.cpp
)

add_executable(AI-Digi-Ellie ${SOURCES})

target_link_libraries(AI-Digi-Ellie 
    PRIVATE 
    nlohmann_json::nlohmann_json
)

if(WIN32)
    # Create the destination directory first
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Release")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")
endif()

set_property(TARGET AI-Digi-Ellie PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")