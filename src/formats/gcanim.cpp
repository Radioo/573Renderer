#include "formats/gcanim.h"

#include "formats/sysidx.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

namespace GcAnim {

namespace {

constexpr int kPercent = 100;
constexpr float kRotationScale = 6.2831853F / 65536.0F;
constexpr int kMaxDepth = 8;

int Lerp(int a, int b, int num, int den) {
    if (den == 0) return a;
    return a + (((b - a) * num) / den);
}

struct Transform {
    float ox = 0.0F;
    float oy = 0.0F;
    float sx = 1.0F;
    float sy = 1.0F;
    float alpha = 1.0F;
    Blend blend = Blend::Normal;
};

Blend SelectBlend(uint16_t flags, int alpha_a, int alpha_b) {
    if ((flags & SysIdx::kFlagSubtract) != 0) return Blend::Subtract;
    if (alpha_b != 0 && (alpha_a + alpha_b) > kPercent) return Blend::Additive;
    return Blend::Normal;
}

void EvaluateGroup(const SysIdx::Package& pkg, size_t start_index, int frame, const Transform& xf,
                   int depth, std::vector<DrawNode>& out);

void EvaluateRecord(const SysIdx::Package& pkg, const SysIdx::Record& rec, int frame,
                    const Transform& xf, int depth, std::vector<DrawNode>& out) {
    if (frame < rec.t_start || frame >= rec.t_end) return;

    int child_frame = rec.t_base + frame - rec.t_start;
    if (rec.duration > 0) child_frame = (kPercent * child_frame) / rec.duration;

    int scale_y = kPercent;
    const int scale_x = SampleTrack(rec.scale, frame, kPercent, scale_y);
    int pos_y = 0;
    const int pos_x = SampleTrack(rec.position, frame, 0, pos_y);
    int alpha_b = 0;
    int alpha_a = kPercent;
    if (!rec.alpha.empty()) alpha_a = SampleTrack(rec.alpha, frame, kPercent, alpha_b);
    if ((rec.flags & SysIdx::kFlagAlphaTrack) == 0) alpha_b = 0;
    if (alpha_a == 0 && alpha_b == kPercent) return;

    const Blend own_blend = SelectBlend(rec.flags, alpha_a, alpha_b);
    const Blend blend = (own_blend == Blend::Normal) ? xf.blend : own_blend;
    const float alpha = xf.alpha * ((float)alpha_a / (float)kPercent);
    int rot_unused = 0;
    const int rot = SampleTrack(rec.rotation, frame, 0, rot_unused);

    const float own_sx = (float)scale_x / (float)kPercent;
    const float own_sy = (float)scale_y / (float)kPercent;
    const float pivot_x = xf.ox + (xf.sx * (float)pos_x);
    const float pivot_y = xf.oy + (xf.sy * (float)pos_y);
    const float draw_x = pivot_x - (xf.sx * own_sx * (float)rec.anchor_x);
    const float draw_y = pivot_y - (xf.sy * own_sy * (float)rec.anchor_y);
    const float sx = xf.sx * own_sx;
    const float sy = xf.sy * own_sy;

    if (rec.type == SysIdx::kRecNested) {
        if (rec.id >= 0 && (size_t)rec.id < pkg.records.size()) {
            const Transform child{
                .ox = draw_x, .oy = draw_y, .sx = sx, .sy = sy, .alpha = alpha, .blend = blend};
            EvaluateGroup(pkg, (size_t)rec.id, child_frame, child, depth + 1, out);
        }
        return;
    }
    if (rec.type != SysIdx::kRecDrawCell) return;
    if (rec.id < 0 || (size_t)rec.id >= pkg.cells.size()) return;

    const SysIdx::Cell& cell = pkg.cells[(size_t)rec.id];
    DrawNode node;
    node.cell = rec.id;
    node.x = draw_x;
    node.y = draw_y;
    node.w = (float)cell.w * sx;
    node.h = (float)cell.h * sy;
    node.rotation = (float)rot * kRotationScale;
    node.pivot_x = pivot_x;
    node.pivot_y = pivot_y;
    node.alpha = std::clamp(alpha, 0.0F, 1.0F);
    node.blend = blend;
    out.push_back(node);
}

void EvaluateGroup(const SysIdx::Package& pkg, size_t start_index, int frame, const Transform& xf,
                   int depth, std::vector<DrawNode>& out) {
    if (depth > kMaxDepth) return;
    for (size_t i = start_index; i < pkg.records.size(); i++) {
        const SysIdx::Record& rec = pkg.records[i];
        if (rec.type < 0) return;
        EvaluateRecord(pkg, rec, frame, xf, depth, out);
    }
}

}

int ResolveFrame(int time, int length, const Timing& timing) {
    if (time < 0) return -1;
    if (timing.loop_end > timing.loop_start && time >= timing.loop_end) {
        const int span = timing.loop_end - timing.loop_start;
        time = timing.loop_start + ((time - timing.loop_start) % span);
    }
    if (length <= 0 || time < length) return time;
    switch (timing.playback) {
    case Playback::Loop:
        return time % length;
    case Playback::HoldLast:
        return length - 1;
    case Playback::HideAfterEnd:
        break;
    }
    return -1;
}

int SampleTrack(const std::vector<SysIdx::Key>& keys, int t, int fallback_a, int& out_b) {
    if (keys.empty()) {
        out_b = fallback_a;
        return fallback_a;
    }
    if (t <= keys.front().t) {
        out_b = keys.front().b;
        return keys.front().a;
    }
    if (t >= keys.back().t) {
        out_b = keys.back().b;
        return keys.back().a;
    }
    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].t <= t)
        i++;
    const SysIdx::Key& k0 = keys[i];
    const SysIdx::Key& k1 = keys[i + 1];
    const int den = k1.t - k0.t;
    const int num = t - k0.t;
    out_b = Lerp(k0.b, k1.b, num, den);
    return Lerp(k0.a, k1.a, num, den);
}

void Evaluate(const SysIdx::Package& pkg, size_t start_index, int frame, float ox, float oy,
              std::vector<DrawNode>& out, const SkipSet& skip) {
    out.clear();
    if (start_index >= pkg.records.size()) return;
    const Transform root{.ox = ox, .oy = oy};
    for (size_t i = start_index; i < pkg.records.size(); i++) {
        const SysIdx::Record& rec = pkg.records[i];
        if (rec.type < 0) break;
        if (rec.type == SysIdx::kRecNested &&
            std::ranges::find(skip.children, (size_t)rec.id) != skip.children.end())
            continue;
        if (rec.type == SysIdx::kRecDrawCell &&
            std::ranges::find(skip.cells, (int)rec.id) != skip.cells.end())
            continue;
        EvaluateRecord(pkg, rec, frame, root, 0, out);
    }
    std::ranges::reverse(out);
}

}
