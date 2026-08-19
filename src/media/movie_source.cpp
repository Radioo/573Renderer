#include "media/movie_source.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

namespace Movie {

namespace {

constexpr int kMaxDecodeSteps = 4096;

std::string ErrorText(int code) {
    std::string text(AV_ERROR_MAX_STRING_SIZE, '\0');
    av_strerror(code, text.data(), text.size());
    text.resize(text.find('\0') == std::string::npos ? text.size() : text.find('\0'));
    return text;
}

}

struct Source::Impl {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* dec = nullptr;
    SwsContext* sws = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    int stream = -1;
    int width = 0;
    int height = 0;
    int out_width = 0;
    int out_height = 0;
    double fps = 0.0;
    int current = -1;
    bool ended = false;
    bool broken = false;
    std::vector<std::uint8_t> bgra;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
    ~Impl() { Free(); }

    void Free() {
        if (sws != nullptr) sws_freeContext(sws);
        sws = nullptr;
        if (frame != nullptr) av_frame_free(&frame);
        if (packet != nullptr) av_packet_free(&packet);
        if (dec != nullptr) avcodec_free_context(&dec);
        if (fmt != nullptr) avformat_close_input(&fmt);
        stream = -1;
        width = 0;
        height = 0;
        fps = 0.0;
        current = -1;
        ended = false;
        broken = false;
        bgra.clear();
    }

    bool Store() {
        bgra.assign((std::size_t)out_width * (std::size_t)out_height * 4, 0);
        sws = sws_getCachedContext(sws, width, height, (AVPixelFormat)frame->format, out_width,
                                   out_height, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr,
                                   nullptr);
        if (sws == nullptr) {
            broken = true;
            return false;
        }
        const std::span<std::uint8_t* const> source(frame->data);
        const std::span<const int> source_stride(frame->linesize);
        std::array<const std::uint8_t*, 4> src = {};
        std::array<int, 4> src_stride = {};
        std::copy_n(source.begin(), src.size(), src.begin());
        std::copy_n(source_stride.begin(), src_stride.size(), src_stride.begin());
        const std::array<std::uint8_t*, 4> planes = {bgra.data(), nullptr, nullptr, nullptr};
        const std::array<int, 4> strides = {out_width * 4, 0, 0, 0};
        sws_scale(sws, src.data(), src_stride.data(), 0, height, planes.data(), strides.data());
        current++;
        return true;
    }

    bool Receive() {
        const int rc = avcodec_receive_frame(dec, frame);
        if (rc < 0) return false;
        return Store();
    }

    bool DecodeNext() {
        if (ended || broken) return false;
        if (Receive()) return true;
        if (broken) return false;
        while (av_read_frame(fmt, packet) >= 0) {
            const bool wanted = packet->stream_index == stream;
            const int rc = wanted ? avcodec_send_packet(dec, packet) : 0;
            av_packet_unref(packet);
            if (!wanted || rc < 0) continue;
            if (Receive()) return true;
            if (broken) return false;
        }
        avcodec_send_packet(dec, nullptr);
        if (Receive()) return true;
        if (broken) return false;
        ended = true;
        return false;
    }

    void Rewind() {
        av_seek_frame(fmt, stream, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(dec);
        current = -1;
        ended = false;
    }
};

Source::Source() = default;

Source::~Source() = default;

Source::Source(Source&&) noexcept = default;

Source& Source::operator=(Source&&) noexcept = default;

void Source::Close() {
    impl_.reset();
}

bool Source::IsOpen() const {
    return impl_ != nullptr && impl_->dec != nullptr;
}

double Source::FrameRate() const {
    return IsOpen() ? impl_->fps : 0.0;
}

int Source::IndexAt(double seconds) const {
    if (!IsOpen() || impl_->fps <= 0.0) return 0;
    const double index = std::floor(seconds * impl_->fps);
    if (index <= 0.0) return 0;
    return (int)index;
}

int Source::Position() const {
    return IsOpen() ? impl_->current : -1;
}

bool Source::Broken() const {
    return impl_ != nullptr && impl_->broken;
}

bool Source::Open(const std::string& path) {
    Close();
    err_.clear();
    auto impl = std::make_unique<Impl>();
    int rc = avformat_open_input(&impl->fmt, path.c_str(), nullptr, nullptr);
    if (rc < 0) {
        err_ = "cannot open " + path + ": " + ErrorText(rc);
        return false;
    }
    rc = avformat_find_stream_info(impl->fmt, nullptr);
    if (rc < 0) {
        err_ = path + " has no readable stream info: " + ErrorText(rc);
        return false;
    }
    const AVCodec* codec = nullptr;
    impl->stream = av_find_best_stream(impl->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (impl->stream < 0 || codec == nullptr) {
        err_ = path + " carries no decodable video stream";
        return false;
    }
    const std::span<AVStream*> streams(impl->fmt->streams, impl->fmt->nb_streams);
    AVStream* stream = streams[(std::size_t)impl->stream];
    impl->dec = avcodec_alloc_context3(codec);
    if (impl->dec == nullptr) {
        err_ = "out of memory opening " + path;
        return false;
    }
    rc = avcodec_parameters_to_context(impl->dec, stream->codecpar);
    if (rc >= 0) rc = avcodec_open2(impl->dec, codec, nullptr);
    if (rc < 0) {
        err_ = "cannot start the " + std::string(codec->name) + " decoder for " + path + ": " +
               ErrorText(rc);
        return false;
    }
    impl->frame = av_frame_alloc();
    impl->packet = av_packet_alloc();
    if (impl->frame == nullptr || impl->packet == nullptr) {
        err_ = "out of memory opening " + path;
        return false;
    }
    impl->width = impl->dec->width;
    impl->height = impl->dec->height;
    if (impl->width <= 0 || impl->height <= 0) {
        err_ = path + " declares no picture size";
        return false;
    }
    impl->out_width = impl->width;
    impl->out_height = impl->height;
    const AVRational rate = av_guess_frame_rate(impl->fmt, stream, nullptr);
    impl->fps = (rate.den > 0 && rate.num > 0) ? av_q2d(rate) : 0.0;
    if (impl->fps <= 0.0) impl->fps = 30.0;
    impl_ = std::move(impl);
    return true;
}

void Source::SetOutputSize(int width, int height) {
    if (!IsOpen() || width <= 0 || height <= 0) return;
    if (impl_->out_width == width && impl_->out_height == height) return;
    impl_->out_width = width;
    impl_->out_height = height;
    impl_->Rewind();
}

Frame Source::FrameAt(int index, const StepFn& on_step) {
    if (!IsOpen()) return {};
    const int wanted = std::max(0, index);
    if (wanted < impl_->current) impl_->Rewind();
    for (int step = 0; impl_->current < wanted && step < kMaxDecodeSteps; step++) {
        if (!impl_->DecodeNext()) break;
        if (on_step) on_step(impl_->current, wanted);
    }
    if (impl_->broken || impl_->current < 0) return {};
    return Frame{
        .width = impl_->out_width, .height = impl_->out_height, .bgra = impl_->bgra.data()};
}

}
