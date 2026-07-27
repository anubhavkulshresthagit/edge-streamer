#pragma once
#include <string>
#include <vector>

struct Config {
    std::vector<std::string> camera_codes;
    std::vector<std::string> rtsps;
    // ZLMediaKit push base: rtsp://<host>:<rtsp_port>/<app>
    // final push url per camera = zlm_base + "/" + camera_code
    std::string zlm_base = "rtsp://127.0.0.1:554/live";
};

// Parses the .ini. Returns a Config; caller validates non-empty / equal sizes.
Config load_config(const std::string& path);