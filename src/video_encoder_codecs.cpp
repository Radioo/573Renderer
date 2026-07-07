#include "video_encoder_codecs.h"
#include "video_encoder.h"

#include <algorithm>
#include <cstdio>
#include <string>

extern "C" {
#include <libavutil/error.h>
#include <libavcodec/codec.h>
#include <libavutil/macros.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace VideoEncoder {

std::string AvErr(int rc) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(rc, buf, sizeof(buf));
    return buf;
}

void SetSrgbColorMetadata(AVCodecContext* ctx) {
    ctx->color_primaries = AVCOL_PRI_BT709;
    ctx->color_trc = AVCOL_TRC_IEC61966_2_1;
    ctx->colorspace = AVCOL_SPC_BT709;
    ctx->color_range = AVCOL_RANGE_JPEG;
}

int QualityToCRF(int q_user, int lo, int hi) {
    int crf = hi - (q_user * (hi - lo) / 100);
    crf = std::max(crf, lo);
    crf = std::min(crf, hi);
    return crf;
}

int KeyframeGop(const Params& p, int floor_frames) {
    int const gop = p.keyframe_interval > 0 ? p.keyframe_interval : p.fps;
    return gop < floor_frames ? floor_frames : gop;
}

bool OpenLibaom(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                int out_h, AVPixelFormat pix_fmt, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(libaom)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = pix_fmt;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    int const crf = QualityToCRF(p.quality, 4, 40);
    av_opt_set_int(out_ctx->priv_data, "crf", crf, 0);
    av_opt_set_int(out_ctx->priv_data, "b", 0, 0);
    av_opt_set(out_ctx->priv_data, "usage", "good", 0);
    out_ctx->gop_size = KeyframeGop(p, 1);
    out_ctx->keyint_min = out_ctx->gop_size;
    out_ctx->thread_count = 0;
    out_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    auto tile_log2 = [](int dim) -> int {
        if (dim >= 1024) return 2;
        if (dim >= 512) return 1;
        return 0;
    };
    av_opt_set_int(out_ctx->priv_data, "cpu-used", 4, 0);
    av_opt_set_int(out_ctx->priv_data, "row-mt", 1, 0);
    av_opt_set_int(out_ctx->priv_data, "tile-columns", tile_log2(out_w), 0);
    av_opt_set_int(out_ctx->priv_data, "tile-rows", tile_log2(out_h), 0);
    av_opt_set_int(out_ctx->priv_data, "threads", 16, 0);

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(libaom-av1): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool OpenAv1Nvenc(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                  int out_h, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(nvenc)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    av_opt_set(out_ctx->priv_data, "rc", "constqp", 0);
    int const cqp = QualityToCRF(p.quality, 10, 40);
    av_opt_set_int(out_ctx->priv_data, "qp", cqp, 0);
    av_opt_set(out_ctx->priv_data, "preset", "p5", 0);
    av_opt_set(out_ctx->priv_data, "tune", "hq", 0);

    out_ctx->max_b_frames = 0;
    out_ctx->gop_size = KeyframeGop(p, 2);
    out_ctx->keyint_min = out_ctx->gop_size;

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(av1_nvenc): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool OpenLibwebpAnim(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                     int out_h, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(libwebp_anim)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = AV_PIX_FMT_YUVA420P;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    int const q = std::clamp(p.quality, 0, 100);
    av_opt_set_int(out_ctx->priv_data, "quality", q, 0);
    av_opt_set_int(out_ctx->priv_data, "compression_level", 4, 0);
    av_opt_set_int(out_ctx->priv_data, "loop", 0, 0);

    out_ctx->thread_count = 0;

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(libwebp_anim): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool OpenLibvpxVp9(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                   int out_h, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(libvpx)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = AV_PIX_FMT_YUVA420P;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    int const crf = QualityToCRF(p.quality, 4, 50);
    av_opt_set_int(out_ctx->priv_data, "crf", crf, 0);
    av_opt_set_int(out_ctx->priv_data, "b", 0, 0);
    av_opt_set(out_ctx->priv_data, "deadline", "good", 0);
    av_opt_set_int(out_ctx->priv_data, "cpu-used", 2, 0);
    av_opt_set_int(out_ctx->priv_data, "row-mt", 1, 0);
    av_opt_set_int(out_ctx->priv_data, "tile-columns", 2, 0);
    out_ctx->thread_count = 0;

    out_ctx->gop_size = KeyframeGop(p, 1);
    out_ctx->keyint_min = out_ctx->gop_size;

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(libvpx-vp9): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool OpenH264Nvenc(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                   int out_h, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(h264_nvenc)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    av_opt_set(out_ctx->priv_data, "rc", "constqp", 0);
    int const cqp = QualityToCRF(p.quality, 18, 38);
    av_opt_set_int(out_ctx->priv_data, "qp", cqp, 0);
    av_opt_set(out_ctx->priv_data, "preset", "p5", 0);
    av_opt_set(out_ctx->priv_data, "tune", "hq", 0);
    av_opt_set(out_ctx->priv_data, "profile", "high", 0);

    out_ctx->gop_size = KeyframeGop(p, 1);
    out_ctx->keyint_min = out_ctx->gop_size;

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(h264_nvenc): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool OpenLibx264(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                 int out_h, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(libx264)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    int const crf = QualityToCRF(p.quality, 16, 34);
    av_opt_set_int(out_ctx->priv_data, "crf", crf, 0);
    av_opt_set(out_ctx->priv_data, "preset", "medium", 0);
    av_opt_set(out_ctx->priv_data, "profile", "high", 0);
    out_ctx->gop_size = KeyframeGop(p, 1);
    out_ctx->keyint_min = out_ctx->gop_size;
    out_ctx->thread_count = 0;

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(libx264): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool OpenLibx265Alpha(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                      int out_h, bool global_header, std::string& err) {
    out_ctx = avcodec_alloc_context3(codec);
    if (out_ctx == nullptr) {
        err = "avcodec_alloc_context3(libx265)";
        return false;
    }

    out_ctx->width = out_w;
    out_ctx->height = out_h;
    out_ctx->pix_fmt = AV_PIX_FMT_YUVA420P;
    out_ctx->time_base = AVRational{1, p.fps};
    out_ctx->framerate = AVRational{p.fps, 1};
    out_ctx->codec_tag = MKTAG('h', 'v', 'c', '1');
    if (global_header) out_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    SetSrgbColorMetadata(out_ctx);

    int const crf = QualityToCRF(p.quality, 16, 34);
    char x265p[128];
    snprintf(x265p, sizeof(x265p), "crf=%d:log-level=none", crf);
    av_opt_set(out_ctx->priv_data, "preset", "medium", 0);
    av_opt_set(out_ctx->priv_data, "x265-params", x265p, 0);
    out_ctx->gop_size = KeyframeGop(p, 1);
    out_ctx->keyint_min = out_ctx->gop_size;
    out_ctx->thread_count = 0;

    int const rc = avcodec_open2(out_ctx, codec, nullptr);
    if (rc < 0) {
        err = "avcodec_open2(libx265-alpha): " + AvErr(rc);
        avcodec_free_context(&out_ctx);
        return false;
    }
    return true;
}

bool AllocYuvFrame(AVFrame*& f, int w, int h, AVPixelFormat fmt, std::string& err) {
    f = av_frame_alloc();
    if (f == nullptr) {
        err = "av_frame_alloc";
        return false;
    }
    f->width = w;
    f->height = h;
    f->format = fmt;
    f->color_range = AVCOL_RANGE_JPEG;
    f->color_primaries = AVCOL_PRI_BT709;
    f->color_trc = AVCOL_TRC_IEC61966_2_1;
    f->colorspace = AVCOL_SPC_BT709;
    int const rc = av_frame_get_buffer(f, 32);
    if (rc < 0) {
        err = "av_frame_get_buffer: " + AvErr(rc);
        return false;
    }
    return true;
}

SwsContext* MakeSws(int sw, int sh, int ow, int oh, AVPixelFormat target, std::string& err) {
    SwsContext* sws = sws_getContext(sw, sh, AV_PIX_FMT_BGRA, ow, oh, target, SWS_BICUBIC, nullptr,
                                     nullptr, nullptr);
    if (sws == nullptr) {
        err = "sws_getContext failed";
        return nullptr;
    }
    int table[4];
    const int* def = sws_getCoefficients(SWS_CS_ITU709);
    table[0] = def[0];
    table[1] = def[1];
    table[2] = def[2];
    table[3] = def[3];
    sws_setColorspaceDetails(sws, table, 1, table, 1, 0, 1 << 16, 1 << 16);
    return sws;
}

}
