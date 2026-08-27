#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "formats/sysidx.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

void FillCells(SysIdx::Package& pkg) {
    pkg.cells.push_back(SysIdx::Cell{.x = 0, .y = 0, .w = 32, .h = 16});
    pkg.cells.push_back(SysIdx::Cell{.x = 32, .y = 0, .w = 8, .h = 8});
}

SysIdx::Record DrawCell(int16_t id, int16_t t_start, int16_t t_end) {
    SysIdx::Record rec;
    rec.type = SysIdx::kRecDrawCell;
    rec.id = id;
    rec.t_start = t_start;
    rec.t_end = t_end;
    return rec;
}

SysIdx::Record EndAnimation() {
    SysIdx::Record rec;
    rec.type = SysIdx::kRecEndAnimation;
    return rec;
}

}

TEST_CASE("SampleTrack falls back on an empty track") {
    const std::vector<SysIdx::Key> keys;
    int b = -1;
    CHECK(GcAnim::SampleTrack(keys, 5, 100, b) == 100);
    CHECK(b == 100);
}

TEST_CASE("SampleTrack clamps to the first and last keys") {
    const std::vector<SysIdx::Key> keys = {{.t = 0, .a = 10, .b = 1}, {.t = 10, .a = 110, .b = 51}};
    int b = -1;
    CHECK(GcAnim::SampleTrack(keys, -5, 0, b) == 10);
    CHECK(b == 1);
    CHECK(GcAnim::SampleTrack(keys, 99, 0, b) == 110);
    CHECK(b == 51);
}

TEST_CASE("SampleTrack interpolates both channels between keys") {
    const std::vector<SysIdx::Key> keys = {{.t = 0, .a = 0, .b = 0}, {.t = 10, .a = 100, .b = 50}};
    int b = -1;
    CHECK(GcAnim::SampleTrack(keys, 5, 0, b) == 50);
    CHECK(b == 25);
}

TEST_CASE("Evaluate emits a draw node inside the frame window only") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.anchor_x = 4;
    rec.anchor_y = 2;
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 100.0F, 50.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].cell == 0);
    CHECK(out[0].x == Catch::Approx(96.0F));
    CHECK(out[0].y == Catch::Approx(48.0F));
    CHECK(out[0].w == Catch::Approx(32.0F));
    CHECK(out[0].h == Catch::Approx(16.0F));
    CHECK(out[0].alpha == Catch::Approx(1.0F));
    CHECK(out[0].blend == GcAnim::Blend::Replace);

    GcAnim::Evaluate(pkg, 0, 10, 100.0F, 50.0F, out);
    CHECK(out.empty());
}

TEST_CASE("Evaluate clears the output and rejects a bad start index") {
    SysIdx::Package pkg;
    FillCells(pkg);
    std::vector<GcAnim::DrawNode> out(3);
    GcAnim::Evaluate(pkg, 7, 0, 0.0F, 0.0F, out);
    CHECK(out.empty());
}

TEST_CASE("Evaluate reverses draw order so later records draw first") {
    SysIdx::Package pkg;
    FillCells(pkg);
    pkg.records.push_back(DrawCell(0, 0, 10));
    pkg.records.push_back(DrawCell(1, 0, 10));
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 0, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].cell == 1);
    CHECK(out[1].cell == 0);
}

TEST_CASE("An end-animation record terminates the group walk") {
    SysIdx::Package pkg;
    FillCells(pkg);
    pkg.records.push_back(DrawCell(0, 0, 10));
    pkg.records.push_back(EndAnimation());
    pkg.records.push_back(DrawCell(1, 0, 10));

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 0, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].cell == 0);
}

TEST_CASE("Out-of-range cell and nested ids are skipped") {
    SysIdx::Package pkg;
    FillCells(pkg);
    pkg.records.push_back(DrawCell(99, 0, 10));
    SysIdx::Record nested = DrawCell(50, 0, 10);
    nested.type = SysIdx::kRecNested;
    pkg.records.push_back(nested);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 0, 0.0F, 0.0F, out);
    CHECK(out.empty());
}

TEST_CASE("An alpha track past 100 percent combined coverage selects additive blending") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.flags = SysIdx::kFlagBlend;
    rec.alpha = {{.t = 0, .a = 60, .b = 60}, {.t = 10, .a = 60, .b = 60}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].blend == GcAnim::Blend::Additive);
    CHECK(out[0].alpha == Catch::Approx(0.6F));
}

TEST_CASE("The subtract flag outranks the additive alpha rule") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.flags = SysIdx::kFlagBlend | SysIdx::kFlagSubtract;
    rec.alpha = {{.t = 0, .a = 60, .b = 60}, {.t = 10, .a = 60, .b = 60}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].blend == GcAnim::Blend::Subtract);
}

TEST_CASE("Fully transparent with full secondary coverage is culled") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.flags = SysIdx::kFlagBlend;
    rec.alpha = {{.t = 0, .a = 0, .b = 100}, {.t = 10, .a = 0, .b = 100}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    CHECK(out.empty());
}

TEST_CASE("Without the alpha-track flag a zero alpha still emits at alpha zero") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.alpha = {{.t = 0, .a = 0, .b = 100}, {.t = 10, .a = 0, .b = 100}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].alpha == Catch::Approx(0.0F));
    CHECK(out[0].blend == GcAnim::Blend::Replace);
}

TEST_CASE("A nested record remaps the child frame through its duration") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record nested = DrawCell(2, 0, 10);
    nested.type = SysIdx::kRecNested;
    nested.duration = 5;
    nested.flags = SysIdx::kFlagSubtract | SysIdx::kFlagBlend;
    nested.alpha = {{.t = 0, .a = 100, .b = 50}, {.t = 10, .a = 100, .b = 50}};
    pkg.records.push_back(nested);
    pkg.records.push_back(EndAnimation());
    pkg.records.push_back(DrawCell(0, 20, 21));
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 1, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].blend == GcAnim::Blend::Subtract);

    GcAnim::Evaluate(pkg, 0, 2, 0.0F, 0.0F, out);
    CHECK(out.empty());
}

TEST_CASE("Self-referencing nesting stops at the depth cap") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record nested = DrawCell(0, 0, 10);
    nested.type = SysIdx::kRecNested;
    pkg.records.push_back(nested);
    pkg.records.push_back(DrawCell(0, 0, 10));
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 0, 0.0F, 0.0F, out);
    CHECK(out.size() == 9);
}

TEST_CASE("Rotation keys convert from 16-bit turns to radians") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.rotation = {{.t = 0, .a = 16384, .b = 0}, {.t = 10, .a = 16384, .b = 0}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].rotation == Catch::Approx(1.5707963F).margin(0.0001F));
}

TEST_CASE("Position and scale tracks compose with the parent transform") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.anchor_x = 2;
    rec.position = {{.t = 0, .a = 10, .b = 20}, {.t = 10, .a = 10, .b = 20}};
    rec.scale = {{.t = 0, .a = 200, .b = 50}, {.t = 10, .a = 200, .b = 50}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 100.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].pivot_x == Catch::Approx(110.0F));
    CHECK(out[0].pivot_y == Catch::Approx(20.0F));
    CHECK(out[0].x == Catch::Approx(110.0F - 4.0F));
    CHECK(out[0].w == Catch::Approx(64.0F));
    CHECK(out[0].h == Catch::Approx(8.0F));
}

TEST_CASE("ResolveFrame loops, holds or hides past the end") {
    const GcAnim::Timing loop = {.playback = GcAnim::Playback::Loop};
    const GcAnim::Timing hold = {.playback = GcAnim::Playback::HoldLast};
    const GcAnim::Timing hide = {.playback = GcAnim::Playback::HideAfterEnd};

    CHECK(GcAnim::ResolveFrame(30, 120, loop) == 30);
    CHECK(GcAnim::ResolveFrame(30, 120, hold) == 30);
    CHECK(GcAnim::ResolveFrame(30, 120, hide) == 30);

    CHECK(GcAnim::ResolveFrame(120, 120, loop) == 0);
    CHECK(GcAnim::ResolveFrame(120, 120, hold) == 119);
    CHECK(GcAnim::ResolveFrame(120, 120, hide) == -1);

    CHECK(GcAnim::ResolveFrame(605, 120, loop) == 5);
    CHECK(GcAnim::ResolveFrame(605, 120, hold) == 119);
}

TEST_CASE("ResolveFrame wraps a loop range back to its start") {
    const GcAnim::Timing prompt = {
        .playback = GcAnim::Playback::HoldLast, .loop_start = 80, .loop_end = 200};

    CHECK(GcAnim::ResolveFrame(0, 200, prompt) == 0);
    CHECK(GcAnim::ResolveFrame(199, 200, prompt) == 199);
    CHECK(GcAnim::ResolveFrame(200, 200, prompt) == 80);
    CHECK(GcAnim::ResolveFrame(320, 200, prompt) == 80);
    CHECK(GcAnim::ResolveFrame(325, 200, prompt) == 85);
}

TEST_CASE("ResolveFrame leaves an unknown length alone") {
    const GcAnim::Timing loop = {.playback = GcAnim::Playback::Loop};
    CHECK(GcAnim::ResolveFrame(42, 0, loop) == 42);
    CHECK(GcAnim::ResolveFrame(-1, 120, loop) == -1);
}

TEST_CASE("A hidden part stays hidden inside a nested child") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record nested;
    nested.type = SysIdx::kRecNested;
    nested.id = 2;
    nested.t_start = 0;
    nested.t_end = 10;
    pkg.records.push_back(nested);
    pkg.records.push_back(EndAnimation());
    pkg.records.push_back(DrawCell(0, 0, 10));
    pkg.records.push_back(DrawCell(1, 0, 10));
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> nodes;
    GcAnim::Evaluate(pkg, 0, 1, 0.0F, 0.0F, nodes);
    REQUIRE(nodes.size() == 2);

    const std::vector<int> hide = {1};
    GcAnim::SkipSet skip;
    skip.cells = hide;
    GcAnim::Evaluate(pkg, 0, 1, 0.0F, 0.0F, nodes, skip);
    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].cell == 0);
}

TEST_CASE("the blend mode follows the game's own four way rule") {
    const auto blend_of = [](uint16_t flags, int alpha_a, int alpha_b) {
        SysIdx::Package pkg;
        FillCells(pkg);
        SysIdx::Record rec = DrawCell(0, 0, 10);
        rec.flags = flags;
        rec.alpha = {{.t = 0, .a = (int16_t)alpha_a, .b = (int16_t)alpha_b},
                     {.t = 10, .a = (int16_t)alpha_a, .b = (int16_t)alpha_b}};
        pkg.records.push_back(rec);
        pkg.records.push_back(EndAnimation());
        std::vector<GcAnim::DrawNode> out;
        GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
        REQUIRE(out.size() == 1);
        return out[0].blend;
    };

    CHECK(blend_of(0, 100, 0) == GcAnim::Blend::Replace);
    CHECK(blend_of(0, 100, 80) == GcAnim::Blend::Replace);
    CHECK(blend_of(SysIdx::kFlagBlend, 100, 0) == GcAnim::Blend::Replace);
    CHECK(blend_of(SysIdx::kFlagBlend, 50, 40) == GcAnim::Blend::Normal);
    CHECK(blend_of(SysIdx::kFlagBlend, 100, 80) == GcAnim::Blend::Additive);
    CHECK(blend_of(SysIdx::kFlagBlend | SysIdx::kFlagSubtract, 100, 80) == GcAnim::Blend::Subtract);
    INFO("the subtract flag wins on its own, without the blend enable bit");
    CHECK(blend_of(SysIdx::kFlagSubtract, 100, 80) == GcAnim::Blend::Subtract);
    CHECK(blend_of(SysIdx::kFlagSubtract, 100, 0) == GcAnim::Blend::Subtract);
}

namespace {

struct Texel {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
};

int ApplyFactor(GcAnim::Factor f, int value, int src_alpha) {
    switch (f) {
    case GcAnim::Factor::Zero:
        return 0;
    case GcAnim::Factor::One:
        return value;
    case GcAnim::Factor::SrcAlpha:
        return (value * src_alpha) / 255;
    case GcAnim::Factor::InvSrcAlpha:
        return (value * (255 - src_alpha)) / 255;
    }
    return 0;
}

int Combine(GcAnim::BlendOp op, int s, int d) {
    const int out = (op == GcAnim::BlendOp::RevSubtract) ? (d - s) : (d + s);
    return std::clamp(out, 0, 255);
}

Texel Composite(const Texel& dst, const Texel& src, GcAnim::Blend blend) {
    if (GcAnim::TexelDiscarded(src.a)) return dst;
    const GcAnim::BlendFactors f = GcAnim::FactorsFor(blend);
    Texel out;
    out.r = Combine(f.op, ApplyFactor(f.src, src.r, src.a), ApplyFactor(f.dst, dst.r, src.a));
    out.g = Combine(f.op, ApplyFactor(f.src, src.g, src.a), ApplyFactor(f.dst, dst.g, src.a));
    out.b = Combine(f.op, ApplyFactor(f.src, src.b, src.a), ApplyFactor(f.dst, dst.b, src.a));
    out.a = Combine(f.op, ApplyFactor(f.src_alpha, src.a, src.a),
                    ApplyFactor(f.dst_alpha, dst.a, src.a));
    return out;
}

}

TEST_CASE("a logo drawn over a background does not punch a black rectangle through it") {
    SysIdx::Package pkg;
    pkg.cells.push_back(SysIdx::Cell{.x = 0, .y = 0, .w = 64, .h = 64});
    pkg.cells.push_back(SysIdx::Cell{.x = 64, .y = 0, .w = 32, .h = 32});
    pkg.records.push_back(DrawCell(0, 0, 10));
    pkg.records.push_back(DrawCell(1, 0, 10));
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 2);

    const Texel backdrop{.r = 20, .g = 40, .b = 90, .a = 255};

    const Texel logo_art{.r = 230, .g = 240, .b = 255, .a = 255};
    const Texel lit = Composite(backdrop, logo_art, out[1].blend);
    CHECK(lit.r == 230);
    CHECK(lit.g == 240);
    CHECK(lit.b == 255);

    const Texel logo_surround{.r = 0, .g = 0, .b = 0, .a = 0};
    const Texel around = Composite(backdrop, logo_surround, out[1].blend);
    INFO("a fully transparent texel must leave the backdrop untouched, not wipe it to black");
    CHECK(around.r == backdrop.r);
    CHECK(around.g == backdrop.g);
    CHECK(around.b == backdrop.b);
    CHECK(around.a == backdrop.a);

    const Texel below_art{.r = 10, .g = 10, .b = 10, .a = 0};
    const Texel dark = Composite(backdrop, below_art, out[1].blend);
    INFO("a transparent texel that is dark but not pure black must also leave the backdrop alone");
    CHECK(dark.r == backdrop.r);
    CHECK(dark.b == backdrop.b);
}

TEST_CASE("a subtract flagged record subtracts even when its alpha pair is not authored") {
    SysIdx::Package pkg;
    FillCells(pkg);
    SysIdx::Record rec = DrawCell(0, 0, 10);
    rec.flags = SysIdx::kFlagBlend | SysIdx::kFlagSubtract;
    rec.alpha = {{.t = 0, .a = 100, .b = 0}, {.t = 10, .a = 100, .b = 0}};
    pkg.records.push_back(rec);
    pkg.records.push_back(EndAnimation());

    std::vector<GcAnim::DrawNode> out;
    GcAnim::Evaluate(pkg, 0, 5, 0.0F, 0.0F, out);
    REQUIRE(out.size() == 1);
    INFO("IIDX 17's CREDIT line has flags 0x12 and an alpha pair of (100,0)");
    CHECK(out[0].blend == GcAnim::Blend::Subtract);

    const Texel backdrop{.r = 236, .g = 236, .b = 236, .a = 255};

    const Texel glyph{.r = 255, .g = 255, .b = 255, .a = 255};
    const Texel inked = Composite(backdrop, glyph, out[0].blend);
    INFO("white glyphs subtract from the backdrop, giving DARK text");
    CHECK(inked.r < backdrop.r);

    const Texel field{.r = 0, .g = 0, .b = 0, .a = 255};
    const Texel around = Composite(backdrop, field, out[0].blend);
    INFO("the cell's opaque BLACK field must subtract nothing and leave the backdrop exact");
    CHECK(around.r == backdrop.r);
    CHECK(around.g == backdrop.g);
    CHECK(around.b == backdrop.b);
}
