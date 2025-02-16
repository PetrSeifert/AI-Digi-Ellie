# AI-Digi-Ellie

AI-Digi-Ellie is an AI-powered companion. She can operate in two modes:
- Discord mode: Runs as a Discord bot when provided with a Discord token
- Console mode: Runs in console for local testing when no token is provided

## Features

- AI-powered conversation capabilities
- Discord integration using the D++ library
- Text-to-Speech support using Azure Cognitive Services
- Supports both Discord bot and console modes

## Requirements

### Build Requirements
- CMake 3.18 or higher
- C++20 compatible compiler
- Git (for cloning the repository)
- OpenSSL (for secure connections)

### Dependencies
The project uses the following libraries (included as submodules):
- [D++](https://github.com/brainboxdotcc/DPP) - A lightweight C++ Discord library
- [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++

## Configuration

The project uses environment variables for configuration. Set the following variables before running:

### Required Environment Variables
- `DIGI_ELLIE_DISCORD_TOKEN` - Your Discord bot token
- `DIGI_ELLIE_DEFAULT_CHANNEL_ID` - Discord channel ID for boot messages

### Optional Environment Variables
- `DIGI_ELLIE_MODEL_NAME` - Ollama model name (default: "mistral")

### Text-to-Speech Configuration
To enable TTS functionality, set these environment variables:
- `DIGI_ELLIE_AZURE_TTS_KEY` - Your Azure Cognitive Services subscription key
- `DIGI_ELLIE_AZURE_TTS_REGION` - Azure region (default: "germanywestcentral")
- `DIGI_ELLIE_AZURE_TTS_VOICE` - Voice to use (default: "en-US-JennyNeural")
- `DIGI_ELLIE_AZURE_TTS_APP_NAME` - Application name for Azure requests

## Building the Project

1. Clone the repository:
```bash
git clone https://github.com/yourusername/AI-Digi-Ellie.git
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

## Prerequisites

### Ollama Setup
1. Make sure Ollama is installed and running
2. Ensure your chosen model is downloaded in Ollama (e.g., `ollama pull mistral`)

### Discord Setup
1. Create a Discord application and bot in the Discord Developer Portal
2. Enable Message Content Intent in the bot settings
3. Add the bot to your server with appropriate permissions (text, voice)

### Azure TTS Setup
1. Create an Azure account and subscribe to Azure Cognitive Services
2. Create a Speech Service resource in your preferred region
3. Get your subscription key and region from the Azure portal
4. Set the required environment variables

## Running the Bot

### Environment Setup
Set the required environment variables:
```bash
export DIGI_ELLIE_DISCORD_TOKEN="your_discord_token"
export DIGI_ELLIE_DEFAULT_CHANNEL_ID="your_channel_id"
```

### Discord Mode
To run as a Discord bot:
```bash
./AI-Digi-Ellie
```

### Voice Commands
Once the bot is running:
1. Join a voice channel
2. Use the `/joinvoice` command to make the bot join your channel
3. The bot will now speak its responses using TTS

## Project Structure

- `src/` - Source files
- `include/` - Header files
- `vendor/` - Third-party dependencies
  - `DPP/` - D++ Discord library
  - `json/` - nlohmann/json library

## License

This project is licensed under the terms of the LICENSE file included in the repository. 