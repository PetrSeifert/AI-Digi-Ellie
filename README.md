# AI-Digi-Ellie

AI-Digi-Ellie is an AI-powered companion. She can operate in two modes:
- Discord mode: Runs as a Discord bot when provided with a Discord token
- Console mode: Runs in console for local testing when no token is provided

## Features

- AI-powered conversation capabilities
- Discord integration using the D++ library
- Text-to-Speech support using Azure Cognitive Services
- Speech-to-Text support using Whisper.cpp (local processing)
- Voice chat interaction in Discord
- Supports both Discord bot and console modes

## Requirements

### Build Requirements
- CMake 3.10 or higher
- C++20 compatible compiler
- Git (for cloning the repository)
- OpenSSL 3.0 or higher (for secure connections)
- Opus development libraries (for voice processing)

### Dependencies
The project uses the following libraries (included as submodules):
- [D++](https://github.com/brainboxdotcc/DPP) - A lightweight C++ Discord library
- [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++
- [spdlog](https://github.com/gabime/spdlog) - Fast C++ logging library
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - A C++ HTTP/HTTPS client library
- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) - C++ implementation of OpenAI's Whisper model

## Configuration

The project uses environment variables for configuration. Set the following variables before running:

### Required Environment Variables
- `DIGI_ELLIE_DISCORD_TOKEN` - Your Discord bot token
- `DIGI_ELLIE_DEFAULT_CHANNEL_ID` - Discord channel ID for boot messages

### Optional Environment Variables
- `DIGI_ELLIE_MODEL_NAME` - Ollama model name (default: "mistral")

### TTS Configuration (Azure)
To enable TTS functionality, set these environment variables:
- `DIGI_ELLIE_AZURE_SPEECH_KEY` - Your Azure Cognitive Services subscription key
- `DIGI_ELLIE_AZURE_SPEECH_REGION` - Azure region (default: "germanywestcentral")
- `DIGI_ELLIE_AZURE_SPEECH_VOICE` - Voice to use (default: "en-US-JennyNeural")
- `DIGI_ELLIE_AZURE_SPEECH_APP_NAME` - Application name for Azure requests

### STT Configuration (Whisper)
The Whisper STT service runs as a separate process and can be configured with:
- `DIGI_ELLIE_WHISPER_MODEL_NAME` - Whisper model file name (default: "ggml-large-v3-turbo-q8_0.bin")
- `DIGI_ELLIE_WHISPER_SERVICE_HOST` - Host for the Whisper service (default: "0.0.0.0")
- `DIGI_ELLIE_WHISPER_SERVICE_PORT` - Port for the Whisper service (default: 8000)

## Building the Project

### Linux/macOS

1. Clone the repository with submodules:
```bash
git clone --recursive https://github.com/yourusername/AI-Digi-Ellie.git
cd AI-Digi-Ellie
```

2. Create a build directory:
```bash
mkdir build
```

3. Generate build files with CMake:
```bash
cd build; cmake ..; cd ..
```

4. Build the project:
```bash
cmake --build build --config Release
```

### Windows

1. Clone the repository with submodules:
```cmd
git clone --recursive https://github.com/yourusername/AI-Digi-Ellie.git
cd AI-Digi-Ellie
```

2. Create a build directory:
```cmd
mkdir build
```

3. Generate build files with CMake:
```cmd
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cd ..
```

4. Build the project:
```cmd
cmake --build build --config Release
```

The build process will automatically download the Whisper model file (approximately 4GB) during the first build.

## Prerequisites

### OpenSSL Setup
1. Ensure OpenSSL 3.0 or higher is installed on your system
2. Make sure the OpenSSL development libraries are available
3. On Windows, ensure OpenSSL DLLs are in your system PATH or install OpenSSL using:
   ```cmd
   winget install ShiningLight.OpenSSL
   ```

### Opus Setup
1. Install the Opus development libraries:
   - Ubuntu/Debian: `sudo apt-get install libopus-dev`
   - Fedora/RHEL: `sudo dnf install opus-devel`
   - Arch Linux: `sudo pacman -S opus`
   - Windows: Opus is included with the DPP library

### Ollama Setup
1. Make sure Ollama is installed and running
2. Ensure your chosen model is downloaded in Ollama (e.g., `ollama pull mistral`)
3. Set the `DIGI_ELLIE_OLLAMA_HOST` environment variable to the host of your Ollama server (default: "localhost")

### Discord Setup
1. Create a Discord application and bot in the Discord Developer Portal
2. Enable Message Content Intent in the bot settings
3. Add the bot to your server with appropriate permissions (text, voice)

### Azure Setup (for TTS)
1. Create an Azure account and subscribe to Azure Cognitive Services
2. Create a Speech Service resource in your preferred region
3. Get your subscription key and region from the Azure portal
4. Set the required environment variables for TTS

## Running the Bot

### Environment Setup

#### Linux/macOS
Set the required environment variables:
```bash
export DIGI_ELLIE_DISCORD_TOKEN="your_discord_token"
export DIGI_ELLIE_DEFAULT_CHANNEL_ID="your_channel_id"
export DIGI_ELLIE_AZURE_SPEECH_KEY="your_azure_speech_key"
```

#### Windows
Set the required environment variables:
```cmd
set DIGI_ELLIE_DISCORD_TOKEN=your_discord_token
set DIGI_ELLIE_DEFAULT_CHANNEL_ID=your_channel_id
set DIGI_ELLIE_AZURE_SPEECH_KEY=your_azure_speech_key
```

### Start the Whisper STT Service
The Whisper service needs to be running for voice recognition to work:

#### Linux/macOS
```bash
./build/bin/whisper_service
```

#### Windows
```cmd
.\build\bin\Release\whisper_service.exe
```

You can run the Whisper service on a different machine. If you do, set the service URL in the bot:
```bash
export DIGI_ELLIE_WHISPER_SERVICE_HOST="your_whisper_server_ip"
export DIGI_ELLIE_WHISPER_SERVICE_PORT="your_whisper_server_port"
```

### Start the bot

#### Linux/macOS
```bash
./build/bin/AI-Digi-Ellie
```

#### Windows
```cmd
.\build\bin\Release\AI-Digi-Ellie.exe
```

### Voice Commands
Once the bot is running:
1. Join a voice channel
2. Use the `/joinvoice` command to make the bot join your channel and start recording audio
3. The bot will transcribe the recorded audio using Whisper and respond to you

## Project Structure

- `src/` - Source files
- `include/` - Header files
- `vendor/` - Third-party dependencies
  - `DPP/` - D++ Discord library
  - `json/` - nlohmann/json library
  - `spdlog/` - spdlog logging library
  - `cpp-httplib/` - HTTP/HTTPS client library
  - `whisper.cpp/` - Whisper speech recognition library
- `whisper_models/` - Directory for Whisper model files

## Troubleshooting

### Windows Build Issues
- If you encounter errors about missing DLLs, ensure that OpenSSL is properly installed and in your PATH

### Linux Build Issues
- Make sure all development libraries are installed (OpenSSL, Opus)
- If you encounter permission issues when running the executables, use `chmod +x` to make them executable

## License

This project is licensed under the terms of the LICENSE file included in the repository. 