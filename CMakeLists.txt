cmake_minimum_required(VERSION 3.18)

set(PROJECT_NAME "AI-Digi-Ellie")
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_subdirectory(vendor/json)
add_subdirectory(vendor/DPP)
add_subdirectory(vendor/spdlog)

set(SOURCES
    src/main.cpp
    src/discord_bot.cpp
    src/discord_bot/core.cpp
    src/discord_bot/voice.cpp
    src/discord_bot/commands.cpp
    src/discord_bot/tts.cpp
    src/discord_bot/message.cpp
    src/azure_tts.cpp
    src/inference.cpp
    src/conversation.cpp
    src/system_control.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    nlohmann_json::nlohmann_json
    dpp
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)
    
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/vendor/DPP/include
    ${PROJECT_SOURCE_DIR}/vendor/spdlog/include
    ${OPENSSL_INCLUDE_DIR}
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
    file(GLOB SPDLOG_DLLS "${CMAKE_BINARY_DIR}/vendor/spdlog/Release/*.dll")

    # Get OpenSSL DLL paths
    get_filename_component(OPENSSL_ROOT_DIR ${OPENSSL_INCLUDE_DIR} DIRECTORY)
    set(OPENSSL_DLLS
        "${OPENSSL_ROOT_DIR}/bin/libssl-3-x64.dll"
        "${OPENSSL_ROOT_DIR}/bin/libcrypto-3-x64.dll"
    )

    # Copy DLLs after build
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    
        # Copy DPP DLLs from Release directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DPP_DLLS_RELEASE}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

        # Copy DPP DLLs from library directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DPP_DLLS_LIB}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

        # Copy OpenSSL DLLs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENSSL_DLLS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

        # Copy spdlog DLLs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SPDLOG_DLLS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()