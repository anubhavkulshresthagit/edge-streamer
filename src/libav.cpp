#include "libav.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#include <cstdio>
#include <vector>

namespace {

// Abort a blocked open/read/write after this long (protects against dead links).
constexpr int64_t kIoTimeoutUs = 8'000'000;

void log_err(const char* what, int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    std::fprintf(stderr, "[libav] %s: %s\n", what, buf);
}

// libav polls this during blocking I/O; return non-zero to abort.
struct InterruptCtx {
    const std::atomic<bool>* running;
    int64_t deadline_us;  // 0 = disabled
};
int interrupt_cb(void* p) {
    auto* c = static_cast<InterruptCtx*>(p);
    if (!c->running->load()) return 1;
    if (c->deadline_us && av_gettime_relative() > c->deadline_us) return 1;
    return 0;
}

}  // namespace

int relay_rtsp(const std::string& in_url, const std::string& out_url,
               const std::atomic<bool>& running) {
    InterruptCtx ictx{&running, 0};

    // ---- input ----
    AVFormatContext* in = avformat_alloc_context();
    if (!in) return AVERROR(ENOMEM);
    in->interrupt_callback = {interrupt_cb, &ictx};

    AVDictionary* iopts = nullptr;
    av_dict_set(&iopts, "rtsp_transport", "tcp", 0);   // pull over TCP (no UDP loss)
    av_dict_set(&iopts, "stimeout", "5000000", 0);     // socket read timeout (us)
    av_dict_set(&iopts, "max_delay", "500000", 0);
    av_dict_set(&iopts, "fflags", "nobuffer", 0);      // lower latency
    // Do NOT set "timeout" on the RTSP demuxer: on modern FFmpeg it means
    // "listen timeout" and implies server/listen mode, so the open tries to
    // bind the camera address instead of connecting to it ("Cannot assign
    // requested address"). Hard timeouts are enforced by the interrupt
    // callback (kIoTimeoutUs) instead.

    ictx.deadline_us = av_gettime_relative() + kIoTimeoutUs;
    int ret = avformat_open_input(&in, in_url.c_str(), nullptr, &iopts);
    av_dict_free(&iopts);
    if (ret < 0) { log_err("open input", ret); avformat_close_input(&in); return ret; }

    ictx.deadline_us = av_gettime_relative() + kIoTimeoutUs;
    ret = avformat_find_stream_info(in, nullptr);
    if (ret < 0) { log_err("find_stream_info", ret); avformat_close_input(&in); return ret; }

    // ---- output (rtsp push to ZLMediaKit) ----
    AVFormatContext* out = nullptr;
    avformat_alloc_output_context2(&out, nullptr, "rtsp", out_url.c_str());
    if (!out) { avformat_close_input(&in); return AVERROR_UNKNOWN; }
    out->interrupt_callback = {interrupt_cb, &ictx};

    // Map video streams and copy codec params (no re-encode).
    // Video-only, matching `-an`. To also relay audio, change the condition to:
    //   if (par->codec_type != AVMEDIA_TYPE_VIDEO &&
    //       par->codec_type != AVMEDIA_TYPE_AUDIO) continue;
    std::vector<int> smap(in->nb_streams, -1);
    int oidx = 0;
    for (unsigned i = 0; i < in->nb_streams; ++i) {
        AVCodecParameters* par = in->streams[i]->codecpar;
        if (par->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;

        AVStream* os = avformat_new_stream(out, nullptr);
        if (!os) { avformat_free_context(out); avformat_close_input(&in); return AVERROR(ENOMEM); }
        ret = avcodec_parameters_copy(os->codecpar, par);
        if (ret < 0) { avformat_free_context(out); avformat_close_input(&in); return ret; }
        os->codecpar->codec_tag = 0;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO)
            std::printf("[libav] %s <- %s\n", out_url.c_str(), avcodec_get_name(par->codec_id));
        smap[i] = oidx++;
    }

    AVDictionary* oopts = nullptr;
    av_dict_set(&oopts, "rtsp_transport", "tcp", 0);
    av_dict_set(&oopts, "muxdelay", "0.1", 0);
    ictx.deadline_us = av_gettime_relative() + kIoTimeoutUs;
    ret = avformat_write_header(out, &oopts);
    av_dict_free(&oopts);
    if (ret < 0) { log_err("write_header", ret); avformat_free_context(out); avformat_close_input(&in); return ret; }

    // ---- pump packets ----
    AVPacket* pkt = av_packet_alloc();
    while (running.load()) {
        ictx.deadline_us = av_gettime_relative() + kIoTimeoutUs;
        ret = av_read_frame(in, pkt);
        if (ret < 0) { if (ret != AVERROR_EOF) log_err("read_frame", ret); break; }

        int oi = smap[pkt->stream_index];
        if (oi < 0) { av_packet_unref(pkt); continue; }

        AVStream* is = in->streams[pkt->stream_index];
        AVStream* os = out->streams[oi];
        pkt->stream_index = oi;
        av_packet_rescale_ts(pkt, is->time_base, os->time_base);
        pkt->pos = -1;

        ictx.deadline_us = av_gettime_relative() + kIoTimeoutUs;
        int w = av_interleaved_write_frame(out, pkt);  // takes ownership of pkt refs
        if (w < 0) { log_err("write_frame", w); ret = w; break; }
    }
    av_packet_free(&pkt);

    ictx.deadline_us = av_gettime_relative() + 2'000'000;  // allow a quick TEARDOWN
    av_write_trailer(out);
    avformat_free_context(out);
    avformat_close_input(&in);

    return (ret == AVERROR_EOF) ? 0 : ret;
}