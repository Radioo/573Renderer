#pragma once

#include "afp_funcs.h"
#include <cstdint>

namespace McControl {

constexpr uint32_t kProp_Visible = 0x1007;
constexpr uint32_t kProp_Invalidate = 0x101E;

constexpr int kDir_ContainingTimeline = 0;
constexpr int kDir_FirstDescendant = 1;
constexpr int kDir_FirstInTree = 2;
constexpr int kDir_NextInDFS = 3;
constexpr int kDir_PrevInDFS = 4;
constexpr int kDir_LastInTree = 5;
constexpr int kDir_NextSibling = 6;
constexpr int kDir_PrevSibling = 7;

int FindClip(const AfpFuncs& afp, uint32_t stream_id, const char* path);

void SetClipVisible(const AfpFuncs& afp, uint32_t stream_id, const char* path, bool visible);

bool SetClipBitmap(const AfpFuncs& afp, uint32_t stream_id, const char* path,
                   const char* bitmap_name);

struct ImageSlot {
    int slot;
    int w;
    int h;
};

int BindClipImages(const AfpFuncs& afp, uint32_t stream_id, const char* clip_path,
                   const ImageSlot* slots, int n);

void BindImageToMc(const AfpFuncs& afp, int mc_id, const ImageSlot& s);

int ResolveSiblings(const AfpFuncs& afp, uint32_t stream_id, const char* clip_path, int* out_ids,
                    int max);

using ClipVisitor = bool (*)(int mc_id, void* user);
int EnumerateClips(const AfpFuncs& afp, uint32_t stream_id, ClipVisitor visitor, void* user,
                   int max_clips = 10000);

}
