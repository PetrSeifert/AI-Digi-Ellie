#include "whisper_service.hpp"
#include "logging.hpp"
#include <nlohmann/json.hpp>
#include <random>

using json = nlohmann::json;

WhisperService::WhisperService(const std::string& model_path, const std::string& host, int port)
    : host(host), port(port), running(false) {
    
    whisper = std::make_unique<WhisperSTT>(model_path);
    server = std::make_unique<httplib::Server>();
    
    setupRoutes();
}

WhisperService::~WhisperService() {
    stop();
}

void WhisperService::setupRoutes() {
    // Health check endpoint
    server->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });

    // Transcription endpoint
    server->Post("/transcribe", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_header("Content-Type") || req.get_header_value("Content-Type") != "audio/raw") {
            res.status = 400;
            json error = {{"error", "Invalid content type. Expected audio/raw"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            // Convert body to vector of bytes
            std::vector<uint8_t> audio_data(req.body.begin(), req.body.end());

            LOG_INFO("Received audio data of size: {}", audio_data.size());
            
            // Process the audio
            std::string transcription = handleTranscription(audio_data);
            
            // Return the result
            json response = {
                {"text", transcription}
            };
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing transcription request: {}", e.what());
            res.status = 500;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // Streaming: start session
    server->Post("/stream/start", [this](const httplib::Request& req, httplib::Response& res) {
        // Generate a simple random session id
        static const char* alphabet = "0123456789abcdef";
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> dist(0, 15);
        std::string sid(32, '0');
        for (char& c : sid) c = alphabet[dist(rng)];

        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions[sid] = {};
        }

        json response = { {"session_id", sid} };
        res.set_content(response.dump(), "application/json");
    });

    // Streaming: append chunk
    server->Post("/stream/chunk", [this](const httplib::Request& req, httplib::Response& res) {
        auto it = req.headers.find("X-Session-Id");
        if (it == req.headers.end()) {
            res.status = 400;
            json error = {{"error", "Missing X-Session-Id header"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        const std::string& sid = it->second;

        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto sit = sessions.find(sid);
        if (sit == sessions.end()) {
            res.status = 404;
            json error = {{"error", "Session not found"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        auto& buffer = sit->second;
        buffer.insert(buffer.end(), req.body.begin(), req.body.end());

        // Optional: provide partial transcription for real-time feedback
        std::string partial_text;
        try {
            // To avoid long blocking, only attempt partial if buffer is reasonably sized
            if (buffer.size() >= 194000) {
                std::vector<uint8_t> tail;
                // Take last ~0.5s worth of 48k mono data (~48000 bytes for 16-bit)
                size_t span = std::min<size_t>(buffer.size(), 194000);
                tail.insert(tail.end(), buffer.end() - span, buffer.end());

                std::lock_guard<std::mutex> wlock(whisper_mutex);
                partial_text = handleTranscription(tail);
            }
        } catch (const std::exception& e) {
            LOG_WARN("Partial transcription failed: {}", e.what());
        }

        json response = { {"partial", partial_text} };
        res.set_content(response.dump(), "application/json");
    });

    // Streaming: finish and transcribe
    server->Post("/stream/finish", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string sid = body.value("session_id", "");
            if (sid.empty()) {
                res.status = 400;
                json error = {{"error", "Missing session_id"}};
                res.set_content(error.dump(), "application/json");
                return;
            }

            std::vector<uint8_t> data;
            {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                auto it = sessions.find(sid);
                if (it == sessions.end()) {
                    res.status = 404;
                    json error = {{"error", "Session not found"}};
                    res.set_content(error.dump(), "application/json");
                    return;
                }
                data = std::move(it->second);
                sessions.erase(it);
            }

            std::string transcription = handleTranscription(data);
            json response = { {"text", transcription} };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            LOG_ERROR("Error finishing stream: {}", e.what());
            res.status = 500;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });
}

std::string WhisperService::handleTranscription(const std::vector<uint8_t>& audio_data) {
    return whisper->audioToText(audio_data);
}

void WhisperService::start() {
    if (!running) {
        running = true;
        LOG_INFO("Starting Whisper service on {}:{}", host, port);
        if (!server->listen(host.c_str(), port)) {
            running = false;
            throw std::runtime_error("Failed to start Whisper service");
        }
    }
}

void WhisperService::stop() {
    if (running) {
        server->stop();
        running = false;
        LOG_INFO("Whisper service stopped");
    }
} 