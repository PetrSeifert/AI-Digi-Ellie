# AI-Digi-Ellie

AI-Digi-Ellie is an AI-powered companion. She can operate in two modes:
- Discord mode: Runs as a Discord bot when provided with a Discord token
- Console mode: Runs in console for local testing when no token is provided

## Features

- AI-powered conversation capabilities
- Discord integration using the D++ library
- Supports both Discord bot and console modes

## Requirements

### Build Requirements
- CMake 3.18 or higher
- C++20 compatible compiler
- Git (for cloning the repository)

### Dependencies
The project uses the following libraries (included as submodules):
- [D++](https://github.com/brainboxdotcc/DPP) - A lightweight C++ Discord library
- [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++

## Configuration

Before building the project, you need to modify the configuration in `include/config.hpp`:

1. Set `MODEL_NAME` to the name of your Ollama model (default: "mistral")
2. Set `DEFAULT_CHANNEL_ID` to your Discord channel ID where boot messages will be sent

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

## Running the Bot

### Prerequisites
1. Make sure Ollama is installed and running
2. Ensure your chosen model is downloaded in Ollama (e.g., `ollama pull mistral`)
3. Configure your Discord bot token and enable Message Content Intent in Discord Developer Portal

### Discord Mode
To run as a Discord bot, provide your Discord token as a command line argument:
```bash
./AI-Digi-Ellie YOUR_DISCORD_TOKEN
```

### Console Mode
To run in console mode, simply run the executable without any arguments:
```bash
./AI-Digi-Ellie
```

## Project Structure

- `src/` - Source files
- `include/` - Header files
- `vendor/` - Third-party dependencies
  - `DPP/` - D++ Discord library
  - `json/` - nlohmann/json library

## License

This project is licensed under the terms of the LICENSE file included in the repository. 