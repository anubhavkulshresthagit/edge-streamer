#pragma once
#include <atomic>
#include <string>
#include <thread>

// Owns one input->output relay, running on its own thread with auto-reconnect.
class Streamer {
public:
    Streamer(std::string camera_code, std::string in_url, std::string out_url);
    ~Streamer();

    Streamer(const Streamer&) = delete;
    Streamer& operator=(const Streamer&) = delete;

    void start();          // spawn worker thread
    void stop() noexcept;  // signal shutdown (non-blocking)
    void join();           // wait for worker to finish

private:
    void run();

    std::string camera_, in_, out_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};