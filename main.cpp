#include "config.h"
#include "streamer.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>


static std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    auto p = s.find_last_not_of(ws);
    if (p != std::string::npos) s.erase(p + 1); else s.clear();
    return s;
}

static std::string unquote(std::string s) {
    s = trim(s);
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front())
        s = s.substr(1, s.size() - 2);
    return s;
}

static std::vector<std::string> parse_array(std::string v) {
    v = trim(v);
    if (!v.empty() && v.front() == '[') v.erase(0, 1);
    if (!v.empty() && v.back() == ']') v.pop_back();

    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= v.size(); ++i) {
        if (i == v.size() || v[i] == ',') {
            std::string item = unquote(v.substr(start, i - start));
            if (!item.empty()) out.push_back(item);
            start = i + 1;
        }
    }
    return out;
}

Config load_config(const std::string& path) {
    Config c;
    std::ifstream f(path);
    if (!f) { std::fprintf(stderr, "cannot open config: %s\n", path.c_str()); return c; }

    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        auto eq = t.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));  // split on FIRST '=' (URLs keep theirs)

        if (key == "cameracode")      c.camera_codes = parse_array(val);
        else if (key == "rtsps")      c.rtsps = parse_array(val);
        else if (key == "zlm" || key == "zlm_base" || key == "output")
            c.zlm_base = unquote(val);
    }
    return c;
}


static std::atomic<bool> g_run{true};
static void on_signal(int) { g_run.store(false); }

int main(int argc, char** argv) {
    const std::string cfg_path = (argc > 1) ? argv[1] : "config.ini";
    Config cfg = load_config(cfg_path);

    if (cfg.rtsps.empty() || cfg.rtsps.size() != cfg.camera_codes.size()) {
        std::fprintf(stderr,
                     "config error: cameracode(%zu) and rtsps(%zu) must be non-empty and equal\n",
                     cfg.camera_codes.size(), cfg.rtsps.size());
        return 1;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    avformat_network_init();

    std::vector<std::unique_ptr<Streamer>> streamers;
    for (size_t i = 0; i < cfg.rtsps.size(); ++i) {
        std::string out = cfg.zlm_base + "/" + cfg.camera_codes[i];
        auto s = std::make_unique<Streamer>(cfg.camera_codes[i], cfg.rtsps[i], out);
        s->start();
        streamers.push_back(std::move(s));
    }
    std::printf("started %zu streamer(s); Ctrl+C to stop\n", streamers.size());

    while (g_run.load()) std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::printf("\nshutting down...\n");
    for (auto& s : streamers) s->stop();
    for (auto& s : streamers) s->join();

    avformat_network_deinit();
    return 0;
}