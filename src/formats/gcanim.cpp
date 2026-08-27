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
    uint16_t blend_code = 0;
    int alpha_a = kPercent;
    int alpha_b = 0;
};

uint16_t BlendCode(uint16_t flags, uint16_t inherited, bool alpha_authored) {
    uint16_t code = inherited;
    if (alpha_authored) code |= flags & SysIdx::kBlendCodeMask;
    return (flags & SysIdx::kBlendCodeXor) ^ (code | (flags & SysIdx::kFlagSubtract));
}

struct Shade {
    uint16_t code = 0;
    int alpha_a = kPercent;
    int alpha_b = 0;
};

Blend SelectBlend(uint16_t code, int alpha_a, int alpha_b) {
    if ((code & SysIdx::kFlagSubtract) != 0) return Blend::Subtract;
    const int blended = ((code & SysIdx::kFlagBlend) != 0) ? alpha_b : 0;
    if (blended == 0) return Blend::Replace;
    if ((alpha_a + blended) > kPercent) return Blend::Additive;
    return Blend::Normal;
}

void EvaluateGroup(const SysIdx::Package& pkg, size_t start_index, int frame, const Transform& xf,
                   int depth, const SkipSet& skip, std::vector<DrawNode>& out);

bool Hidden(const SysIdx::Record& rec, const SkipSet& skip) {
    if (rec.type == SysIdx::kRecNested)
        return std::ranges::find(skip.children, (size_t)rec.id) != skip.children.end();
    if (rec.type == SysIdx::kRecDrawCell)
        return std::ranges::find(skip.cells, (int)rec.id) != skip.cells.end();
    return false;
}

Shade ShadeOf(const SysIdx::Record& rec, int frame, const Transform& xf) {
    int alpha_a = xf.alpha_a;
    const int inherited_b = xf.alpha_b;
    int sum = alpha_a + inherited_b;
    if (!rec.alpha.empty()) {
        int track_b = 0;
        const int track_a = SampleTrack(rec.alpha, frame, kPercent, track_b);
        alpha_a = (alpha_a * track_a) / kPercent;
        const bool keep_b = inherited_b > 0 && (xf.blend_code & SysIdx::kFlagBlend) != 0;
        sum = keep_b ? (alpha_a + inherited_b) : ((sum * (track_a + track_b)) / kPercent);
    }

    Shade out;
    out.alpha_a = alpha_a;
    out.alpha_b = sum - alpha_a;
    const bool authored = (out.alpha_a != kPercent) || (out.alpha_b != 0);
    out.code = BlendCode(rec.flags, xf.blend_code, authored);
    return out;
}

struct Placement {
    float x = 0.0F;
    float y = 0.0F;
    float sx = 1.0F;
    float sy = 1.0F;
};

struct Pivot {
    float x = 0.0F;
    float y = 0.0F;
};

DrawNode MakeNode(const SysIdx::Record& rec, const SysIdx::Cell& cell, const Shade& shade,
                  Blend blend, float alpha, Placement at, Pivot pivot, int rot) {
    DrawNode node;
    node.cell = rec.id;
    node.x = at.x;
    node.y = at.y;
    node.w = (float)cell.w * at.sx;
    node.h = (float)cell.h * at.sy;
    node.rotation = (float)rot * kRotationScale;
    node.pivot_x = pivot.x;
    node.pivot_y = pivot.y;
    node.alpha = std::clamp(alpha, 0.0F, 1.0F);
    node.blend = blend;
    node.flags = rec.flags;
    node.blend_code = shade.code;
    node.alpha_a = shade.alpha_a;
    node.alpha_b = shade.alpha_b;
    node.alpha_keys = (int)rec.alpha.size();
    return node;
}

void EvaluateRecord(const SysIdx::Package& pkg, const SysIdx::Record& rec, int frame,
                    const Transform& xf, int depth, const SkipSet& skip,
                    std::vector<DrawNode>& out) {
    if (frame < rec.t_start || frame >= rec.t_end) return;

    int child_frame = rec.t_base + frame - rec.t_start;
    if (rec.duration > 0) child_frame = (kPercent * child_frame) / rec.duration;

    int scale_y = kPercent;
    const int scale_x = SampleTrack(rec.scale, frame, kPercent, scale_y);
    int pos_y = 0;
    const int pos_x = SampleTrack(rec.position, frame, 0, pos_y);
    const Shade shade = ShadeOf(rec, frame, xf);
    const int blended_b = ((shade.code & SysIdx::kFlagBlend) != 0) ? shade.alpha_b : 0;
    if (shade.alpha_a == 0 && blended_b == kPercent) return;
    const Blend blend = SelectBlend(shade.code, shade.alpha_a, shade.alpha_b);
    const float alpha = (float)shade.alpha_a / (float)kPercent;
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
            const Transform child{.ox = draw_x,
                                  .oy = draw_y,
                                  .sx = sx,
                                  .sy = sy,
                                  .blend_code = shade.code,
                                  .alpha_a = shade.alpha_a,
                                  .alpha_b = shade.alpha_b};
            EvaluateGroup(pkg, (size_t)rec.id, child_frame, child, depth + 1, skip, out);
        }
        return;
    }
    if (rec.type != SysIdx::kRecDrawCell) return;
    if (rec.id < 0 || (size_t)rec.id >= pkg.cells.size()) return;

    const SysIdx::Cell& cell = pkg.cells[(size_t)rec.id];
    out.push_back(MakeNode(rec, cell, shade, blend, alpha,
                           Placement{.x = draw_x, .y = draw_y, .sx = sx, .sy = sy},
                           Pivot{.x = pivot_x, .y = pivot_y}, rot));
}

void EvaluateGroup(const SysIdx::Package& pkg, size_t start_index, int frame, const Transform& xf,
                   int depth, const SkipSet& skip, std::vector<DrawNode>& out) {
    if (depth > kMaxDepth) return;
    for (size_t i = start_index; i < pkg.records.size(); i++) {
        const SysIdx::Record& rec = pkg.records[i];
        if (rec.type < 0) return;
        if (Hidden(rec, skip)) continue;
        EvaluateRecord(pkg, rec, frame, xf, depth, skip, out);
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

bool TexelDiscarded(int src_alpha) {
    return src_alpha <= 0;
}

BlendFactors FactorsFor(Blend blend) {
    switch (blend) {
    case Blend::Additive:
        return {.src = Factor::SrcAlpha,
                .dst = Factor::One,
                .op = BlendOp::Add,
                .src_alpha = Factor::Zero,
                .dst_alpha = Factor::One};
    case Blend::Subtract:
        return {.src = Factor::SrcAlpha,
                .dst = Factor::One,
                .op = BlendOp::RevSubtract,
                .src_alpha = Factor::Zero,
                .dst_alpha = Factor::One};
    case Blend::Replace:
        return {.src = Factor::SrcAlpha,
                .dst = Factor::Zero,
                .op = BlendOp::Add,
                .src_alpha = Factor::One,
                .dst_alpha = Factor::Zero};
    default:
        return {.src = Factor::SrcAlpha,
                .dst = Factor::InvSrcAlpha,
                .op = BlendOp::Add,
                .src_alpha = Factor::One,
                .dst_alpha = Factor::InvSrcAlpha};
    }
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
        if (Hidden(rec, skip)) continue;
        EvaluateRecord(pkg, rec, frame, root, 0, skip, out);
    }
    std::ranges::reverse(out);
}

}
