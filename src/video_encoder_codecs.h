#pragma once

#include "video_encoder.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace VideoEncoder {

std::string AvErr(int rc);
void SetSrgbColorMetadata(AVCodecContext* ctx);
int QualityToCRF(int q_user, int lo, int hi);
int KeyframeGop(const Params& p, int floor_frames);

bool OpenLibaom(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                int out_h, AVPixelFormat pix_fmt, bool global_header, std::string& err);
bool OpenAv1Nvenc(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                  int out_h, bool global_header, std::string& err);
bool OpenLibwebpAnim(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                     int out_h, bool global_header, std::string& err);
bool OpenLibvpxVp9(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                   int out_h, bool global_header, std::string& err);
bool OpenH264Nvenc(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                   int out_h, bool global_header, std::string& err);
bool OpenLibx264(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                 int out_h, bool global_header, std::string& err);
bool OpenLibx265Alpha(AVCodecContext*& out_ctx, const AVCodec* codec, const Params& p, int out_w,
                      int out_h, bool global_header, std::string& err);

bool AllocYuvFrame(AVFrame*& f, int w, int h, AVPixelFormat fmt, std::string& err);
SwsContext* MakeSws(int sw, int sh, int ow, int oh, AVPixelFormat target, std::string& err);

}
