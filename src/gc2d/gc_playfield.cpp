#include "gc2d/gc_playfield.h"

#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "iidx_playfield.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace Gc2dPlayfield {

namespace {

constexpr int kElementEffector = 1;
constexpr int kElementBpmFirst = 2;
constexpr int kElementBank = 100;
constexpr int kSecondPlayerBank = 2;
constexpr int kSlotPercentFirst = 5;
constexpr int kSlotGauge = 8;
constexpr int kSlotScore = 9;
constexpr int kSlotMaxCombo = 10;
constexpr int kSlotHiSpeed = 11;
constexpr int kSlotInformation = 12;

constexpr int kScoreDigits = 6;
constexpr int kMaxComboDigits = 4;
constexpr int kDigitAdvance = 15;
constexpr int kMaxComboCeiling = 9999;
constexpr float kLeadingAlpha = 0.3F;
constexpr float kFullAlpha = 1.0F;

constexpr int kGaugeCells = 50;
constexpr int kGaugeCellStep = 4;
constexpr int kGaugeClearPercent = 80;

constexpr int kDecimalPlaces = 3;
constexpr std::array<int, kDecimalPlaces> kPowers = {100, 10, 1};
constexpr int kMostThreeDigits = 999;
constexpr int kFullPercent = 100;

constexpr int kValuePriority = 100;
constexpr int kJudgeLinePriority = 90;
constexpr int kJudgeGlowPriority = 89;
constexpr float kJudgeGlowSteadyAlpha = 0.5F;
constexpr float kJudgeGlowPulseAlpha = 0.75F;

constexpr std::array<const char*, 11> kHiSpeedCells = {"HS_OFF", "HS_05", "HS_10", "HS_15",
                                                       "HS_20",  "HS_25", "HS_30", "HS_35",
                                                       "HS_40",  "HS_45", "HS_50"};
constexpr std::array<const char*, 6> kStageCells = {"ST_1ST", "ST_2ND",   "ST_3RD",
                                                    "ST_4TH", "ST_FINAL", "ST_EXTRA"};

bool g_enabled = false;
std::string g_asset;
IidxPlayfield::Values g_values;

Gc2dHost::SpritePlacement Cell(const std::string& name, float x, float y, float alpha) {
    Gc2dHost::SpritePlacement out;
    out.asset = g_asset;
    out.name = name;
    out.animated = false;
    out.priority = kValuePriority;
    out.x = x;
    out.y = y;
    out.alpha = alpha;
    return out;
}

int PowerAt(int index) {
    return kPowers.at(static_cast<std::size_t>(std::clamp(index, 0, kDecimalPlaces - 1)));
}

void AppendNumber(std::vector<Gc2dHost::SpritePlacement>& sprites, const std::string& prefix,
                  int value, int digits, const GcAnim::ElementNode& at) {
    int remaining = std::max(value, 0);
    bool consumed = false;
    for (int place = digits - 1; place >= 0; place--) {
        const int digit = remaining % 10;
        remaining /= 10;
        const float alpha = (consumed && digit == 0) ? kLeadingAlpha : kFullAlpha;
        sprites.push_back(Cell(prefix + std::to_string(digit),
                               at.x + static_cast<float>(kDigitAdvance * place), at.y, alpha));
        if (remaining == 0) consumed = true;
    }
}

void AppendPercentDigit(std::vector<Gc2dHost::SpritePlacement>& sprites, int index,
                        const GcAnim::ElementNode& at) {
    const int percent = std::clamp(g_values.percent, 0, kMostThreeDigits);
    const int above = percent / PowerAt(index);
    if (above == 0 && index != kDecimalPlaces - 1) return;
    sprites.push_back(Cell("PAR0" + std::to_string(above % 10), at.x, at.y, kFullAlpha));
}

void AppendBpmDigit(std::vector<Gc2dHost::SpritePlacement>& sprites, int index,
                    const GcAnim::ElementNode& at) {
    const int bpm = std::clamp(g_values.bpm, 0, kMostThreeDigits);
    const int above = bpm / PowerAt(index);
    sprites.push_back(Cell("BPM0" + std::to_string(above % 10), at.x, at.y,
                           (above == 0) ? kLeadingAlpha : kFullAlpha));
}

void AppendGauge(std::vector<Gc2dHost::SpritePlacement>& sprites, const GcAnim::ElementNode& at,
                 bool second_player) {
    const int lit = std::clamp(g_values.percent, 0, kFullPercent) / 2;
    const int step = second_player ? -kGaugeCellStep : kGaugeCellStep;
    const int red_from = kGaugeClearPercent / 2;
    for (int cell = 1; cell <= kGaugeCells; cell++) {
        const char* name = (cell >= red_from) ? "GAUGE_R" : "GAUGE_G";
        const float alpha = (cell <= lit) ? kFullAlpha : kLeadingAlpha;
        sprites.push_back(Cell(name, at.x + static_cast<float>(step * (cell - 1)), at.y, alpha));
    }
}

void AppendSlot(std::vector<Gc2dHost::SpritePlacement>& sprites, int slot, bool second_player,
                const GcAnim::ElementNode& at) {
    if (slot == kSlotScore) {
        AppendNumber(sprites, "SCO", g_values.score, kScoreDigits, at);
        return;
    }
    if (slot == kSlotMaxCombo) {
        AppendNumber(sprites, "SCO", std::min(g_values.max_combo, kMaxComboCeiling),
                     kMaxComboDigits, at);
        return;
    }
    if (slot >= kSlotPercentFirst && slot < kSlotPercentFirst + kDecimalPlaces) {
        AppendPercentDigit(sprites, slot - kSlotPercentFirst, at);
        return;
    }
    if (slot == kSlotGauge) {
        AppendGauge(sprites, at, second_player);
        return;
    }
    if (slot == kSlotHiSpeed) {
        const auto pick = static_cast<std::size_t>(
            std::clamp(g_values.hispeed, 0, static_cast<int>(kHiSpeedCells.size()) - 1));
        sprites.push_back(Cell(kHiSpeedCells.at(pick), at.x, at.y, kFullAlpha));
        return;
    }
    if (slot != kSlotInformation) return;
    const auto pick = static_cast<std::size_t>(
        std::clamp(g_values.stage, 0, static_cast<int>(kStageCells.size()) - 1));
    const std::string name = kStageCells.at(pick);
    const std::array<int, 2> size = Gc2dHost::CellSize(g_asset, name);
    const int half_w = size[0] / 2;
    const int half_h = size[1] / 2;
    sprites.push_back(Cell(name, at.x - static_cast<float>(half_w),
                           at.y - static_cast<float>(half_h), kFullAlpha));
}

void AppendElement(std::vector<Gc2dHost::SpritePlacement>& sprites, const GcAnim::ElementNode& at) {
    if (at.id == kElementEffector) {
        sprites.push_back(
            Cell(g_values.effect.empty() ? "EF_OFF" : g_values.effect, at.x, at.y, kFullAlpha));
        return;
    }
    if (at.id >= kElementBpmFirst && at.id < kElementBpmFirst + kDecimalPlaces) {
        AppendBpmDigit(sprites, at.id - kElementBpmFirst, at);
        return;
    }
    if (at.id < kElementBank) return;
    AppendSlot(sprites, at.id % kElementBank, (at.id / kElementBank) == kSecondPlayerBank, at);
}

void AppendJudgeLine(std::vector<Gc2dHost::SpritePlacement>& sprites) {
    if (g_values.line_y == 0) return;
    const auto x = static_cast<float>(g_values.line_x);
    if (g_values.glow_y != 0) {
        const auto glow_y = static_cast<float>(g_values.glow_y);
        for (const float alpha : {kJudgeGlowSteadyAlpha, kJudgeGlowPulseAlpha}) {
            Gc2dHost::SpritePlacement glow = Cell("GRADA", x, glow_y, alpha);
            glow.priority = kJudgeGlowPriority;
            glow.blend = GcAnim::Blend::Additive;
            sprites.push_back(glow);
        }
    }
    Gc2dHost::SpritePlacement line =
        Cell("RED_LINE", x, static_cast<float>(g_values.line_y), kFullAlpha);
    line.priority = kJudgeLinePriority;
    sprites.push_back(line);
}

}

void Enable(const std::string& parts_asset, const IidxPlayfield::Values& values) {
    g_enabled = true;
    g_asset = parts_asset;
    g_values = values;
}

bool Enabled() {
    return g_enabled;
}

void Append(std::vector<Gc2dHost::SpritePlacement>& sprites) {
    if (!g_enabled) return;
    for (const GcAnim::ElementNode& at : Gc2dHost::ListElements())
        AppendElement(sprites, at);
    AppendJudgeLine(sprites);
}

}
