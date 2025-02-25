cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "AI-Digi-Ellie")
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenSSL REQUIRED)

# Download Whisper model if not exists
set(WHISPER_MODEL_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q8_0.bin?download=true")
set(WHISPER_MODEL_FILENAME "ggml-large-v3-turbo-q8_0.bin")
set(WHISPER_MODEL_SOURCE_PATH "${CMAKE_SOURCE_DIR}/whisper_models/${WHISPER_MODEL_FILENAME}")
set(WHISPER_MODEL_BUILD_PATH "${CMAKE_BINARY_DIR}/whisper_models/${WHISPER_MODEL_FILENAME}")

# Create models directory
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/whisper_models")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/whisper_models")

# Check if model exists in either source or build directory
if(EXISTS "${WHISPER_MODEL_SOURCE_PATH}")
    message(STATUS "Whisper model found in source directory, copying to build directory...")
    file(COPY "${WHISPER_MODEL_SOURCE_PATH}" DESTINATION "${CMAKE_BINARY_DIR}/whisper_models/")
    set(WHISPER_MODEL_PATH "${WHISPER_MODEL_BUILD_PATH}")
elseif(EXISTS "${WHISPER_MODEL_BUILD_PATH}")
    message(STATUS "Whisper model found in build directory...")
    set(WHISPER_MODEL_PATH "${WHISPER_MODEL_BUILD_PATH}")
else()
    message(STATUS "Downloading Whisper model...")
    file(DOWNLOAD
        ${WHISPER_MODEL_URL}
        ${WHISPER_MODEL_BUILD_PATH}
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Whisper model")
    endif()
    
    # Copy to source models directory as well
    file(COPY ${WHISPER_MODEL_BUILD_PATH} DESTINATION "${CMAKE_SOURCE_DIR}/whisper_models/")
    set(WHISPER_MODEL_PATH "${WHISPER_MODEL_BUILD_PATH}")
endif()

# Add vendor libraries with specific build directories
add_subdirectory(vendor/json)
add_subdirectory(vendor/DPP)
add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/cpp-httplib)
add_subdirectory(vendor/whisper.cpp)

# Main bot sources
set(BOT_SOURCES
    src/main.cpp
    src/discord_bot.cpp
    src/discord_bot/core.cpp
    src/discord_bot/voice.cpp
    src/discord_bot/commands.cpp
    src/discord_bot/message.cpp
    src/azure_tts.cpp
    src/whisper_client.cpp
    src/inference.cpp
    src/conversation.cpp
)

# Whisper service sources
set(WHISPER_SERVICE_SOURCES
    src/whisper_service_main.cpp
    src/whisper_service.cpp
    src/whisper_stt.cpp
)

# Main bot executable
add_executable(${PROJECT_NAME} ${BOT_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE 
    nlohmann_json::nlohmann_json
    dpp
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    httplib::httplib
)

# Whisper service executable
add_executable(whisper_service ${WHISPER_SERVICE_SOURCES})
target_link_libraries(whisper_service PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    httplib::httplib
    whisper
)

# Common include directories for both targets
set(COMMON_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/vendor/spdlog/include
    ${PROJECT_SOURCE_DIR}/vendor/cpp-httplib
    ${PROJECT_SOURCE_DIR}/vendor/json/include
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${COMMON_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/vendor/DPP/include
    ${OPENSSL_INCLUDE_DIR}
)

target_include_directories(whisper_service PRIVATE
    ${PROJECT_SOURCE_DIR}/vendor/whisper.cpp
    ${COMMON_INCLUDE_DIRS}
)

# Set output directories
set_property(TARGET ${PROJECT_NAME} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_property(TARGET whisper_service PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Copy Whisper model to bin directory
if(WIN32)
    # Create whisper_models directory in the output directory
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Release/whisper_models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Debug/whisper_models"
    )
    
    add_custom_command(TARGET whisper_service PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Release/whisper_models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Debug/whisper_models"
    )
    
    # Copy model file to output directories
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WHISPER_MODEL_PATH}"
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/whisper_models/"
    )
    
    add_custom_command(TARGET whisper_service POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WHISPER_MODEL_PATH}"
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/whisper_models/"
    )
else()
    foreach(TARGET ${PROJECT_NAME} whisper_service)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/whisper_models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WHISPER_MODEL_PATH}"
                "$<TARGET_FILE_DIR:${TARGET}>/whisper_models/"
        )
    endforeach()
endif()

if(WIN32)
    # Set output directory for different configurations
    foreach(TARGET ${PROJECT_NAME} whisper_service)
        set_target_properties(${TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
        )
    endforeach()

    # Find all DLL files
    file(GLOB DPP_DLLS_RELEASE "${CMAKE_BINARY_DIR}/vendor/DPP/library/Release/*.dll")
    file(GLOB DPP_DLLS_LIB "${CMAKE_BINARY_DIR}/vendor/DPP/library/*.dll")
    file(GLOB SPDLOG_DLLS "${CMAKE_BINARY_DIR}/vendor/spdlog/Release/*.dll")

    # Get OpenSSL DLL paths
    get_filename_component(OPENSSL_ROOT_DIR ${OPENSSL_INCLUDE_DIR} DIRECTORY)
    file(GLOB OPENSSL_DLLS "${OPENSSL_ROOT_DIR}/bin/*.dll")

    # Copy DLLs after build for main bot
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # Copy DPP DLLs from Release directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DPP_DLLS_RELEASE}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"

        # Copy DPP DLLs from library directory
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DPP_DLLS_LIB}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"

        # Copy OpenSSL DLLs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENSSL_DLLS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"

        # Copy spdlog DLLs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SPDLOG_DLLS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    )

    # Copy required DLLs for Whisper service
    add_custom_command(TARGET whisper_service POST_BUILD
        # Copy spdlog DLLs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SPDLOG_DLLS}
            "$<TARGET_FILE_DIR:whisper_service>/"
    )
endif()