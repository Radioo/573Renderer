#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "formats/bgra_crop.h"
#include "media_sink.h"

struct AfpFuncs;

namespace QproExtract {
namespace detail {

struct ClipFrames;

struct AtlasImage {
    std::string name;
    int atlas = 0;
    int x = 0, y = 0, w = 0, h = 0;
    uint16_t raw[4] = {0, 0, 0, 0};
};

struct TexList {
    std::vector<AtlasImage> images;
    int atlas_count = 0;
};

bool ParseTexturelist(TexList& out, const char* root = "/afp/packages");
const AtlasImage* FindImage(const TexList& tl, const char* name);
bool ScopeHueToImage(const AtlasImage* im, int slot0);
bool ScopeHueToImage2(const AtlasImage* im, int slot0);
std::string IfsPath(const std::string& game_dir, const std::string& ifs);
std::string Stem(const std::string& ifs);
bool ReadPiece(const TexList& tl, int slot0, const char* name, std::vector<uint8_t>& out, int& w,
               int& h);
bool RenderFrame(std::vector<uint8_t>& out, int& w, int& h, bool advance);
int CountWithPrefix(const TexList& tl, const char* prefix);
int RenderClipAvif(const std::string& out_path, int fps, const char* label, int fcx = 0,
                   int fcy = 0, int fcw = 0, int fch = 0);
bool WritePngBGRA(const std::string& path, const uint8_t* bgra, int w, int h);
bool WriteStillAvif(const std::string& path, const uint8_t* bgra, int w, int h, int quality);

constexpr int kQproAvifQuality = 40;
constexpr bool kQproAvifPreferHardware = true;

float ProgressFrac();

extern bool g_hue_scope_enabled;
extern bool g_clip_dump_raw;

}

inline const char* const kAllAvatarLayers[] = {
    "qpro_bg",           "qp_cat_1",       "qp_cat_2",       "qp_cat_3",        "qp_head_f_neutral",
    "qp_head_b_neutral", "qp_hair_f",      "qp_hair_b",      "qp_face_neutral", "qp_body_f",
    "qp_body_b",         "qp_arm_r_upper", "qp_arm_r_lower", "qp_arm_l_upper",  "qp_arm_l_lower",
    "qp_leg_r_upper",    "qp_leg_r_lower", "qp_leg_l_upper", "qp_leg_l_lower",  "qp_hand_l_neutral",
    "qp_hand_r_neutral",
};

int ClipVisualCmdsAfterFrame0(const AfpFuncs& afp, uint32_t mc_id);

bool WriteAnimatedTriple(const std::string& base_avif, const detail::ClipFrames& cf,
                         const char* prefix);

}
