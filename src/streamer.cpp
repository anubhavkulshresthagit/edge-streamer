#include "streamer.h"
#include "libav.h"

#include <chrono>
#include <cstdio>
#include <utility>

Streamer::Streamer(std::string camera_code, std::string in_url, std::string out_url)
    : camera_(std::move(camera_code)), in_(std::move(in_url)), out_(std::move(out_url)) {}

Streamer::~Streamer() {
    stop();
    join();
}

void Streamer::start() {
    if (running_.exchange(true)) return;  // already running
    thread_ = std::thread(&Streamer::run, this);
}

void Streamer::stop() noexcept { running_.store(false); }

void Streamer::join() {
    if (thread_.joinable()) thread_.join();
}

void Streamer::run() {
    constexpr int kBackoffMs = 2000;

    while (running_.load()) {
        std::printf("[%s] connecting %s -> %s\n", camera_.c_str(), in_.c_str(), out_.c_str());

        int r = relay_rtsp(in_, out_, running_);
        if (!running_.load()) break;  // stopped intentionally, no reconnect

        std::fprintf(stderr, "[%s] stream ended (%d); reconnecting in %dms\n",
                     camera_.c_str(), r, kBackoffMs);

        // Interruptible backoff so stop() is honored promptly.
        for (int slept = 0; slept < kBackoffMs && running_.load(); slept += 100)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::printf("[%s] stopped\n", camera_.c_str());
}