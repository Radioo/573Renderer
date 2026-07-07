#include "gui_export_panel.h"
#include "../state/app_state.h"
#include "../video_encoder.h"
#include "imgui.h"
#include "media/media_format.h"
#include "state/live_controls.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <cmath>

namespace Panels::Export {

namespace {

char g_stem_buf[512] = {};
std::string g_last_ifs;
std::string g_last_anim;

int g_fps = 60;
int g_quality = 60;
int g_keyframe_interval = 0;
int g_max_frames = 0;
bool g_limit_frames = false;
int g_loop_count = 1;
bool g_blend_loop = false;
int g_blend_frames = 15;
int g_format_idx = 0;
bool g_prefer_hw = true;

int g_out_w = 0;
int g_out_h = 0;

bool g_bg_transparent = true;
float g_bg_rgb[3] = {0.13F, 0.14F, 0.17F};

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
        ImGui::SetTooltip("Output filename (stem only - extension is set\n"
                          "automatically from the format below). Auto-\n"
                          "regenerates when you switch IFS or animation.\n"
                          "Relative paths resolve against the renderer's\n"
                          "working directory.\n\n"
                          "For PNG sequence, the stem becomes a folder\n"
                          "name; frame_NNNNNN.png is written inside it.");
    }
    ImGui::SameLine(0.0F, 0.0F);
    ImGui::TextDisabled("%s", shown_ext);

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("format##exp_fmt", MediaSink::FormatLabel(current_format))) {
        for (int i = 0; i < MediaSink::kFormatCount; i++) {
            MediaSink::Format const f = MediaSink::FromIndex(i);
            bool const selected = (i == g_format_idx);
            if (ImGui::Selectable(MediaSink::FormatLabel(f), selected)) g_format_idx = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("AVIF: AV1 dual-stream with an auxiliary alpha plane.\n"
                          "Plays in <img> tags with native transparency. Colour\n"
                          "stream uses NVENC on an RTX 40-series GPU (~50x faster\n"
                          "than software).\n\n"
                          "WebM VP9: yuva420p single stream. Plays in <video> tags\n"
                          "with native transparency. Software encode only - NVIDIA\n"
                          "desktop GPUs have no VP9 encoder.\n"
                          "<video> playback is much smoother than animated <img>\n"
                          "for long clips because Blink's 10ms animated-image\n"
                          "frame-duration clamp doesn't apply.\n\n"
                          "WebM AV1: AV1 single stream in WebM. Hardware-\n"
                          "accelerated via NVENC and plays in <video> - the\n"
                          "fastest and smoothest option, but opaque only.\n\n"
                          "WebP: animated WebP via libwebp_anim. Alpha + smooth\n"
                          "playback in <img> tags on every modern browser. ~2-3x\n"
                          "larger than AV1/AVIF but no decoder bugs at high fps.\n\n"
                          "PNG sequence: writes a folder of frame_NNNNNN.png\n"
                          "files (zero-padded 6 digits). Lossless, large on disk,\n"
                          "ideal for re-encoding workflows or frame-level\n"
                          "inspection. The 'Quality' slider is ignored.");
    }
}

void DrawFpsQualitySliders() {
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("fps##exp_fps", &g_fps, 1, 10);
    g_fps = std::clamp(g_fps, 1, 240);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output framerate (AVIF timebase). Does not\n"
                          "change the animation's playback speed - the\n"
                          "renderer samples the running animation at\n"
                          "this cadence. 120 is the safest default\n"
                          "(matches the internal tick rate, so no\n"
                          "frames are skipped); drop to 30/60 for a\n"
                          "smaller file.");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt("quality##exp_q", &g_quality, 0, 100, "quality %d");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Output quality. 0 = smallest file with strong\n"
                          "artifacts, 100 = near lossless. Default 60.\n"
                          "Applies to every format: libaom CRF for AVIF\n"
                          "software, NVENC CQP for AVIF/WebM-AV1 hardware,\n"
                          "and libvpx CRF for WebM VP9.");
    }
}

void DrawKeyframeIntervalControl(MediaSink::Format current_format) {
    if (MediaSink::UsesKeyframeInterval(current_format)) {
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("keyframe interval##exp_keyint", &g_keyframe_interval, 1, 30);
        g_keyframe_interval = std::clamp(g_keyframe_interval, 0, 100000);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Frames between full keyframes (video formats only).\n"
                              "0 = auto (one keyframe per second). A larger value\n"
                              "means fewer keyframes and a smaller file - ideal for\n"
                              "static or slowly-scrolling scenes, which compress to\n"
                              "almost nothing between keyframes. Set it >= your total\n"
                              "frame count for a single keyframe (smallest file, but\n"
                              "slower to seek). PNG and WebP ignore this.");
        }
        ImGui::SameLine();
        if (g_keyframe_interval == 0) {
            ImGui::TextDisabled("(auto: 1/sec)");
        } else {
            ImGui::TextDisabled("(every %d frames)", g_keyframe_interval);
        }
    }
}

void DrawFrameLimitControls() {
    ImGui::Checkbox("Limit frames##exp_limit", &g_limit_frames);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When ON, stop the export after a fixed number of captured\n"
                          "frames (defaults to 60 = 1 second at 60 fps). When OFF\n"
                          "(default), the export runs until the master animation's\n"
                          "natural end-of-timeline.\n\n"
                          "Use cases: clamp a long title to a short preview, capture\n"
                          "a fixed-duration looped clip without waiting for AFP's\n"
                          "end-of-timeline detector, or get a deterministic file\n"
                          "size for tests.");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!g_limit_frames);
    if (g_limit_frames && g_max_frames <= 0) g_max_frames = 60;
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("frames##exp_maxf", &g_max_frames, 1, 10);
    g_max_frames = std::clamp(g_max_frames, 1, 100000);
    ImGui::SameLine();
    {
        const double secs = (double)g_max_frames / (double)(g_fps > 0 ? g_fps : 60);
        ImGui::TextDisabled("(~%.2fs at %d fps)", secs, g_fps > 0 ? g_fps : 60);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Number of frames the encoder receives before the export\n"
                          "auto-finalises. Counted post-capture: a value of 60\n"
                          "produces a file with exactly 60 encoded frames.");
    }
}

void DrawLoopControls() {
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Continuous loop count##exp_loops", &g_loop_count, 1, 5))
        g_loop_count = std::clamp(g_loop_count, 1, 1000);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How many loops of the animation to capture (default 1).\n"
                          "The exporter keeps recording past each detected loop\n"
                          "boundary until this many loops are in the output, so the\n"
                          "clip plays the loop N times. 'Limit frames' still caps it.");
    }

    ImGui::Checkbox("Blend loop seam##exp_blend", &g_blend_loop);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("For backgrounds that don't loop cleanly (e.g. one-shot intros like\n"
                          "select_bg_iv's _4bg). Buffers the captured cycle, picks the frame\n"
                          "closest to the start, and crossfades the wrap so it dissolves\n"
                          "instead of snapping. This is a synthesized loop the real game does\n"
                          "NOT do - leave off for backgrounds that already loop cleanly.");
    }
    if (g_blend_loop) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Blend frames##exp_blendN", &g_blend_frames, 1, 5))
            g_blend_frames = std::clamp(g_blend_frames, 0, 240);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Crossfade length in frames (default 15). Longer = a softer\n"
                              "dissolve at the wrap; shorter = crisper, with a more visible\n"
                              "seam mismatch. 0 = no crossfade (hard cut at the best frame).");
        }
    }
}

void DrawFpsQualityFrameCap(MediaSink::Format current_format) {
    DrawFpsQualitySliders();
    DrawKeyframeIntervalControl(current_format);
    DrawFrameLimitControls();
    DrawLoopControls();
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
        {.label = native_label, .w = 0, .h = 0},    {.label = "1920x1080", .w = 1920, .h = 1080},
        {.label = "1280x720", .w = 1280, .h = 720}, {.label = "1080x1920", .w = 1080, .h = 1920},
        {.label = "720x1280", .w = 720, .h = 1280}, {.label = "Custom", .w = -1, .h = -1},
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
        ImGui::SetTooltip("Output resolution. Smaller sizes shrink the file and\n"
                          "substantially reduce browser decode cost. \"Native\" tracks\n"
                          "the render size from Setup - if you change render res,\n"
                          "the export follows. The fixed presets pin the export\n"
                          "to that exact pixel dim regardless of render size.");
    }
}

void DrawResolutionInputs(int rw, int rh, int& w_disp, int& h_disp) {
    w_disp = (g_out_w > 0) ? g_out_w : rw;
    h_disp = (g_out_h > 0) ? g_out_h : rh;
    const float input_w = 100.0F;
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
         .tip = "Halve the current size (1080p->540p, 720p->360p).\n"
                "Aspect ratio preserved."},
        {.label = "x0.25##exp_scl",
         .mul = 0.25F,
         .tip = "Quarter the current size (1080p->270p, 720p->180p).\n"
                "Very small files; preview-quality output."},
        {.label = "x0.1##exp_scl",
         .mul = 0.1F,
         .tip = "One tenth size. Useful for thumbnail strips or\n"
                "very-low-bandwidth previews."},
    };
    ImGui::TextDisabled("Scale current x:");
    ImGui::SameLine();
    for (const auto& sb : kScales) {
        int new_w = (int)std::lroundf((float)w_disp * sb.mul);
        int new_h = (int)std::lroundf((float)h_disp * sb.mul);
        new_w = std::clamp(new_w, 64, 8192);
        new_h = std::clamp(new_h, 64, 8192);
        char btn_label[32];
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

    DrawResolutionPresetCombo(rw, rh);
    int w_disp = 0;
    int h_disp = 0;
    DrawResolutionInputs(rw, rh, w_disp, h_disp);
    DrawScaleButtons(w_disp, h_disp);
}

void DrawCrop(App::State& state) {
    App::CropRect const rect = state.GetCropRect();
    int xywh[4] = {rect.x, rect.y, rect.w, rect.h};
    const bool pick_mode = state.GetCropPickMode();

    ImGui::TextUnformatted("crop");

    const float avail = ImGui::GetContentRegionAvail().x;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float lbl_w = ImGui::CalcTextSize("x").x;
    const float per_overhead = lbl_w + st.ItemInnerSpacing.x + st.ItemSpacing.x;
    const float per_input_w = (std::max)(36.0F, (avail - per_overhead * 4.0F) / 4.0F);

    const char* labels[4] = {"x##crop_x", "y##crop_y", "w##crop_w", "h##crop_h"};
    bool changed = false;
    for (int i = 0; i < 4; i++) {
        ImGui::SetNextItemWidth(per_input_w);
        if (ImGui::InputInt(labels[i], &xywh[i], 0, 0, ImGuiInputTextFlags_AutoSelectAll)) {
            changed = true;
        }
        if (i < 3) ImGui::SameLine();
    }

    if (pick_mode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65F, 0.45F, 0.15F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75F, 0.55F, 0.22F, 1.0F));
        if (ImGui::Button("Picking...##crop_pick", ImVec2(110, 0))) {
            state.SetCropPickMode(false);
        }
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::Button("Pick region##crop_pick", ImVec2(110, 0))) {
            state.SetCropPickMode(true);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to arm crop-selection mode, then\n"
                          "click-and-drag on the render window to\n"
                          "draw a rectangle. Esc cancels. The export\n"
                          "will encode only pixels inside the rect.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##crop_clear", ImVec2(70, 0))) {
        state.SetCropRect({});
        state.SetCropPickMode(false);
        xywh[0] = xywh[1] = xywh[2] = xywh[3] = 0;
        changed = true;
    }

    if (changed) {
        App::CropRect r;
        r.x = (std::max)(0, xywh[0]);
        r.y = (std::max)(0, xywh[1]);
        r.w = (std::max)(0, xywh[2]);
        r.h = (std::max)(0, xywh[3]);
        state.SetCropRect(r);
    }
}

void DrawHwAccelTooltip(MediaSink::Format current_format, bool hw_available, bool is_h264) {
    if (!hw_available && is_h264) {
        ImGui::SetTooltip("h264_nvenc unavailable on this machine.\n"
                          "H.264 export needs an NVIDIA GPU with NVENC -\n"
                          "this ffmpeg build has no software H.264\n"
                          "encoder (no libx264 / openh264).");
    } else if (!hw_available) {
        ImGui::SetTooltip("Hardware acceleration unavailable on this\n"
                          "machine. Needs an NVIDIA GPU with AV1\n"
                          "encode support (RTX 40-series or newer)\n"
                          "and an ffmpeg build compiled with the\n"
                          "nvcodec feature enabled.");
    } else if (current_format == MediaSink::Format::WebM_VP9) {
        ImGui::SetTooltip("WebM VP9 has no hardware encoder path.\n"
                          "NVIDIA desktop GPUs don't ship a VP9\n"
                          "encoder - switch to AVIF or WebM-AV1\n"
                          "to enable NVENC.");
    } else if (current_format == MediaSink::Format::WebP_Anim) {
        ImGui::SetTooltip("WebP (libwebp_anim) is software only -\n"
                          "VP8 has no hardware encoder on consumer\n"
                          "GPUs. libwebp is fast in software\n"
                          "(~5-10x faster than libaom-av1).");
    } else if (current_format == MediaSink::Format::PNG_Sequence) {
        ImGui::SetTooltip("PNG sequence writes lossless per-frame\n"
                          "files through WIC. No encoder, no NVENC,\n"
                          "no quality slider - frames are written\n"
                          "directly from BGRA to disk.");
    } else if (is_h264) {
        ImGui::SetTooltip("Encode H.264 with NVENC (h264_nvenc) -\n"
                          "fast hardware encode, available on most\n"
                          "NVIDIA GPUs (not only RTX 40-series). Opaque\n"
                          "only. This ffmpeg build has no software H.264\n"
                          "encoder, so H.264 export requires NVENC.");
    } else {
        ImGui::SetTooltip("Encode the AV1 stream with NVENC\n"
                          "(av1_nvenc). ~50x faster than software\n"
                          "libaom-av1 on an RTX 40-series GPU.\n"
                          "For AVIF the alpha stream stays on\n"
                          "software - bitstream is tiny and NVENC\n"
                          "has no alpha path. Falls back to software\n"
                          "if NVENC init fails at runtime.");
    }
}

void DrawBackgroundAndHw(MediaSink::Format current_format, bool hw_available) {
    ImGui::Checkbox("Transparent bg", &g_bg_transparent);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable to export a real alpha channel (AVIF\n"
                          "plays with transparency). Disable to composite\n"
                          "a solid colour under the animation - useful\n"
                          "when the target viewer does not handle\n"
                          "animated AVIF transparency well.");
    }
    ImGui::SameLine();
    if (g_bg_transparent) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(160);
    ImGui::ColorEdit3("##exp_bg_color", g_bg_rgb,
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    if (g_bg_transparent) ImGui::EndDisabled();

    const bool is_h264 = (current_format == MediaSink::Format::MP4_H264);
    const bool format_can_use_hw = (current_format == MediaSink::Format::AVIF ||
                                    current_format == MediaSink::Format::WebM_AV1 || is_h264);
    const bool hw_applies = hw_available && format_can_use_hw;
    if (!hw_applies) {
        ImGui::BeginDisabled();
        bool tmp = false;
        ImGui::Checkbox("HW accel##exp_hw", &tmp);
        ImGui::EndDisabled();
    } else {
        ImGui::Checkbox("HW accel##exp_hw", &g_prefer_hw);
    }
    if (ImGui::IsItemHovered()) DrawHwAccelTooltip(current_format, hw_available, is_h264);
}

void PostStartRequest(App::State& state, MediaSink::Format current_format, bool hw_applies) {
    App::Request r;
    r.start_export = true;
    r.export_output_path = MediaSink::MakeOutputPath(g_stem_buf, current_format);
    r.export_fps = g_fps;
    r.export_quality = g_quality;
    r.export_keyframe_interval = g_keyframe_interval;
    r.export_max_frames = g_limit_frames ? g_max_frames : 0;
    r.export_loop_count = g_loop_count;
    r.export_blend_loop = g_blend_loop;
    r.export_blend_frames = g_blend_frames;
    r.export_bg_transparent = g_bg_transparent;
    r.export_bg_r = g_bg_rgb[0];
    r.export_bg_g = g_bg_rgb[1];
    r.export_bg_b = g_bg_rgb[2];
    r.export_width = g_out_w;
    r.export_height = g_out_h;
    App::CropRect const cr = state.GetCropRect();
    r.export_crop_x = cr.x;
    r.export_crop_y = cr.y;
    r.export_crop_w = cr.w;
    r.export_crop_h = cr.h;
    r.export_format = g_format_idx;
    r.export_prefer_hardware = g_prefer_hw && hw_applies;
    state.PostRequest(std::move(r));
}

void DrawStartAndStatus(App::State& state, const App::ExportState& ex, bool busy,
                        MediaSink::Format current_format, bool hw_available) {
    if (!busy) {
        if (ImGui::Button("Start export", ImVec2(120, 0))) {
            const bool format_can_use_hw = (current_format == MediaSink::Format::AVIF ||
                                            current_format == MediaSink::Format::WebM_AV1 ||
                                            current_format == MediaSink::Format::MP4_H264);
            const bool hw_applies = hw_available && format_can_use_hw;
            PostStartRequest(state, current_format, hw_applies);
        }
    } else {
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            App::Request r;
            r.cancel_export = true;
            state.PostRequest(std::move(r));
        }
    }

    switch (ex.phase) {
    case App::ExportPhase::Idle:
        ImGui::TextDisabled("idle");
        break;
    case App::ExportPhase::Capturing:
        ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.3F, 1.0F), "capturing frame %d...%s",
                           ex.frames_captured, ex.using_hardware ? "  [NVENC]" : "");
        break;
    case App::ExportPhase::Encoding: {
        const char* enc_label = MediaSink::FormatToken(MediaSink::FromIndex(ex.format));
        ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.3F, 1.0F), "encoding %s (%d frames)...%s",
                           enc_label, ex.frames_captured, ex.using_hardware ? "  [NVENC]" : "");
        ImGui::ProgressBar(-1.0F * (float)ImGui::GetTime(), ImVec2(-FLT_MIN, 0), "finalising");
        break;
    }
    case App::ExportPhase::Done:
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50F, 0.92F, 0.65F, 1.0F));
        ImGui::TextWrapped("done - %s", ex.output_path.c_str());
        ImGui::PopStyleColor();
        break;
    case App::ExportPhase::Failed:
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.45F, 0.45F, 1.0F));
        ImGui::TextWrapped("failed: %s", ex.error.c_str());
        ImGui::PopStyleColor();
        break;
    }
}

}

void RenderPanel() {
    auto& state = App::Global();
    App::ExportState const ex = state.GetExport();
    const bool busy =
        (ex.phase == App::ExportPhase::Capturing || ex.phase == App::ExportPhase::Encoding);

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.92F, 0.92F, 0.93F, 1.00F), "Export");

    MaybeRegenerateStem(state);

    const MediaSink::Format current_format = MediaSink::FromIndex(g_format_idx);
    const bool hw_available =
        VideoEncoder::HardwareAvailable(MediaSink::HardwareProbeFormat(current_format));

    if (busy) ImGui::BeginDisabled();
    DrawFilenameAndFormat();
    DrawFpsQualityFrameCap(current_format);
    DrawOutputResolution(state);
    DrawCrop(state);
    DrawBackgroundAndHw(current_format, hw_available);
    if (busy) ImGui::EndDisabled();

    DrawStartAndStatus(state, ex, busy, current_format, hw_available);
}

}
