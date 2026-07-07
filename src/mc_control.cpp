#include "mc_control.h"
#include "afp_funcs.h"
#include <cstdint>
#include <cstring>

namespace McControl {

int FindClip(const AfpFuncs& afp, uint32_t stream_id, const char* path) {
    if ((afp.afp_mc_get_id_by_path == nullptr) || (path == nullptr)) return -1;
    return afp.afp_mc_get_id_by_path(stream_id, path);
}

void SetClipVisible(const AfpFuncs& afp, uint32_t stream_id, const char* path, bool visible) {
    if ((afp.afp_mc_get == nullptr) || (afp.afp_mc_get_relative_id == nullptr)) return;
    int id = FindClip(afp, stream_id, path);
    if (id < 0) return;

    const int v = visible ? 1 : 0;
    int count = 0;
    for (; id >= 0 && count < 64; id = afp.afp_mc_get_relative_id(id, kDir_NextSibling)) {
        afp.afp_mc_get(id, kProp_Visible, v);
        afp.afp_mc_get(id, kProp_Invalidate, 1);
        count++;
    }
}

bool SetClipBitmap(const AfpFuncs& afp, uint32_t stream_id, const char* path,
                   const char* bitmap_name) {
    if ((afp.afp_play_work_load_bitmap == nullptr) || (afp.afp_mc_get_relative_id == nullptr) ||
        (afp.afp_mc_get == nullptr) || (bitmap_name == nullptr))
        return false;
    int id = FindClip(afp, stream_id, path);
    if (id < 0) return false;

    int count = 0;
    for (; id >= 0 && count < 64; id = afp.afp_mc_get_relative_id(id, kDir_NextSibling)) {
        afp.afp_play_work_load_bitmap(id, bitmap_name, 0);
        afp.afp_mc_get(id, kProp_Invalidate, 1);
        count++;
    }
    return count > 0;
}

void BindImageToMc(const AfpFuncs& afp, int mc_id, const ImageSlot& s) {
    if ((afp.afp_play_work_load_image == nullptr) || mc_id < 0 || s.slot < 1) return;
    uint8_t info[32] = {};
    auto texid = (uint32_t)(s.slot - 1);
    auto w16 = (uint16_t)s.w;
    auto h16 = (uint16_t)s.h;
    float u0 = 0.0F;
    float u1 = 1.0F;
    float v0 = 0.0F;
    float v1 = 1.0F;
    std::memcpy(info + 0, &texid, 4);
    std::memcpy(info + 4, &w16, 2);
    std::memcpy(info + 8, &h16, 2);
    std::memcpy(info + 12, &u0, 4);
    std::memcpy(info + 16, &u1, 4);
    std::memcpy(info + 20, &v0, 4);
    std::memcpy(info + 24, &v1, 4);
    afp.afp_play_work_load_image(mc_id, info, 0);
    if (afp.afp_mc_get != nullptr) afp.afp_mc_get(mc_id, kProp_Invalidate, 1);
}

int ResolveSiblings(const AfpFuncs& afp, uint32_t stream_id, const char* clip_path, int* out_ids,
                    int max) {
    if ((afp.afp_mc_get_relative_id == nullptr) || (out_ids == nullptr) || max <= 0) return 0;
    int mc = FindClip(afp, stream_id, clip_path);
    int n = 0;
    for (; mc >= 0 && n < max; mc = afp.afp_mc_get_relative_id(mc, kDir_NextSibling))
        out_ids[n++] = mc;
    return n;
}

int BindClipImages(const AfpFuncs& afp, uint32_t stream_id, const char* clip_path,
                   const ImageSlot* slots, int n) {
    if ((afp.afp_play_work_load_image == nullptr) || (afp.afp_mc_get_relative_id == nullptr) ||
        (afp.afp_mc_get_id_by_path == nullptr) || (slots == nullptr) || n <= 0 || slots[0].slot < 1)
        return 0;
    int child = FindClip(afp, stream_id, clip_path);
    if (child < 0) return 0;

    int bound = 0;
    const int kMaxSiblings = 64;
    for (; child >= 0 && bound < kMaxSiblings;
         child = afp.afp_mc_get_relative_id(child, kDir_NextSibling)) {
        BindImageToMc(afp, child, slots[0]);
        bound++;
    }
    return bound;
}

int EnumerateClips(const AfpFuncs& afp, uint32_t stream_id, ClipVisitor visitor, void* user,
                   int max_clips) {
    if ((afp.afp_mc_get_id_by_path == nullptr) || (afp.afp_mc_get_relative_id == nullptr) ||
        (visitor == nullptr))
        return 0;
    int const id = afp.afp_mc_get_id_by_path(stream_id, "");
    if (id < 0) {
        return 0;
    }
    int first = afp.afp_mc_get_relative_id(id, kDir_FirstInTree);
    if (first < 0) first = id;
    int visited = 0;
    for (int cur = first; cur >= 0 && visited < max_clips;
         cur = afp.afp_mc_get_relative_id(cur, kDir_NextInDFS)) {
        visited++;
        if (!visitor(cur, user)) break;
    }
    return visited;
}

}
