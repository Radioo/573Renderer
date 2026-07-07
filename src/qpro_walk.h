#pragma once

#include "qpro_dll.h"
#include "qpro_internal.h"
#include "media_sink.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace QproExtract {
namespace detail {

struct Layer {
    const char* suffix;
    std::vector<const char*> candidates;
    std::vector<const char*> clip_candidates;
};

const std::vector<Layer>& LayersFor(QproDll::Category c);

struct ClipFrames {
    std::vector<std::vector<uint8_t>> frames;
    int cw = 0, ch = 0;
    int fps = 0;
};

constexpr int kBackCropX = 0, kBackCropY = 0, kBackCropW = 342, kBackCropH = 502;
constexpr int kHeadCanvasX = 0, kHeadCanvasY = 0, kHeadCanvasW = 262, kHeadCanvasH = 352;
constexpr int kHairCanvasX = 0, kHairCanvasY = 0, kHairCanvasW = 262, kHairCanvasH = 352;
constexpr int kHandCanvasH = 352, kHandCanvasRW = 212, kHandCanvasLW = kHandCanvasRW;
constexpr int kFaceCanvasW = 150, kFaceCanvasH = 158;

void FixedCanvasFor(QproDll::Category cat, const char* suffix, int& x, int& y, int& w, int& h);

bool CategoryIsFrontBackPair(QproDll::Category cat);

bool CategoryCanvasIsHardCrop(QproDll::Category cat);

const char* PairFrontImage(QproDll::Category cat);

int AfpFpsFromSteps(int steps, int frames, int fallback_fps);
void UnionContentBBox(const std::vector<uint8_t>& bgra, int w, int h, int& x0, int& y0, int& x1,
                      int& y1);
void UnpremultiplyBGRA(std::vector<uint8_t>& bgra);
int SwitchToAnimatedClip(const std::vector<const char*>& clips);

bool CaptureClipFrames(ClipFrames& out, int fps, const char* label, int fcx, int fcy, int fcw,
                       int fch);
int EncodeClipFrames(const std::string& out_path, const ClipFrames& cf, const char* dump_raw_prefix,
                     MediaSink::Format format = MediaSink::Format::AVIF);
std::string QproFormatPath(const std::string& avif_path, MediaSink::Format fmt);

void NormalizeClipToCanvas(ClipFrames& cf, int cw, int ch, bool hard_crop);

bool CaptureLayerStatic(const TexList& tl, int slot0, QproDll::Category cat, const Layer& ly,
                        ClipFrames& out);
bool CaptureLayerViaWalk(const TexList& tl, int slot0, QproDll::Category cat, const Layer& ly,
                         bool composed, ClipFrames& out, bool& is_animated);
bool EmitClipFrames(const ClipFrames& cf, bool anim, const std::string& path,
                    const char* dump_label);
int RenderLayerViaWalk(const TexList& tl, int slot0, QproDll::Category cat, const Layer& ly,
                       bool composed, const std::string& out_path);
bool IsComposedPlaceholder(const TexList& tl, int slot0, QproDll::Category cat);
int RenderPairViaWalk(const TexList& tl, int slot0, QproDll::Category cat, bool composed,
                      const std::function<std::string(const char*)>& out_for);

}
}
