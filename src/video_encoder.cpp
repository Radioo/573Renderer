#include "video_encoder.h"
#include "video_encoder_codecs.h"
#include "media/media_format.h"
#include "support/log.h"

#include <cstdint>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/packet.h>
#include <libavutil/error.h>
#include <libavformat/avio.h>
#include <libavcodec/codec.h>
#include <libavutil/macros.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace VideoEncoder {

struct Encoder::Impl {
    Params params;
    Format format = Format::AVIF;
    bool using_hw = false;

    AVFormatContext* fmt_ctx = nullptr;

    AVCodecContext* enc_color = nullptr;
    AVCodecContext* enc_alpha = nullptr;
    AVStream* stream_color = nullptr;
    AVStream* stream_alpha = nullptr;

    AVFrame* frame_main = nullptr;
    AVFrame* frame_alpha = nullptr;

    SwsContext* sws_main = nullptr;
    SwsContext* sws_alpha = nullptr;

    int src_w = 0, src_h = 0;
    std::vector<uint8_t> alpha_src;
    bool header_written = false;
    bool trailer_written = false;

    int Drain(AVCodecContext* enc, AVStream* st) const {
        while (true) {
            AVPacket* pkt = av_packet_alloc();
            if (pkt == nullptr) return AVERROR(ENOMEM);
            int const r = avcodec_receive_packet(enc, pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
                av_packet_free(&pkt);
                return 0;
            }
            if (r < 0) {
                av_packet_free(&pkt);
                return r;
            }
            av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
            pkt->stream_index = st->index;
            int const rc = av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_free(&pkt);
            if (rc < 0) return rc;
        }
    }

    [[nodiscard]] int DrainAll() const {
        int rc = Drain(enc_color, stream_color);
        if (rc < 0) return rc;
        if (enc_alpha != nullptr) {
            rc = Drain(enc_alpha, stream_alpha);
            if (rc < 0) return rc;
        }
        return 0;
    }

    void Cleanup() {
        if (enc_color != nullptr) avcodec_free_context(&enc_color);
        if (enc_alpha != nullptr) avcodec_free_context(&enc_alpha);
        if (frame_main != nullptr) av_frame_free(&frame_main);
        if (frame_alpha != nullptr) av_frame_free(&frame_alpha);
        if (sws_main != nullptr) {
            sws_freeContext(sws_main);
            sws_main = nullptr;
        }
        if (sws_alpha != nullptr) {
            sws_freeContext(sws_alpha);
            sws_alpha = nullptr;
        }
        if (fmt_ctx != nullptr) {
            if ((fmt_ctx->pb != nullptr) && ((fmt_ctx->oformat->flags & AVFMT_NOFILE) == 0)) {
                avio_closep(&fmt_ctx->pb);
            }
            avformat_free_context(fmt_ctx);
            fmt_ctx = nullptr;
        }
        stream_color = stream_alpha = nullptr;
        alpha_src.clear();
        alpha_src.shrink_to_fit();
    }

    bool NewColorStream(std::string& err) {
        stream_color = avformat_new_stream(fmt_ctx, nullptr);
        if (stream_color == nullptr) {
            err = "avformat_new_stream";
            return false;
        }
        return true;
    }

    bool FinishColorStream(const Params& p, int out_w, int out_h, AVPixelFormat pix,
                           const char* what, std::string& err) {
        int const rc = avcodec_parameters_from_context(stream_color->codecpar, enc_color);
        if (rc < 0) {
            err = std::string("parameters_from_context(") + what + "): " + AvErr(rc);
            return false;
        }
        stream_color->time_base = AVRational{1, p.fps * 1000};
        stream_color->avg_frame_rate = AVRational{p.fps, 1};
        stream_color->r_frame_rate = AVRational{p.fps, 1};
        if (!AllocYuvFrame(frame_main, out_w, out_h, pix, err)) return false;
        sws_main = MakeSws(p.src_width, p.src_height, out_w, out_h, pix, err);
        return sws_main != nullptr;
    }

    using OpenHwFn = bool (*)(AVCodecContext*&, const AVCodec*, const Params&, int, int, bool,
                              std::string&);

    bool TryHwEncoder(const AVCodec* hw, OpenHwFn open_fn, const Params& p, int out_w, int out_h,
                      bool global_header, const char* label) {
        if (hw == nullptr) return false;
        std::string hw_err;
        if (open_fn(enc_color, hw, p, out_w, out_h, global_header, hw_err)) {
            using_hw = true;
            LOG("VideoEncoder", "%s: NVENC yuv420p active", label);
            return true;
        }
        LOG("VideoEncoder", "%s: NVENC unavailable (%s); falling back to software", label,
            hw_err.c_str());
        return false;
    }

    bool OpenAvif(const Params& p, int out_w, int out_h, bool global_header, std::string& err) {
        const AVCodec* nvenc_av1 =
            p.prefer_hardware ? avcodec_find_encoder_by_name("av1_nvenc") : nullptr;
        const AVCodec* libaom = avcodec_find_encoder_by_name("libaom-av1");
        if (libaom == nullptr) {
            err = "libaom-av1 not present in this ffmpeg build";
            return false;
        }

        stream_color = avformat_new_stream(fmt_ctx, nullptr);
        stream_alpha = avformat_new_stream(fmt_ctx, nullptr);
        if ((stream_color == nullptr) || (stream_alpha == nullptr)) {
            err = "avformat_new_stream";
            return false;
        }

        AVPixelFormat color_pix = AV_PIX_FMT_YUV444P;
        bool const opened = TryHwEncoder(nvenc_av1, OpenAv1Nvenc, p, out_w, out_h, global_header,
                                         "AVIF colour (av1_nvenc)");
        if (opened) color_pix = AV_PIX_FMT_YUV420P;
        if (!opened) {
            if (!OpenLibaom(enc_color, libaom, p, out_w, out_h, color_pix, global_header, err))
                return false;
            LOG("VideoEncoder", "AVIF colour: libaom-av1 yuv444p");
        }

        if (!OpenLibaom(enc_alpha, libaom, p, out_w, out_h, AV_PIX_FMT_GRAY8, global_header, err))
            return false;

        if (!FinishColorStream(p, out_w, out_h, color_pix, "color", err)) return false;

        int const rc = avcodec_parameters_from_context(stream_alpha->codecpar, enc_alpha);
        if (rc < 0) {
            err = "parameters_from_context(alpha): " + AvErr(rc);
            return false;
        }
        stream_alpha->time_base = AVRational{1, p.fps * 1000};
        stream_alpha->avg_frame_rate = AVRational{p.fps, 1};
        stream_alpha->r_frame_rate = AVRational{p.fps, 1};

        if (!AllocYuvFrame(frame_alpha, out_w, out_h, AV_PIX_FMT_GRAY8, err)) return false;
        sws_alpha = sws_getContext(p.src_width, p.src_height, AV_PIX_FMT_GRAY8, out_w, out_h,
                                   AV_PIX_FMT_GRAY8, SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (sws_alpha == nullptr) {
            err = "sws_getContext(gray8)";
            return false;
        }
        return true;
    }

    bool OpenWebP(const Params& p, int out_w, int out_h, bool global_header, std::string& err) {
        const AVCodec* libwebp_anim = avcodec_find_encoder_by_name("libwebp_anim");
        if (libwebp_anim == nullptr) {
            err = "libwebp_anim not present in this ffmpeg build";
            return false;
        }
        if (!NewColorStream(err)) return false;
        if (!OpenLibwebpAnim(enc_color, libwebp_anim, p, out_w, out_h, global_header, err))
            return false;
        LOG("VideoEncoder", "WebP: libwebp_anim yuva420p");
        return FinishColorStream(p, out_w, out_h, AV_PIX_FMT_YUVA420P, "webp", err);
    }

    bool OpenWebMVp9(const Params& p, int out_w, int out_h, bool global_header, std::string& err) {
        const AVCodec* libvpx = avcodec_find_encoder_by_name("libvpx-vp9");
        if (libvpx == nullptr) {
            err = "libvpx-vp9 not present in this ffmpeg build";
            return false;
        }
        if (!NewColorStream(err)) return false;
        if (!OpenLibvpxVp9(enc_color, libvpx, p, out_w, out_h, global_header, err)) return false;
        LOG("VideoEncoder", "WebM: libvpx-vp9 yuva420p");
        return FinishColorStream(p, out_w, out_h, AV_PIX_FMT_YUVA420P, "webm-vp9", err);
    }

    bool OpenWebMAv1(const Params& p, int out_w, int out_h, bool global_header, std::string& err) {
        const AVCodec* nvenc_av1 =
            p.prefer_hardware ? avcodec_find_encoder_by_name("av1_nvenc") : nullptr;
        const AVCodec* libaom = avcodec_find_encoder_by_name("libaom-av1");
        if ((libaom == nullptr) && (nvenc_av1 == nullptr)) {
            err = "no AV1 encoder present in this ffmpeg build";
            return false;
        }
        if (!NewColorStream(err)) return false;

        bool const opened = TryHwEncoder(nvenc_av1, OpenAv1Nvenc, p, out_w, out_h, global_header,
                                         "WebM-AV1 (av1_nvenc)");
        if (!opened) {
            if (libaom == nullptr) {
                err = "av1_nvenc failed and libaom-av1 not built in";
                return false;
            }
            if (!OpenLibaom(enc_color, libaom, p, out_w, out_h, AV_PIX_FMT_YUV420P, global_header,
                            err))
                return false;
            LOG("VideoEncoder", "WebM-AV1: libaom-av1 yuv420p (software)");
        }
        return FinishColorStream(p, out_w, out_h, AV_PIX_FMT_YUV420P, "webm-av1", err);
    }

    bool OpenMp4H264(const Params& p, int out_w, int out_h, bool global_header, std::string& err) {
        const AVCodec* nvenc_h264 =
            p.prefer_hardware ? avcodec_find_encoder_by_name("h264_nvenc") : nullptr;
        const AVCodec* libx264 = avcodec_find_encoder_by_name("libx264");
        if ((libx264 == nullptr) && (nvenc_h264 == nullptr)) {
            err = "MP4 H.264 unavailable: this ffmpeg build has no software "
                  "H.264 (libx264/openh264) and hardware (h264_nvenc) is off "
                  "or absent. Enable HW accel on an NVIDIA GPU, or rebuild "
                  "ffmpeg with the x264/openh264 feature.";
            return false;
        }
        if (!NewColorStream(err)) return false;

        bool const opened = TryHwEncoder(nvenc_h264, OpenH264Nvenc, p, out_w, out_h, global_header,
                                         "MP4-H264 (h264_nvenc)");
        if (!opened) {
            if (libx264 == nullptr) {
                err = "MP4 H.264: h264_nvenc unavailable and this ffmpeg "
                      "build has no software H.264 (libx264/openh264). Enable "
                      "HW accel on an NVIDIA GPU, or rebuild ffmpeg with the "
                      "x264/openh264 feature.";
                return false;
            }
            if (!OpenLibx264(enc_color, libx264, p, out_w, out_h, global_header, err)) return false;
            LOG("VideoEncoder", "MP4-H264: libx264 yuv420p (software)");
        }
        return FinishColorStream(p, out_w, out_h, AV_PIX_FMT_YUV420P, "mp4-h264", err);
    }

    bool OpenMp4HevcAlpha(const Params& p, int out_w, int out_h, bool global_header,
                          std::string& err) {
        const AVCodec* libx265 = avcodec_find_encoder_by_name("libx265");
        if (libx265 == nullptr) {
            err = "MP4 HEVC-alpha unavailable: this ffmpeg build has no libx265. "
                  "Rebuild ffmpeg with the x265 feature (the repo ships an x265 "
                  "overlay port that enables it, with ENABLE_ALPHA).";
            return false;
        }
        if (!NewColorStream(err)) return false;
        if (!OpenLibx265Alpha(enc_color, libx265, p, out_w, out_h, global_header, err))
            return false;
        LOG("VideoEncoder", "MP4-HEVC-Alpha: libx265 yuva420p (alpha layer)");
        if (!FinishColorStream(p, out_w, out_h, AV_PIX_FMT_YUVA420P, "hevc-alpha", err))
            return false;
        stream_color->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
        return true;
    }

    bool OpenOutputAndWriteHeader(const Params& p, std::string& err) {
        AVDictionary* mux_opts = nullptr;
        if (p.format == Format::WebP_Anim) {
            av_dict_set_int(&mux_opts, "loop", 0, 0);
        } else if (p.format == Format::MP4_H264 || p.format == Format::MP4_HEVC_Alpha) {
            av_dict_set(&mux_opts, "movflags", "+faststart", 0);
        }
        if ((fmt_ctx->oformat->flags & AVFMT_NOFILE) == 0) {
            int const rc = avio_open(&fmt_ctx->pb, p.output_path.c_str(), AVIO_FLAG_WRITE);
            if (rc < 0) {
                av_dict_free(&mux_opts);
                err = "avio_open('" + p.output_path + "'): " + AvErr(rc);
                return false;
            }
        }
        int const rc = avformat_write_header(fmt_ctx, &mux_opts);
        av_dict_free(&mux_opts);
        if (rc < 0) {
            err = "avformat_write_header: " + AvErr(rc);
            return false;
        }
        header_written = true;
        return true;
    }
};

Encoder::Encoder() : impl_(std::make_unique<Impl>()) {}
Encoder::~Encoder() {
    try {
        if (impl_) impl_->Cleanup();
    } catch (...) {
        LOG("VideoEncoder", "~Encoder: Cleanup threw, ignoring");
    }
}
Encoder::Encoder(Encoder&&) noexcept = default;
Encoder& Encoder::operator=(Encoder&& other) noexcept {
    if (this != &other) {
        try {
            if (impl_) impl_->Cleanup();
        } catch (...) {
            LOG("VideoEncoder", "move-assign: Cleanup threw, ignoring");
        }
        impl_ = std::move(other.impl_);
        err_ = std::move(other.err_);
    }
    return *this;
}

bool Encoder::UsingHardware() const {
    return impl_ && impl_->using_hw;
}

bool Encoder::Create(const Params& raw) {
    Params fixed = raw;
    if (fixed.format == Format::PNG_Sequence) fixed.format = Format::AVIF;
    const Params& p = fixed;
    err_.clear();
    impl_->params = p;
    impl_->format = p.format;
    impl_->src_w = p.src_width;
    impl_->src_h = p.src_height;

    int out_w = p.out_width > 0 ? p.out_width : p.src_width;
    int out_h = p.out_height > 0 ? p.out_height : p.src_height;
    if (p.format == Format::MP4_H264 || p.format == Format::MP4_HEVC_Alpha) {
        out_w &= ~1;
        out_h &= ~1;
    }

    const char* muxer_name = MediaSink::FormatMuxer(p.format);
    int const rc =
        avformat_alloc_output_context2(&impl_->fmt_ctx, nullptr, muxer_name, p.output_path.c_str());
    if (rc < 0 || (impl_->fmt_ctx == nullptr)) {
        err_ = std::string("avformat_alloc_output_context2(") + muxer_name + "): " + AvErr(rc);
        return false;
    }

    const bool global_header = (impl_->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) != 0;

    bool opened = false;
    switch (p.format) {
    case Format::AVIF:
    case Format::PNG_Sequence:
        opened = impl_->OpenAvif(p, out_w, out_h, global_header, err_);
        break;
    case Format::WebP_Anim:
        opened = impl_->OpenWebP(p, out_w, out_h, global_header, err_);
        break;
    case Format::WebM_VP9:
        opened = impl_->OpenWebMVp9(p, out_w, out_h, global_header, err_);
        break;
    case Format::WebM_AV1:
        opened = impl_->OpenWebMAv1(p, out_w, out_h, global_header, err_);
        break;
    case Format::MP4_H264:
        opened = impl_->OpenMp4H264(p, out_w, out_h, global_header, err_);
        break;
    case Format::MP4_HEVC_Alpha:
        opened = impl_->OpenMp4HevcAlpha(p, out_w, out_h, global_header, err_);
        break;
    }
    if (!opened) return false;

    if (!impl_->OpenOutputAndWriteHeader(p, err_)) return false;

    LOG("VideoEncoder", "created: src %dx%d -> out %dx%d @ %d fps, q=%d, fmt=%s%s", p.src_width,
        p.src_height, out_w, out_h, p.fps, p.quality, MediaSink::FormatShortLabel(p.format),
        impl_->using_hw ? " [HW NVENC]" : " [SW]");
    return true;
}

namespace {

void ExtractAlphaPlane(const uint8_t* bgra, int sw, int sh, std::vector<uint8_t>& alpha_src) {
    alpha_src.resize((size_t)sw * sh);
    uint8_t* a_src = alpha_src.data();
    for (int y = 0; y < sh; y++) {
        const uint8_t* srow = bgra + ((size_t)y * sw * 4);
        uint8_t* drow = a_src + ((size_t)y * sw);
        for (int x = 0; x < sw; x++)
            drow[x] = srow[(x * 4) + 3];
    }
}

}

bool Encoder::SubmitFrame(const uint8_t* bgra, int frame_index) {
    if (impl_->fmt_ctx == nullptr) {
        err_ = "SubmitFrame on closed encoder";
        return false;
    }

    const int sw = impl_->src_w;
    const int sh = impl_->src_h;

    int rc = av_frame_make_writable(impl_->frame_main);
    if (rc < 0) {
        err_ = "av_frame_make_writable(main): " + AvErr(rc);
        return false;
    }

    const uint8_t* src_slices[1] = {bgra};
    int src_strides[1] = {sw * 4};
    sws_scale(impl_->sws_main, src_slices, src_strides, 0, sh, impl_->frame_main->data,
              impl_->frame_main->linesize);
    impl_->frame_main->pts = frame_index;

    if (impl_->format == Format::AVIF) {
        rc = av_frame_make_writable(impl_->frame_alpha);
        if (rc < 0) {
            err_ = "av_frame_make_writable(alpha): " + AvErr(rc);
            return false;
        }

        ExtractAlphaPlane(bgra, sw, sh, impl_->alpha_src);
        const uint8_t* a_slices[1] = {impl_->alpha_src.data()};
        int a_strides[1] = {sw};
        sws_scale(impl_->sws_alpha, a_slices, a_strides, 0, sh, impl_->frame_alpha->data,
                  impl_->frame_alpha->linesize);
        impl_->frame_alpha->pts = frame_index;

        rc = avcodec_send_frame(impl_->enc_color, impl_->frame_main);
        if (rc < 0) {
            err_ = "send_frame(color): " + AvErr(rc);
            return false;
        }
        rc = avcodec_send_frame(impl_->enc_alpha, impl_->frame_alpha);
        if (rc < 0) {
            err_ = "send_frame(alpha): " + AvErr(rc);
            return false;
        }
    } else {
        rc = avcodec_send_frame(impl_->enc_color, impl_->frame_main);
        if (rc < 0) {
            err_ = "send_frame: " + AvErr(rc);
            return false;
        }
    }

    rc = impl_->DrainAll();
    if (rc < 0) {
        err_ = "drain after SubmitFrame: " + AvErr(rc);
        return false;
    }
    return true;
}

bool Encoder::Finish() {
    if ((impl_->fmt_ctx == nullptr) || impl_->trailer_written) return true;

    int rc = avcodec_send_frame(impl_->enc_color, nullptr);
    if (rc < 0) {
        err_ = "flush send_frame(color): " + AvErr(rc);
        return false;
    }
    if (impl_->enc_alpha != nullptr) {
        rc = avcodec_send_frame(impl_->enc_alpha, nullptr);
        if (rc < 0) {
            err_ = "flush send_frame(alpha): " + AvErr(rc);
            return false;
        }
    }
    rc = impl_->DrainAll();
    if (rc < 0) {
        err_ = "flush drain: " + AvErr(rc);
        return false;
    }

    rc = av_write_trailer(impl_->fmt_ctx);
    if (rc < 0) {
        err_ = "av_write_trailer: " + AvErr(rc);
        return false;
    }
    impl_->trailer_written = true;

    LOG("VideoEncoder", "finished: %s", impl_->params.output_path.c_str());
    return true;
}

void Encoder::Cancel() {
    if (impl_->fmt_ctx == nullptr) return;
    impl_->Cleanup();
}

namespace {
bool ProbeNvencEncoder(const char* name) {
    const AVCodec* c = avcodec_find_encoder_by_name(name);
    if (c == nullptr) return false;
    AVCodecContext* ctx = avcodec_alloc_context3(c);
    if (ctx == nullptr) return false;
    ctx->width = 256;
    ctx->height = 256;
    ctx->time_base = AVRational{1, 30};
    ctx->framerate = AVRational{30, 1};
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "p5", 0);
    const int saved_level = av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
    int const rc = avcodec_open2(ctx, c, &opts);
    av_log_set_level(saved_level);
    av_dict_free(&opts);
    avcodec_free_context(&ctx);
    return rc >= 0;
}
}

bool HardwareAvailable(Format f) {
    static const bool av1_ok = ProbeNvencEncoder("av1_nvenc");
    static const bool h264_ok = ProbeNvencEncoder("h264_nvenc");
    return (f == Format::MP4_H264) ? h264_ok : av1_ok;
}

}
