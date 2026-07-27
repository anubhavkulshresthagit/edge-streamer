#pragma once
#include <atomic>
#include <string>

// Remuxes (stream-copies) one RTSP input to one RTSP output. No decode/encode,
// so H.264 and H.265 are both relayed as-is at minimal CPU cost.
//
// Blocks until the source ends, an error occurs, or `running` turns false.
// Returns 0 on clean end / EOF, a negative AVERROR on failure. The caller
// (Streamer) decides whether to reconnect based on the return value.
int relay_rtsp(const std::string& in_url,
               const std::string& out_url,
               const std::atomic<bool>& running);