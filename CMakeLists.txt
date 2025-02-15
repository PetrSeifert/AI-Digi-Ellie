cmake_minimum_required(VERSION 3.18)

set(PROJECT_NAME "AI-Digi-Ellie")
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(vendor/json)
add_subdirectory(vendor/DPP)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/inference.cpp
    src/conversation.cpp
    src/system_control.cpp
    src/discord_bot.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    nlohmann_json::nlohmann_json
    dpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    vendor/DPP/include
)

# Set output directory
set_property(TARGET ${PROJECT_NAME} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(WIN32)
    # Set output directory for different configurations
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    )

    # Find all DLL files
    file(GLOB DPP_DLLS_RELEASE "${CMAKE_BINARY_DIR}/vendor/DPP/library/Release/*.dll")
    file(GLOB DPP_DLLS_LIB "${CMAKE_BINARY_DIR}/vendor/DPP/library/*.dll")

    # Copy DLLs after build
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    
        # Copy DLLs from Release directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DPP_DLLS_RELEASE}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

        # Copy DLLs from library directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DPP_DLLS_LIB}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()