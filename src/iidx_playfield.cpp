#include "iidx_playfield.h"

#include "afp_ddr.h"
#include "afp_ddr_clips.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace IidxPlayfield {

namespace {

struct Font {
    const char* glyph_prefix;
    int cells;
    int advance;
    std::array<int, 3> table;
    bool fixed_pitch;
    float leading_alpha;
};

constexpr Font kScoreFont = {
    .glyph_prefix = "_sco",
    .cells = 6,
    .advance = 14,
    .table = {0, 0, 0},
    .fixed_pitch = true,
    .leading_alpha = 0.3F,
};

constexpr Font kMaxComboFont = {
    .glyph_prefix = "_sco",
    .cells = 4,
    .advance = 14,
    .table = {0, 0, 0},
    .fixed_pitch = true,
    .leading_alpha = 0.3F,
};

constexpr Font kBpmFont = {
    .glyph_prefix = "_bpm",
    .cells = 3,
    .advance = 17,
    .table = {0, 0, 0},
    .fixed_pitch = true,
    .leading_alpha = 0.3F,
};

constexpr Font kBpmRangeFont = {
    .glyph_prefix = "_bpm_m",
    .cells = 3,
    .advance = 7,
    .table = {0, 0, 0},
    .fixed_pitch = true,
    .leading_alpha = 0.3F,
};

constexpr Font kPercentSpFont = {
    .glyph_prefix = "_par",
    .cells = 3,
    .advance = 0,
    .table = {0, 11, 25},
    .fixed_pitch = false,
    .leading_alpha = 0.0F,
};

constexpr Font kPercentDpFont = {
    .glyph_prefix = "_par",
    .cells = 3,
    .advance = 0,
    .table = {0, 10, 23},
    .fixed_pitch = false,
    .leading_alpha = 0.0F,
};

constexpr int kDigitPriority = 67;
constexpr int kJudgeLinePriority = 64;
constexpr int kJudgeGlowPriority = 65;
constexpr float kJudgeGlowSteadyAlpha = 50.0F;
constexpr float kJudgeGlowPulseAlpha = 75.0F;
constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 480;
constexpr const char* kJudgeLineBitmap = "_red_line";
constexpr const char* kJudgeGlowBitmap = "_grada";
constexpr int kAfpBlendAdd = 8;
constexpr int kGaugeTrackPriority = 70;
constexpr int kGaugeFillPriority = 69;
constexpr int kGaugeThrobPriority = 68;
constexpr float kGaugeTrackAlpha = 20.0F;
constexpr float kFullAlpha = 100.0F;
constexpr int kGaugePixelsPerPercent = 2;
constexpr int kGaugeWidth = 200;
constexpr int kGaugeMaskHeight = 480;
constexpr int kGaugeTipMinPercent = 2;
constexpr int kGaugeTipWidth = 4;
constexpr int kGaugeRedFromPercent = 80;
constexpr int kMaxPercent = 100;
constexpr int kExHardThrobFrames = 90;
constexpr float kTwoPi = 6.28318548F;
constexpr int kHispeedSteps = 11;
constexpr int kStageCount = 9;
constexpr int kDifficultyCount = 4;

constexpr std::array<const char*, kHispeedSteps> kHispeedBitmaps = {
    "_hs_off", "_hs_05", "_hs_10", "_hs_15", "_hs_20", "_hs_25",
    "_hs_30",  "_hs_35", "_hs_40", "_hs_45", "_hs_50"};

constexpr std::array<const char*, kStageCount> kStageBitmaps = {
    "_st_1st",   "_st_2nd",   "_st_3rd",  "_st_4th",  "_st_final",
    "_st_extra", "_st_extra", "_st_demo", "_st_extra"};

constexpr std::array<const char*, kDifficultyCount> kDifficultyBitmaps = {"_m_normal", "_m_hyper",
                                                                          "_m_ano", "_m_begin"};

constexpr std::array<const char*, 3> kGaugeTipBitmaps = {"_gauge_g", "_gauge_r", "_gauge_y"};

constexpr const char* kEffectOff = "_ef_off";

Values g_values;
bool g_enabled = false;

int Clamp(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

int PowerOfTen(int exponent) {
    int out = 1;
    for (int i = 0; i < exponent; i++)
        out *= 10;
    return out;
}

std::string GlyphName(const Font& font, int digit) {
    return std::string(font.glyph_prefix) + "0" + static_cast<char>('0' + digit);
}

void DrawNumber(const Font& font, const DdrClips::ClipState& anchor, int value) {
    const int limit = PowerOfTen(font.cells) - 1;
    int remaining = Clamp(value, 0, limit);
    float alpha = anchor.alpha;

    for (int cell = 0; cell < font.cells; cell++) {
        const int digit = remaining % 10;
        remaining /= 10;
        const int column = font.cells - cell;
        const float x =
            font.fixed_pitch
                ? anchor.x + static_cast<float>(font.advance * (column - 1))
                : anchor.x + static_cast<float>(font.table[static_cast<size_t>(
                                 Clamp(column - 1, 0, static_cast<int>(font.table.size()) - 1))]);
        if (alpha > 0.0F) {
            static_cast<void>(DdrClips::DrawSprite(GlyphName(font, digit), x, anchor.y, alpha,
                                                   kDigitPriority, anchor.origin_x,
                                                   anchor.origin_y));
        }
        if (remaining == 0) alpha = anchor.alpha * font.leading_alpha;
    }
}

void DrawAnchoredNumber(const std::string& name, const Font& font, int value) {
    const DdrClips::ClipState anchor = DdrClips::Read(name);
    if (!anchor.found) return;
    DdrClips::SetVisible(name, false);
    DrawNumber(font, anchor, value);
}

std::string GaugeBitmap(Gauge gauge, bool second_player) {
    const char* kind = "normal";
    if (gauge == Gauge::Hard) kind = "hard";
    if (gauge == Gauge::ExHard) kind = "exhard";
    return std::string("_gauge_") + kind + (second_player ? "_2p" : "_1p");
}

void DrawGauge(const std::string& name, bool second_player) {
    const DdrClips::ClipState anchor = DdrClips::Read(name);
    if (!anchor.found) return;
    DdrClips::SetVisible(name, false);

    const int percent = Clamp(g_values.percent, 0, kMaxPercent);
    const int x = static_cast<int>(anchor.x);
    const int y = static_cast<int>(anchor.y);
    const std::string bar = GaugeBitmap(g_values.gauge, second_player);

    const uint32_t track =
        DdrClips::DrawSprite(bar, anchor.x, anchor.y, kGaugeTrackAlpha, kGaugeTrackPriority);
    DdrClips::MaskSprite(track, x, y, kGaugeWidth, kGaugeMaskHeight);

    const int fill = kGaugePixelsPerPercent * percent;
    const uint32_t filled =
        DdrClips::DrawSprite(bar, anchor.x, anchor.y, kFullAlpha, kGaugeFillPriority);
    if (second_player) {
        DdrClips::MaskSprite(filled, x + (kGaugePixelsPerPercent * (kMaxPercent - percent)), y,
                             kGaugeWidth, kGaugeMaskHeight);
    } else {
        DdrClips::MaskSprite(filled, x, y, fill, kGaugeMaskHeight);
    }

    if (g_values.gauge == Gauge::ExHard) {
        const auto phase = static_cast<float>(DdrAfp::FrameCounter() % kExHardThrobFrames);
        const float amplitude =
            std::fabs(std::sin(phase / static_cast<float>(kExHardThrobFrames) * kTwoPi));
        const uint32_t throb = DdrClips::DrawSprite(bar, anchor.x, anchor.y, amplitude * kFullAlpha,
                                                    kGaugeThrobPriority);
        DdrClips::MaskSprite(throb, x, y, fill, kGaugeMaskHeight);
    }

    if (percent < kGaugeTipMinPercent) return;
    auto tip = static_cast<size_t>(g_values.gauge);
    if (g_values.gauge == Gauge::Normal) tip = (percent >= kGaugeRedFromPercent) ? 1 : 0;
    const float tip_x =
        second_player
            ? anchor.x + static_cast<float>(kGaugePixelsPerPercent * (kMaxPercent - percent))
            : anchor.x + static_cast<float>(fill - kGaugeTipWidth);
    static_cast<void>(DdrClips::DrawSprite(kGaugeTipBitmaps[tip], tip_x, anchor.y, kFullAlpha,
                                           kGaugeFillPriority));
}

bool ParseInt(std::string_view text, int& out) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void DrawJudgeLine() {
    if (g_values.line_y == 0) return;
    const auto x = static_cast<float>(g_values.line_x);

    if (g_values.glow_y != 0) {
        const auto glow_y = static_cast<float>(g_values.glow_y);
        for (const float alpha : {kJudgeGlowSteadyAlpha, kJudgeGlowPulseAlpha}) {
            const uint32_t glow =
                DdrClips::DrawSprite(kJudgeGlowBitmap, x, glow_y, alpha, kJudgeGlowPriority);
            DdrClips::BlendSprite(glow, kAfpBlendAdd);
        }
    }

    const uint32_t line = DdrClips::DrawSprite(
        kJudgeLineBitmap, x, static_cast<float>(g_values.line_y), kFullAlpha, kJudgeLinePriority);
    if (g_values.line_clip > g_values.line_y) {
        DdrClips::MaskSprite(line, 0, g_values.line_clip, kScreenWidth, kScreenHeight);
    }
}

bool ApplyLayout(const std::string& key, int number, Values& out) {
    if (key == "line_x") {
        out.line_x = number;
        return true;
    }
    if (key == "line_y") {
        out.line_y = number;
        return true;
    }
    if (key == "line_clip") {
        out.line_clip = number;
        return true;
    }
    if (key == "glow_y") {
        out.glow_y = number;
        return true;
    }
    return false;
}

bool ApplyNumber(const std::string& key, int number, Values& out, std::string& err) {
    if (key == "score") {
        out.score = number;
    } else if (key == "maxcombo") {
        out.max_combo = number;
    } else if (key == "bpm") {
        out.bpm = number;
    } else if (key == "bpm_min") {
        out.bpm_min = number;
    } else if (key == "bpm_max") {
        out.bpm_max = number;
    } else if (key == "percent") {
        out.percent = number;
    } else if (key == "hispeed") {
        out.hispeed = Clamp(number, 0, kHispeedSteps - 1);
    } else if (key == "stage") {
        out.stage = Clamp(number, 0, kStageCount - 1);
    } else if (key == "difficulty") {
        out.difficulty = Clamp(number, 0, kDifficultyCount - 1);
    } else if (!ApplyLayout(key, number, out)) {
        err = "unknown playfield key '" + key + "'";
        return false;
    }
    return true;
}

bool ApplyKey(const std::string& key, const std::string& value, Values& out, std::string& err) {
    if (key == "effect") {
        out.effect = value;
        return true;
    }
    if (key == "gauge") {
        if (value == "normal") {
            out.gauge = Gauge::Normal;
        } else if (value == "hard") {
            out.gauge = Gauge::Hard;
        } else if (value == "exhard") {
            out.gauge = Gauge::ExHard;
        } else {
            err = "gauge expects normal, hard or exhard, got '" + value + "'";
            return false;
        }
        return true;
    }
    if (key == "dp" || key == "lights") {
        const bool on = (value == "1" || value == "true" || value == "on");
        (key == "dp" ? out.double_play : out.key_lights) = on;
        return true;
    }

    int number = 0;
    if (!ParseInt(value, number)) {
        err = key + " expects a number, got '" + value + "'";
        return false;
    }
    return ApplyNumber(key, number, out, err);
}

}

bool Parse(const std::string& spec, Values& out, std::string& err) {
    size_t at = 0;
    while (at < spec.size()) {
        const size_t comma = spec.find(',', at);
        const std::string item = spec.substr(at, comma - at);
        at = (comma == std::string::npos) ? spec.size() : comma + 1;
        if (item.empty()) continue;
        const size_t equals = item.find('=');
        if (equals == std::string::npos) {
            err = "playfield item '" + item + "' is not key=value";
            return false;
        }
        if (!ApplyKey(item.substr(0, equals), item.substr(equals + 1), out, err)) return false;
    }
    if (out.bpm_min == 0 && out.bpm_max == 0) {
        out.bpm_min = out.bpm;
        out.bpm_max = out.bpm;
    }
    return true;
}

void Enable(const Values& values) {
    g_values = values;
    g_enabled = true;
}

bool Enabled() {
    return g_enabled;
}

void Apply() {
    if (!g_enabled || !DdrClips::Available()) return;

    DrawAnchoredNumber("score_1p", kScoreFont, g_values.score);
    DrawAnchoredNumber("maxcombo_1p", kMaxComboFont, g_values.max_combo);
    DrawAnchoredNumber("percent_1p", g_values.double_play ? kPercentDpFont : kPercentSpFont,
                       g_values.percent);
    DrawAnchoredNumber("bpm", kBpmFont, g_values.bpm);

    if (g_values.bpm_min == g_values.bpm_max) {
        for (const char* name : {"bpm_min", "bpm_max", "bpm_min_t", "bpm_max_t"})
            DdrClips::SetVisible(name, false);
    } else {
        DrawAnchoredNumber("bpm_min", kBpmRangeFont, g_values.bpm_min);
        DrawAnchoredNumber("bpm_max", kBpmRangeFont, g_values.bpm_max);
    }

    DdrClips::SetBitmap("hispeed_1p", kHispeedBitmaps[static_cast<size_t>(g_values.hispeed)]);
    DdrClips::SetBitmap("info_01", kStageBitmaps[static_cast<size_t>(g_values.stage)]);
    DdrClips::SetBitmap("info_02", kDifficultyBitmaps[static_cast<size_t>(g_values.difficulty)]);
    DdrClips::SetBitmap("effect", g_values.effect.empty() ? kEffectOff : g_values.effect);

    const char* light = g_values.double_play ? "_dp_bline" : "_sp_bline";
    if (g_values.key_lights) {
        DdrClips::SetBitmap("light_1p", light);
        DdrClips::SetVisible("light_1p", true);
    } else {
        DdrClips::SetVisible("light_1p", false);
    }

    DrawGauge("gg_bar_1p", false);
    DrawJudgeLine();

    for (const char* name :
         {"score_2p", "maxcombo_2p", "percent_2p", "gg_bar_2p", "light_2p", "hispeed_2p"})
        DdrClips::SetVisible(name, false);
}

}
