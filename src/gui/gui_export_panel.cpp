#include "gui_export_panel.h"
#include "gui_dpi.h"
#include "../native_dialog.h"
#include "editor/export_range.h"
#include "encode/export_encoder.h"
#include "editor/preset_editor_state.h"
#include "../export.h"
#include "../export_capture.h"
#include "../state/app_state.h"
#include "../state/commands.h"
#include "imgui.h"
#include "media/media_format.h"
#include "render/stretch.h"
#include "state/live_controls.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <array>
#include <string>
#include <utility>
#include <cmath>

namespace Panels::Export {

namespace {

char g_stem_buf[512] = {};
std::string g_last_ifs;
std::string g_last_anim;

const App::ExportRequest kDefaults{};

constexpr float kModalWidth = 620.0F;
constexpr float kScreenMargin = 48.0F;

int g_fps = kDefaults.fps;
int g_quality = kDefaults.quality;
int g_keyframe_interval = kDefaults.keyframe_interval;
int g_max_frames = kDefaults.max_frames;
bool g_limit_frames = false;
int g_loop_count = kDefaults.loop_count;
bool g_blend_loop = kDefaults.blend_loop;
int g_blend_frames = kDefaults.blend_frames;
int g_format_idx = kDefaults.format;
bool g_prefer_hw = kDefaults.prefer_hardware;

bool g_use_doc_range = true;

int g_out_w = kDefaults.width;
int g_out_h = kDefaults.height;

bool g_bg_transparent = kDefaults.bg_transparent;
std::array<float, 3> g_bg_rgb = {kDefaults.bg_r, kDefaults.bg_g, kDefaults.bg_b};

struct DocumentPlan {
    bool loaded = false;
    bool range_active = false;
    int start = 0;
    int frames = 0;
    int fps = 60;
};

DocumentPlan CurrentDocumentPlan() {
    DocumentPlan plan;
    const App::PresetStatus status = App::Global().GetPresetStatus();
    const Editor::State& editor = Editor::Global();
    if (status.id.empty() || !editor.Loaded() || editor.Document().id != status.id) return plan;
    plan.loaded = true;
    plan.fps = std::max(1, status.fps);
    const Editor::ExportRange range =
        Editor::ClampRange(editor.GetView().export_range, status.length);
    plan.range_active = range.active;
    plan.start = range.active ? range.start : 0;
    plan.frames = Editor::RangeFrames(range, status.length);
    return plan;
}

void ApplyDocumentDefaults() {
    const DocumentPlan plan = CurrentDocumentPlan();
    if (!plan.loaded) return;
    g_fps = plan.fps;
    g_use_doc_range = true;
}

void DrawDocumentRange(const DocumentPlan& plan) {
    if (!plan.loaded) return;
    ImGui::Checkbox("Document range##exp_preset_range", &g_use_doc_range);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Export the range shown on the ruler (Shift+drag on the ruler sets it),\n"
                          "or the whole document when no range is set. Turn this off to use\n"
                          "the frame limit below.");
    }
    ImGui::SameLine();
    if (plan.range_active) {
        ImGui::TextDisabled("frames %d..%d (%d frames)", plan.start, plan.start + plan.frames - 1,
                            plan.frames);
    } else {
        ImGui::TextDisabled("Whole document (%d frames)", plan.frames);
    }
}

void MaybeRegenerateStem(App::State& state) {
    std::string active = state.ActiveIfs();
    std::string playing = state.GetStatus().playing_animation;
    const bool first_draw = (g_stem_buf[0] == '\0' && g_last_ifs.empty() && g_last_anim.empty());
    if (!first_draw && active == g_last_ifs && playing == g_last_anim) {
        return;
    }
    const std::string stem = MediaSink::DeriveExportStem(active, playing);
    snprintf(g_stem_buf, sizeof(g_stem_buf), "%s", stem.c_str());
    g_last_ifs = std::move(active);
    g_last_anim = std::move(playing);
}

void DrawFilenameAndFormat() {
    const MediaSink::Format current_format = MediaSink::FromIndex(g_format_idx);
    const char* current_ext = MediaSink::FormatExtension(current_format);
    const char* shown_ext = MediaSink::WritesDirectory(current_format) ? "/" : current_ext;

    const float ext_w = ImGui::CalcTextSize(shown_ext).x + 8.0F;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ext_w);
    ImGui::InputText("##exp_stem", g_stem_buf, sizeof(g_stem_buf));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output file name without the extension; the\n"
                          "format below sets the extension. The name is\n"
                          "filled in again when you switch IFS or\n"
                          "animation. A relative path is resolved against\n"
                          "the renderer's working directory.\n\n"
                          "For a PNG sequence the name becomes a folder,\n"
                          "with the frames inside as frame_NNNNNN.png.");
    }
    ImGui::SameLine(0.0F, 0.0F);
    ImGui::TextDisabled("%s", shown_ext);

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##exp_fmt", MediaSink::FormatLabel(current_format))) {
        for (int i = 0; i < MediaSink::kFormatCount; i++) {
            MediaSink::Format const f = MediaSink::FromIndex(i);
            bool const selected = (i == g_format_idx);
            if (ImGui::Selectable(MediaSink::FormatLabel(f), selected)) g_format_idx = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("AVIF: AV1 with a separate alpha plane. Plays in <img>\n"
                          "tags with transparency. The colour stream uses NVENC on\n"
                          "an RTX 40-series GPU, about 50x faster than software.\n\n"
                          "WebM VP9: a single yuva420p stream. Plays in <video>\n"
                          "tags with transparency. Software encoding only, because\n"
                          "NVIDIA desktop GPUs have no VP9 encoder. Long clips play\n"
                          "more smoothly in <video> than as an animated <img>,\n"
                          "since Blink's 10 ms frame-duration clamp for animated\n"
                          "images does not apply.\n\n"
                          "WebM AV1: a single AV1 stream in WebM, encoded with\n"
                          "NVENC and played in <video>. The fastest and smoothest\n"
                          "option, but it has no alpha.\n\n"
                          "WebP: animated WebP through libwebp_anim. Alpha and\n"
                          "smooth playback in <img> tags in every current browser.\n"
                          "About 2-3x larger than AV1 or AVIF, without their\n"
                          "decoder bugs at high frame rates.\n\n"
                          "PNG sequence: a folder of frame_NNNNNN.png files\n"
                          "(six-digit numbers). Lossless and large on disk. Good\n"
                          "for re-encoding elsewhere or for checking single\n"
                          "frames. The quality slider has no effect.");
    }
}

void DrawFpsQualitySliders() {
    ImGui::SetNextItemWidth(Gui::Dpi::S(120.0F));
    ImGui::InputInt("fps##exp_fps", &g_fps, 1, 10);
    g_fps = std::clamp(g_fps, 1, 240);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output frame rate. The animation plays at the\n"
                          "same speed whatever you set here: the renderer\n"
                          "samples it at this rate. 120 matches the\n"
                          "internal tick rate, so no frames are skipped.\n"
                          "Use 30 or 60 for a smaller file.");
    }
    const DocumentPlan plan = CurrentDocumentPlan();
    if (plan.loaded && !Editor::FpsRatioAllowed(plan.fps, g_fps)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.45F, 1.0F),
                           "not a whole multiple or divisor of the document's %d fps", plan.fps);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt("##exp_q", &g_quality, 0, 100, "Quality %d");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output quality. 0 gives the smallest file with\n"
                          "heavy artifacts, 100 is close to lossless.\n"
                          "Default 60. It sets the libaom CRF for software\n"
                          "AVIF, the NVENC CQP for hardware AVIF and\n"
                          "WebM AV1, and the libvpx CRF for WebM VP9.");
    }
}

void DrawKeyframeIntervalControl(MediaSink::Format current_format) {
    if (MediaSink::UsesKeyframeInterval(current_format)) {
        ImGui::SetNextItemWidth(Gui::Dpi::S(120.0F));
        ImGui::InputInt("Keyframe interval##exp_keyint", &g_keyframe_interval, 1, 30);
        g_keyframe_interval = std::clamp(g_keyframe_interval, 0, 100000);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Frames between full keyframes. 0 means one keyframe\n"
                              "per second. Fewer keyframes give a smaller file, which\n"
                              "suits static or slowly scrolling scenes: they compress\n"
                              "to almost nothing between keyframes. A value at or\n"
                              "above the total frame count gives a single keyframe,\n"
                              "the smallest file but the slowest to seek.");
        }
        ImGui::SameLine();
        if (g_keyframe_interval == 0) {
            ImGui::TextDisabled("(one per second)");
        } else {
            ImGui::TextDisabled("(every %d frames)", g_keyframe_interval);
        }
    }
}

void DrawFrameLimitControls(const ::Export::Capabilities& caps) {
    ImGui::Checkbox("Limit frames##exp_limit", &g_limit_frames);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Stop the export after a set number of captured frames\n"
                          "(60 at first, one second at 60 fps). Off by default:\n"
                          "the export then stops after %s.\n\n"
                          "Use it to cut a long title down to a short preview, to\n"
                          "capture a looped clip of fixed length without waiting\n"
                          "for the end-of-timeline detector, or to get the same\n"
                          "file size on every test run.",
                          caps.natural_end);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!g_limit_frames);
    if (g_limit_frames && g_max_frames <= 0) g_max_frames = 60;
    ImGui::SetNextItemWidth(Gui::Dpi::S(120.0F));
    ImGui::InputInt("frames##exp_maxf", &g_max_frames, 1, 10);
    const bool frames_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    g_max_frames = std::clamp(g_max_frames, 1, 100000);
    ImGui::SameLine();
    {
        const double secs = (double)g_max_frames / (double)(g_fps > 0 ? g_fps : 60);
        ImGui::TextDisabled("(~%.2fs at %d fps)", secs, g_fps > 0 ? g_fps : 60);
    }
    ImGui::EndDisabled();
    if (frames_hovered) {
        ImGui::SetTooltip("How many frames the encoder receives before the export\n"
                          "finishes. Captured frames are counted, so 60 gives a\n"
                          "file with exactly 60 frames.");
    }
}

void DrawLoopControls(const ::Export::Capabilities& caps) {
    if (!caps.loop_count && !caps.blend_seam) {
        ImGui::TextDisabled("Loop options do not apply to this content.");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("A screen preset is built from layers with different\n"
                              "periods and a screen timer that changes speed, so it has\n"
                              "no single loop point to count or crossfade. The export\n"
                              "stops after %s.\nUse Limit frames for any other length.",
                              caps.natural_end);
        }
        return;
    }
    ImGui::SetNextItemWidth(Gui::Dpi::S(120.0F));
    if (ImGui::InputInt("Loops to capture##exp_loops", &g_loop_count, 1, 5))
        g_loop_count = std::clamp(g_loop_count, 1, 1000);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How many loops of the animation to capture (default 1).\n"
                          "Recording continues past each detected loop point until\n"
                          "the output holds this many loops. Limit frames still\n"
                          "applies.");
    }

    ImGui::Checkbox("Blend loop seam##exp_blend", &g_blend_loop);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("For backgrounds that do not loop cleanly, such as one-shot intros\n"
                          "like select_bg_iv's _4bg. The captured cycle is buffered, the frame\n"
                          "closest to the start is picked, and the wrap is crossfaded so it\n"
                          "dissolves instead of jumping. The game itself does not do this, so\n"
                          "leave it off for backgrounds that already loop cleanly.");
    }
    if (g_blend_loop) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(Gui::Dpi::S(100.0F));
        if (ImGui::InputInt("Blend frames##exp_blendN", &g_blend_frames, 1, 5))
            g_blend_frames = std::clamp(g_blend_frames, 0, 240);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Crossfade length in frames (default 15). A longer fade is softer;\n"
                              "a shorter one is crisper but shows more of the mismatch at the\n"
                              "seam. 0 means a hard cut at the best frame.");
        }
    }
}

struct ResPreset {
    const char* label;
    int w;
    int h;
};

int MatchPresetIdx(const ResPreset* presets, int custom_idx) {
    for (int i = 0; i < custom_idx; i++) {
        if (presets[i].w == g_out_w && presets[i].h == g_out_h) return i;
    }
    return custom_idx;
}

void ApplyPresetSelection(const ResPreset* presets, int i, int custom_idx, int rw, int rh) {
    if (i == 0) {
        g_out_w = 0;
        g_out_h = 0;
    } else if (i == custom_idx) {
        if (g_out_w <= 0 || g_out_h <= 0) {
            g_out_w = rw;
            g_out_h = rh;
        }
    } else {
        g_out_w = presets[i].w;
        g_out_h = presets[i].h;
    }
}

void DrawResolutionPresetCombo(int rw, int rh) {
    char native_label[48];
    snprintf(native_label, sizeof(native_label), "Native (%dx%d)", rw, rh);
    const ResPreset kPresets[] = {
        {.label = native_label, .w = 0, .h = 0},      {.label = "1920x1080", .w = 1920, .h = 1080},
        {.label = "1280x720", .w = 1280, .h = 720},   {.label = "640x480", .w = 640, .h = 480},
        {.label = "1080x1920", .w = 1080, .h = 1920}, {.label = "720x1280", .w = 720, .h = 1280},
        {.label = "Custom", .w = -1, .h = -1},
    };
    const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
    const int kCustomIdx = kPresetCount - 1;

    static int shown_idx = -1;
    static int last_out_w = -2;
    static int last_out_h = -2;

    if (shown_idx < 0 || g_out_w != last_out_w || g_out_h != last_out_h) {
        shown_idx = MatchPresetIdx(kPresets, kCustomIdx);
    }
    last_out_w = g_out_w;
    last_out_h = g_out_h;

    ImGui::Text("Output resolution:");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##exp_sz", kPresets[shown_idx].label)) {
        for (int i = 0; i < kPresetCount; i++) {
            bool const selected = (i == shown_idx);
            if (ImGui::Selectable(kPresets[i].label, selected)) {
                shown_idx = i;
                ApplyPresetSelection(kPresets, i, kCustomIdx, rw, rh);
                last_out_w = g_out_w;
                last_out_h = g_out_h;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output resolution. Smaller sizes make smaller files that\n"
                          "are much cheaper for a browser to decode. Native follows\n"
                          "the render size from Setup, so the export changes with\n"
                          "it. The fixed sizes always export at exactly that size.");
    }
}

void DrawResolutionInputs(int rw, int rh, int& w_disp, int& h_disp) {
    w_disp = (g_out_w > 0) ? g_out_w : rw;
    h_disp = (g_out_h > 0) ? g_out_h : rh;
    const float input_w = Gui::Dpi::S(100.0F);
    ImGui::SetNextItemWidth(input_w);
    bool const w_changed =
        ImGui::InputInt("##exp_w", &w_disp, 0, 0, ImGuiInputTextFlags_AutoSelectAll);
    ImGui::SameLine();
    ImGui::TextUnformatted("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(input_w);
    bool const h_changed =
        ImGui::InputInt("##exp_h", &h_disp, 0, 0, ImGuiInputTextFlags_AutoSelectAll);
    ImGui::SameLine();
    ImGui::TextDisabled("(width x height, pixels)");
    if (w_changed || h_changed) {
        w_disp = std::clamp(w_disp, 64, 8192);
        h_disp = std::clamp(h_disp, 64, 8192);
        g_out_w = w_disp;
        g_out_h = h_disp;
    }
}

void DrawScaleButtons(int w_disp, int h_disp) {
    struct ScaleBtn {
        const char* label;
        float mul;
        const char* tip;
    };
    const ScaleBtn kScales[] = {
        {.label = "x0.5##exp_scl",
         .mul = 0.5F,
         .tip = "Halve the current size (1080p to 540p, 720p to 360p).\n"
                "Keeps the aspect ratio."},
        {.label = "x0.25##exp_scl",
         .mul = 0.25F,
         .tip = "Quarter the current size (1080p to 270p, 720p to 180p).\n"
                "Very small files at preview quality."},
        {.label = "x0.1##exp_scl",
         .mul = 0.1F,
         .tip = "One tenth of the current size, for thumbnail strips\n"
                "or previews on a very slow connection."},
    };
    ImGui::TextDisabled("Scale current size:");
    ImGui::SameLine();
    for (const auto& sb : kScales) {
        int new_w = (int)std::lroundf((float)w_disp * sb.mul);
        int new_h = (int)std::lroundf((float)h_disp * sb.mul);
        new_w = std::clamp(new_w, 64, 8192);
        new_h = std::clamp(new_h, 64, 8192);
        char btn_label[64];
        const char* visible_end = strstr(sb.label, "##");
        int const prefix_len =
            (visible_end != nullptr) ? (int)(visible_end - sb.label) : (int)strlen(sb.label);
        snprintf(btn_label, sizeof(btn_label), "%.*s (%dx%d)##exp_scl_%s", prefix_len, sb.label,
                 new_w, new_h, sb.label);
        if (ImGui::Button(btn_label)) {
            g_out_w = new_w;
            g_out_h = new_h;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", sb.tip);
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void DrawOutputResolution(App::State& state) {
    int rw = 0;
    int rh = 0;
    state.GetRenderSize(rw, rh);
    if (rw <= 0) rw = 1920;
    if (rh <= 0) rh = 1080;
    const Stretch::Size frame = Stretch::Present(rw, rh, state.GetStretchWide());
    rw = frame.w;
    rh = frame.h;

    DrawResolutionPresetCombo(rw, rh);
    int w_disp = 0;
    int h_disp = 0;
    DrawResolutionInputs(rw, rh, w_disp, h_disp);
    DrawScaleButtons(w_disp, h_disp);
}

bool DrawCropPickButtons(App::State& state, bool pick_mode, int* xywh, bool& changed) {
    bool close_for_pick = false;
    if (pick_mode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65F, 0.45F, 0.15F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75F, 0.55F, 0.22F, 1.0F));
        if (ImGui::Button("Picking...##crop_pick", ImVec2(Gui::Dpi::S(110.0F), 0.0F))) {
            state.SetCropPickMode(false);
        }
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::Button("Pick region##crop_pick", ImVec2(Gui::Dpi::S(110.0F), 0.0F))) {
            state.SetCropPickMode(true);
            close_for_pick = true;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click, then drag a rectangle on the render\n"
                          "window. Esc cancels. This dialog opens again\n"
                          "once the region is picked, and the export\n"
                          "encodes only the pixels inside it.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##crop_clear", ImVec2(Gui::Dpi::S(70.0F), 0.0F))) {
        state.SetCropRect({});
        state.SetCropPickMode(false);
        xywh[0] = xywh[1] = xywh[2] = xywh[3] = 0;
        changed = true;
    }
    return close_for_pick;
}

bool DrawCrop(App::State& state) {
    App::CropRect const rect = state.GetCropRect();
    int xywh[4] = {rect.x, rect.y, rect.w, rect.h};
    const bool pick_mode = state.GetCropPickMode();

    ImGui::TextUnformatted("Crop");

    const float avail = ImGui::GetContentRegionAvail().x;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float lbl_w = ImGui::CalcTextSize("x").x;
    const float per_overhead = lbl_w + st.ItemInnerSpacing.x + st.ItemSpacing.x;
    const float per_input_w = (std::max)(Gui::Dpi::S(36.0F), (avail - per_overhead * 4.0F) / 4.0F);

    const char* labels[4] = {"x##crop_x", "y##crop_y", "w##crop_w", "h##crop_h"};
    bool changed = false;
    for (int i = 0; i < 4; i++) {
        ImGui::SetNextItemWidth(per_input_w);
        if (ImGui::InputInt(labels[i], &xywh[i], 0, 0, ImGuiInputTextFlags_AutoSelectAll)) {
            changed = true;
        }
        if (i < 3) ImGui::SameLine();
    }

    bool const close_for_pick = DrawCropPickButtons(state, pick_mode, xywh, changed);

    if (changed) {
        App::CropRect r;
        r.x = (std::max)(0, xywh[0]);
        r.y = (std::max)(0, xywh[1]);
        r.w = (std::max)(0, xywh[2]);
        r.h = (std::max)(0, xywh[3]);
        state.SetCropRect(r);
    }
    return close_for_pick;
}

void DrawHwAccelTooltip(MediaSink::Format current_format, bool hw_available, bool is_h264) {
    if (current_format == MediaSink::Format::MP4_HEVC_Alpha) {
        ImGui::SetTooltip("MP4 HEVC with alpha is software only (libx265).\n"
                          "NVENC's HEVC encoder cannot encode alpha, and\n"
                          "alpha is the reason to pick this format.");
    } else if (!hw_available && is_h264) {
        ImGui::SetTooltip("h264_nvenc is not available on this machine.\n"
                          "H.264 export needs an NVIDIA GPU with NVENC,\n"
                          "because this ffmpeg build has no software\n"
                          "H.264 encoder (no libx264 or openh264).");
    } else if (!hw_available) {
        ImGui::SetTooltip("Hardware encoding is not available on this\n"
                          "machine. It needs an NVIDIA GPU that can\n"
                          "encode AV1 (RTX 40-series or newer) and an\n"
                          "ffmpeg build with nvcodec enabled.");
    } else if (current_format == MediaSink::Format::WebM_VP9) {
        ImGui::SetTooltip("WebM VP9 has no hardware encoder: NVIDIA\n"
                          "desktop GPUs have no VP9 encoder. Switch\n"
                          "to AVIF or WebM AV1 to use NVENC.");
    } else if (current_format == MediaSink::Format::WebP_Anim) {
        ImGui::SetTooltip("WebP (libwebp_anim) is software only, since\n"
                          "consumer GPUs have no VP8 encoder. libwebp\n"
                          "is fast anyway, about 5-10x faster than\n"
                          "libaom-av1.");
    } else if (current_format == MediaSink::Format::PNG_Sequence) {
        ImGui::SetTooltip("PNG sequence writes lossless per-frame\n"
                          "files through WIC, straight from the BGRA\n"
                          "frames. There is no encoder, so NVENC and\n"
                          "the quality slider do not apply.");
    } else if (is_h264) {
        ImGui::SetTooltip("Encode H.264 with NVENC (h264_nvenc), which\n"
                          "most NVIDIA GPUs have, not only the RTX\n"
                          "40-series. No alpha. This ffmpeg build has no\n"
                          "software H.264 encoder, so H.264 export\n"
                          "needs NVENC.");
    } else {
        ImGui::SetTooltip("Encode the AV1 stream with NVENC\n"
                          "(av1_nvenc), about 50x faster than\n"
                          "libaom-av1 on an RTX 40-series GPU.\n"
                          "AVIF's alpha stream is still encoded in\n"
                          "software: it is tiny, and NVENC cannot\n"
                          "encode alpha. If NVENC fails to start,\n"
                          "the export falls back to software.");
    }
}

void DrawBackgroundAndHw(MediaSink::Format current_format, bool hw_available,
                         const ::Export::Capabilities& caps) {
    if (!caps.transparent_bg) g_bg_transparent = false;
    ImGui::BeginDisabled(!caps.transparent_bg);
    ImGui::Checkbox("Transparent background", &g_bg_transparent);
    ImGui::EndDisabled();
    if (!caps.transparent_bg) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("This content draws its own opaque background, so there\n"
                              "is no alpha to keep.");
        }
    } else if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Export a real alpha channel (AVIF plays with\n"
                          "transparency). Turn it off to put a solid\n"
                          "colour under the animation, for viewers that\n"
                          "handle animated AVIF transparency badly.");
    }
    ImGui::SameLine();
    const bool bg_colour_inert = g_bg_transparent || !caps.transparent_bg;
    if (bg_colour_inert) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(Gui::Dpi::S(160.0F));
    ImGui::ColorEdit3("##exp_bg_color", g_bg_rgb.data(),
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    if (bg_colour_inert) ImGui::EndDisabled();
    if (!caps.transparent_bg && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("This screen is opaque, so the export matches the preview\n"
                          "and no background colour shows through.");
    }

    const bool is_h264 = (current_format == MediaSink::Format::MP4_H264);
    const bool format_can_use_hw = (current_format == MediaSink::Format::AVIF ||
                                    current_format == MediaSink::Format::WebM_AV1 || is_h264);
    const bool hw_applies = hw_available && format_can_use_hw;
    if (!hw_applies) {
        ImGui::BeginDisabled();
        bool tmp = false;
        ImGui::Checkbox("Hardware encoding##exp_hw", &tmp);
        ImGui::EndDisabled();
    } else {
        ImGui::Checkbox("Hardware encoding##exp_hw", &g_prefer_hw);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        DrawHwAccelTooltip(current_format, hw_available, is_h264);
    }
}

void PostStartRequest(App::State& state, MediaSink::Format current_format, bool hw_applies) {
    App::ExportRequest r;
    r.output_path = MediaSink::MakeOutputPath(g_stem_buf, current_format);
    r.fps = g_fps;
    r.quality = g_quality;
    r.keyframe_interval = g_keyframe_interval;
    const DocumentPlan plan = CurrentDocumentPlan();
    if (plan.loaded && g_use_doc_range) {
        r.start_frame = plan.start;
        r.max_frames = plan.frames;
    } else {
        r.max_frames = g_limit_frames ? g_max_frames : 0;
    }
    r.loop_count = g_loop_count;
    r.blend_loop = g_blend_loop;
    r.blend_frames = g_blend_frames;
    r.bg_transparent = g_bg_transparent;
    r.bg_r = g_bg_rgb[0];
    r.bg_g = g_bg_rgb[1];
    r.bg_b = g_bg_rgb[2];
    r.width = g_out_w;
    r.height = g_out_h;
    App::CropRect const cr = state.GetCropRect();
    r.crop_x = cr.x;
    r.crop_y = cr.y;
    r.crop_w = cr.w;
    r.crop_h = cr.h;
    r.format = g_format_idx;
    r.prefer_hardware = g_prefer_hw && hw_applies;
    state.PostCommand(App::Cmd::StartExport{.req = std::move(r)});
}

void DrawRevealButton(const std::string& path) {
    if (path.empty()) return;
    if (ImGui::Button("Open folder")) NativeDialog::RevealInFileManager(path);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show the exported file in Explorer.");
    }
}

void DrawStartAndStatus(App::State& state, const App::ExportState& ex, bool busy,
                        MediaSink::Format current_format, bool hw_available) {
    const DocumentPlan plan = CurrentDocumentPlan();
    const bool ratio_ok = !plan.loaded || Editor::FpsRatioAllowed(plan.fps, g_fps);
    if (!busy) {
        ImGui::BeginDisabled(!ratio_ok);
        if (ImGui::Button("Start export", ImVec2(Gui::Dpi::S(120.0F), 0.0F))) {
            const bool format_can_use_hw = (current_format == MediaSink::Format::AVIF ||
                                            current_format == MediaSink::Format::WebM_AV1 ||
                                            current_format == MediaSink::Format::MP4_H264);
            const bool hw_applies = hw_available && format_can_use_hw;
            PostStartRequest(state, current_format, hw_applies);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        if (!ratio_ok && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("The export fps must be a whole multiple or divisor of the "
                              "document's %d fps, or the screen plays at the wrong speed.",
                              plan.fps);
        }
    } else {
        if (ImGui::Button("Cancel", ImVec2(Gui::Dpi::S(120.0F), 0.0F))) {
            state.PostCommand(App::Cmd::CancelExport{});
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(Gui::Dpi::S(90.0F), 0.0F))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    switch (ex.phase) {
    case App::ExportPhase::Idle:
        ImGui::TextDisabled("Idle");
        break;
    case App::ExportPhase::Capturing:
        ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.3F, 1.0F), "Capturing frame %d...%s",
                           ex.frames_captured, ex.using_hardware ? "  [NVENC]" : "");
        break;
    case App::ExportPhase::Encoding: {
        const char* enc_label = MediaSink::FormatToken(MediaSink::FromIndex(ex.format));
        ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.3F, 1.0F), "Encoding %s (%d frames)...%s",
                           enc_label, ex.frames_captured, ex.using_hardware ? "  [NVENC]" : "");
        ImGui::ProgressBar(-1.0F * (float)ImGui::GetTime(), ImVec2(-FLT_MIN, 0), "Finishing");
        break;
    }
    case App::ExportPhase::Done:
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50F, 0.92F, 0.65F, 1.0F));
        ImGui::TextWrapped("Saved: %s", ex.output_path.c_str());
        ImGui::PopStyleColor();
        DrawRevealButton(ex.output_path);
        break;
    case App::ExportPhase::Failed:
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.45F, 0.45F, 1.0F));
        ImGui::TextWrapped("Export failed: %s", ex.error.c_str());
        ImGui::PopStyleColor();
        break;
    }
}

bool g_open_requested = false;
bool g_reopen_after_pick = false;

}

namespace {
void PlaceModalWindow() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(
        Gui::Dpi::S(kModalWidth, 0.0F),
        ImVec2(Gui::Dpi::S(kModalWidth), vp->WorkSize.y - Gui::Dpi::S(kScreenMargin)));
}
}

void RequestOpen() {
    g_open_requested = true;
}

void RenderModal() {
    auto& state = App::Global();

    if (g_reopen_after_pick && !state.GetCropPickMode()) {
        g_reopen_after_pick = false;
        g_open_requested = true;
    }
    if (g_open_requested) {
        ImGui::OpenPopup("Export");
        ApplyDocumentDefaults();
        g_open_requested = false;
    }

    PlaceModalWindow();
    if (!ImGui::BeginPopupModal("Export", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    App::ExportState const ex = state.GetExport();
    const bool busy =
        (ex.phase == App::ExportPhase::Capturing || ex.phase == App::ExportPhase::Encoding);

    MaybeRegenerateStem(state);

    const MediaSink::Format current_format = MediaSink::FromIndex(g_format_idx);
    const bool hw_available =
        ExportEncoder::HardwareAvailable(MediaSink::HardwareProbeFormat(current_format));

    if (busy) ImGui::BeginDisabled();
    DrawFilenameAndFormat();
    DrawFpsQualitySliders();
    DrawOutputResolution(state);
    const ::Export::Capabilities caps = ::Export::ActiveCapabilities();
    DrawBackgroundAndHw(current_format, hw_available, caps);

    ImGui::Spacing();
    bool close_for_pick = false;
    if (ImGui::CollapsingHeader("Advanced")) {
        DrawKeyframeIntervalControl(current_format);
        DrawDocumentRange(CurrentDocumentPlan());
        DrawFrameLimitControls(caps);
        DrawLoopControls(caps);
        close_for_pick = DrawCrop(state);
    }
    if (busy) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawStartAndStatus(state, ex, busy, current_format, hw_available);

    if (close_for_pick) {
        g_reopen_after_pick = true;
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}
