#include "cli/cli.h"

#include "media/media_format.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Cli {

namespace {

constexpr const char* kUsage =
    "573Renderer - standalone AFP renderer\n\n"
    "Usage: 573Renderer.exe [options]\n\n"
    "Options:\n"
    "  --game-dir <path>       Boot directly against this game directory\n"
    "                          and skip the GUI Setup screen. Saved to\n"
    "                          settings.ini for next launch.\n"
    "  --profile <slug>        Select a GameProfile (iidx33, sdvx7, ...)\n"
    "                          Overrides settings.ini. Default = auto-\n"
    "                          detect from --game-dir at boot.\n"
    "  --render-size <WxH>     Native resolution of the game (window +\n"
    "                          offscreen RT). Defaults to settings.ini's\n"
    "                          last value (1920x1080 on first run).\n"
    "                          Use 1280x720 for IIDX 17-24, 1024x768 for\n"
    "                          jubeat, etc.\n"
    "  --fps <N>               Live render / animation-tick rate (1..1000).\n"
    "                          Defaults to settings.ini's render_fps (120).\n"
    "                          Animation speed is unchanged (dt=1/fps).\n"
    "  --ifs <path>            Load this IFS file at startup (absolute or\n"
    "                          relative path inside the game dir). Skips\n"
    "                          the GUI picker.\n"
    "  --animation <name>      After --ifs loads, switch to this named\n"
    "                          animation BEFORE any --export starts.\n"
    "                          Use this when the IFS ships multiple\n"
    "                          animations (e.g. select_bg_booth.ifs has\n"
    "                          both 'select_bg_booth_bg' and 'select_bg').\n"
    "  --animation-label <l>   After --animation switch, deep-goto the\n"
    "                          'l' label on the master movie clip. SDVX\n"
    "                          uses this for backgrounds with intro+loop\n"
    "                          structure - e.g. bg_bpls5 takes 'loop' to\n"
    "                          skip the intro and start in the steady\n"
    "                          looped state.\n"
    "  --scale <factor>        Master scale applied to the AFP stream\n"
    "                          before the export starts. Required for SDVX-I-\n"
    "                          IV 720x1280 select_bg variants - they need\n"
    "                          1.5 to fill a 1080x1920 viewport.\n"
    "  --seek-frame <N>        After --ifs (and --animation) load, seek the\n"
    "                          master clip to absolute frame N and play\n"
    "                          (CAfpViewerScene TIME seek, afp_mc_control\n"
    "                          0xF08). Pauses on seek. <0 = no seek.\n"
    "  --start-paused          Start the live preview paused (stream speed\n"
    "                          0; debug viewer RETURN+SHIFT pause).\n"
    "  --filter                Enable the AFP layer filter at startup\n"
    "                          (debug viewer F7, afp-core set-filter 0x032).\n"
    "  --show-mc-names         Enable the MC-name overlay (debug viewer F3):\n"
    "                          enumerate the master's child clips into the\n"
    "                          GUI list.\n"
    "  --mc-name-type <0|1>    F6 MC NAME TYPE: 0 = at clip position,\n"
    "                          1 = fixed column. Needs --show-mc-names.\n"
    "  --variant <path>=<bitmap>\n"
    "                          Override a clip slot: swap its bitmap and\n"
    "                          make it visible. Repeatable.\n"
    "                            e.g. --variant coin=start\n"
    "  --hide <path>           Make a clip slot invisible. Repeatable.\n"
    "  --hide-sublayer <path>  Force a named child sub-clip (and its same-\n"
    "                          name siblings) invisible. Repeatable.\n"
    "                            e.g. --hide-sublayer content_usr\n"
    "  --show-sublayer <path>  Force a named child sub-clip visible.\n"
    "                          Repeatable.\n"
    "  --no-gui                Launch the render window without the ImGui\n"
    "                          control panel - useful for headless tests.\n"
    "  --headless              Skip window creation entirely; run only the\n"
    "                          init sequence and exit.\n"
    "  --deferred-replay       Record each frame's render commands and replay\n"
    "                          them in one pass at end of frame instead of\n"
    "                          issuing device calls inline. Debug/CI mode for\n"
    "                          the command-stream executor - pixels must be\n"
    "                          identical to the default live mode.\n"
    "  --swap-after-frames <N>\n"
    "                          After N frames, hot-swap to --ifs2 <path>.\n"
    "                          Zero means never. Useful for --no-gui\n"
    "                          self-tests of the unload/load path.\n"
    "  --ifs2 <path>           Second IFS to load at the swap point.\n"
    "  --exit-after-frames <N>\n"
    "                          Exit cleanly after N frames. Zero means\n"
    "                          run forever (until window closed / ESC).\n"
    "  --screenshot-frames <f1,f2,...>\n"
    "                          Dump the backbuffer as PNG at each listed\n"
    "                          frame (1-indexed). Saved to\n"
    "                          <--screenshot-prefix><frame>.png.\n"
    "  --screenshot-prefix <p> Prefix for --screenshot-frames output.\n"
    "                          Default: \"screenshots/auto_f\".\n"
    "  --export <path>         Start export after startup IFS loads,\n"
    "                          then exit once encoding finishes.\n"
    "  --export-fps <N>        Output framerate. Default 60.\n"
    "  --export-quality <0-100>\n"
    "                          Quality. Mapped to codec CRF. Default 60.\n"
    "  --export-keyframe-interval <N>\n"
    "                          Frames between video keyframes (video formats\n"
    "                          only). 0 (default) = one per second. A larger\n"
    "                          value or a value >= your frame count yields a\n"
    "                          single keyframe = smallest file for static or\n"
    "                          scrolling scenes. Ignored by PNG and WebP.\n"
    "  --export-max-frames <N> Stop the export after N captured frames.\n"
    "  --export-loop-count <N> Capture N loops of the animation (default 1).\n"
    "                          0 (default) = capture until animation ends.\n"
    "  --root-loop <hold|force>\n"
    "                          How the modern scene-BG path treats a root\n"
    "                          timeline that reaches its end. 'hold' (the\n"
    "                          game default) mounts once and lets the\n"
    "                          root play once + HOLD while nested children\n"
    "                          free-run (fixes the select_bg o_kazari5-\n"
    "                          style child snap). 'force' re-drives the\n"
    "                          root (ForceReplay + the continuous-loop\n"
    "                          flag sequence) for one-shot masters\n"
    "                          (bg_common) or user-forced loops.\n"
    "                          Default = settings.ini (hold).\n"
    "  --blend-loop            Smooth-loop: find the frame closest to the start\n"
    "                          and crossfade the seam (for backgrounds with\n"
    "                          no clean loop).\n"
    "  --blend-frames <N>      Crossfade length for --blend-loop (default 15).\n"
    "  --export-size <WxH>     Scale before encode. 0x0 = native.\n"
    "  --export-format <avif|webm|webm-av1|webp|png|mp4>\n"
    "                          Output container/codec.\n"
    "                            avif     : AV1 dual-stream, alpha,\n"
    "                                       NVENC-accelerated (default).\n"
    "                            webm     : WebM VP9 yuva420p, alpha,\n"
    "                                       software-only.\n"
    "                            webm-av1 : WebM AV1, OPAQUE, NVENC.\n"
    "                            webp     : animated WebP, alpha, SW.\n"
    "                            png      : folder of frame_NNNNNN.png\n"
    "                                       (lossless, per-frame files).\n"
    "                            mp4      : MP4 H.264, OPAQUE, h264_nvenc\n"
    "                                       or libx264 - most compatible.\n"
    "  --export-no-hw          Disable NVENC even if available. Falls\n"
    "                          back to software libaom. Applies to\n"
    "                          avif + webm-av1; no-op for webm-vp9.\n"
    "  --export-bg <t|R,G,B>   Background colour. 'transparent' or\n"
    "                          a 0..255 R,G,B triple (e.g. 30,33,43).\n"
    "  --export-dump-frames <d>\n"
    "                          Dump pre-encode BGRA frames to d/ for\n"
    "                          codec-diff regression tests.\n"
    "  --submonitor-frames <p1,p2,...>\n"
    "                          Comma-separated loose image files to bind to\n"
    "                          the startup IFS's submonitor placeholder clip\n"
    "                          after the --animation switch, before --export.\n"
    "                          Frame i -> i-th child layer via afp ord 0x088.\n"
    "                          Drives the SDVX submonitor slideshow/pan render.\n"
    "  --submonitor-clip <path>\n"
    "                          Placeholder clip for --submonitor-frames.\n"
    "                          Default 'subbg_usr/bg_usr' (the game's target).\n"
    "  --submonitor-slideshow  Cross-fade CYCLE mode (oversized r3_fade 2-layer\n"
    "                          dissolve). Needs --submonitor-loop-frames.\n"
    "  --submonitor-slideshow-fade\n"
    "                          NORMAL slideshow mode: centered subbg_usr +\n"
    "                          subbg_0001 fade_in/fade_out labels (no pan).\n"
    "                          Use --animation subbg_0001 --submonitor-clip subbg_usr.\n"
    "  --submonitor-dwell-frames <N>  Hold per frame (default 720).\n"
    "  --submonitor-fade-frames <N>   Fade in/out length (default 120).\n"
    "  --dump-anim-info <out.json>\n"
    "                          After the startup --ifs loads, switch to every\n"
    "                          animation the IFS lists, read its master-clip\n"
    "                          total frame count and timeline labels from the\n"
    "                          game's own afp engine, write them as JSON, and\n"
    "                          exit. Use with --no-gui --game-dir --ifs.\n"
    "  --help                  Show this message.\n";

enum class Handled : std::uint8_t { Ok, Error, NotMine };

struct Cursor {
    std::span<char* const> args;
    int i = 0;
};

bool NextArg(Cursor& c, std::string_view name, std::string& out, std::string& err) {
    if (c.i + 1 >= static_cast<int>(c.args.size())) {
        err = std::string("Missing value for ").append(name);
        return false;
    }
    out = c.args[static_cast<std::size_t>(++c.i)];
    return true;
}

int ParseIntOrZero(std::string_view v) {
    int out = 0;
    std::from_chars(v.data(), std::to_address(v.end()), out);
    return out;
}

bool ParseFloatValue(std::string_view v, float& out) {
    out = 0.0F;
    const auto r = std::from_chars(v.data(), std::to_address(v.end()), out);
    return r.ec == std::errc{};
}

bool ParseSizePair(const std::string& v, int& w, int& h) {
    const std::string_view sv(v);
    const std::size_t x = sv.find('x');
    if (x == std::string_view::npos) return false;
    const std::string_view left = sv.substr(0, x);
    const std::string_view right = sv.substr(x + 1);
    int pw = 0;
    int ph = 0;
    const auto rl = std::from_chars(left.data(), std::to_address(left.end()), pw);
    const auto rr = std::from_chars(right.data(), std::to_address(right.end()), ph);
    if (rl.ec != std::errc{} || rr.ec != std::errc{}) return false;
    w = pw;
    h = ph;
    return true;
}

std::vector<std::string> SplitCsv(const std::string& v) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < v.size()) {
        const std::size_t comma = v.find(',', pos);
        std::string tok = v.substr(pos, comma - pos);
        if (!tok.empty()) out.push_back(std::move(tok));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

bool ParseIntList(const std::string& v, std::vector<int>& out, std::size_t expected) {
    const std::vector<std::string> toks = SplitCsv(v);
    if (toks.size() != expected) return false;
    out.clear();
    for (const std::string& tok : toks) {
        int n = 0;
        const auto r = std::from_chars(tok.data(), std::to_address(tok.end()), n);
        if (r.ec != std::errc{}) return false;
        out.push_back(n);
    }
    return true;
}

struct BoolOpt {
    std::string_view name;
    bool Options::* member;
    bool value;
};

struct StringOpt {
    std::string_view name;
    std::string Options::* member;
};

struct IntOpt {
    std::string_view name;
    int Options::* member;
};

struct ClampedIntOpt {
    std::string_view name;
    int Options::* member;
    int min_ok;
    int fallback;
};

struct RangedIntOpt {
    std::string_view name;
    int Options::* member;
    int lo;
    int hi;
};

using SpecialFn = Handled (*)(Cursor&, Options&, std::string&);

struct SpecialOpt {
    std::string_view name;
    SpecialFn fn;
};

constexpr std::array<BoolOpt, 14> kBoolOpts = {{
    {.name = "--headless", .member = &Options::headless, .value = true},
    {.name = "--no-gui", .member = &Options::no_gui, .value = true},
    {.name = "--deferred-replay", .member = &Options::deferred_replay, .value = true},
    {.name = "--boot-ifses", .member = &Options::boot_ifses, .value = true},
    {.name = "--start-paused", .member = &Options::start_paused, .value = true},
    {.name = "--filter", .member = &Options::filter_enabled, .value = true},
    {.name = "--show-mc-names", .member = &Options::show_mc_names, .value = true},
    {.name = "--qpro-no-hue-scope", .member = &Options::qpro_no_hue_scope, .value = true},
    {.name = "--blend-loop", .member = &Options::export_blend_loop, .value = true},
    {.name = "--submonitor-slideshow", .member = &Options::submonitor_slideshow, .value = true},
    {.name = "--submonitor-swap-layers", .member = &Options::submonitor_swap_layers, .value = true},
    {.name = "--submonitor-slideshow-fade",
     .member = &Options::submonitor_slideshow_fade,
     .value = true},
    {.name = "--export-no-hw", .member = &Options::export_prefer_hardware, .value = false},
    {.name = "--export-sw", .member = &Options::export_prefer_hardware, .value = false},
}};

constexpr std::array<StringOpt, 29> kStringOpts = {{
    {.name = "--game-dir", .member = &Options::game_dir},
    {.name = "--profile", .member = &Options::game_profile},
    {.name = "--ifs", .member = &Options::startup_ifs},
    {.name = "--ifs2", .member = &Options::swap_ifs},
    {.name = "--animation", .member = &Options::animation_name},
    {.name = "--animation-label", .member = &Options::animation_label},
    {.name = "--goto-label", .member = &Options::goto_label},
    {.name = "--extract-qpro", .member = &Options::extract_qpro_dir},
    {.name = "--qpro-parts", .member = &Options::qpro_parts},
    {.name = "--qpro-only", .member = &Options::qpro_only},
    {.name = "--qpro-dump", .member = &Options::qpro_dump_ifs},
    {.name = "--qpro-body-one", .member = &Options::qpro_body_one},
    {.name = "--qpro-back-one", .member = &Options::qpro_back_one},
    {.name = "--qpro-head-one", .member = &Options::qpro_head_one},
    {.name = "--qpro-hand-one", .member = &Options::qpro_hand_one},
    {.name = "--qpro-hair-one", .member = &Options::qpro_hair_one},
    {.name = "--qpro-face-one", .member = &Options::qpro_face_one},
    {.name = "--qpro-clip-one", .member = &Options::qpro_clip_one},
    {.name = "--qpro-hand-composite", .member = &Options::qpro_hand_composite},
    {.name = "--qpro-head-composite", .member = &Options::qpro_head_composite},
    {.name = "--qpro-back-composite", .member = &Options::qpro_back_composite},
    {.name = "--screenshot-prefix", .member = &Options::screenshot_prefix},
    {.name = "--export", .member = &Options::export_path},
    {.name = "--export-dump-frames", .member = &Options::export_dump_frames_dir},
    {.name = "--dump-anim-info", .member = &Options::dump_anim_info},
    {.name = "--cmd-trace", .member = &Options::cmd_trace_path},
    {.name = "--submonitor-clip", .member = &Options::submonitor_clip},
    {.name = "--submonitor-fade-in-label", .member = &Options::submonitor_fade_in_label},
    {.name = "--submonitor-fade-out-label", .member = &Options::submonitor_fade_out_label},
}};

constexpr std::array<IntOpt, 9> kIntOpts = {{
    {.name = "--continuous-loop", .member = &Options::continuous_loop_mode},
    {.name = "--swap-after-frames", .member = &Options::swap_after_frames},
    {.name = "--exit-after-frames", .member = &Options::exit_after_frames},
    {.name = "--export-fps", .member = &Options::export_fps},
    {.name = "--export-quality", .member = &Options::export_quality},
    {.name = "--export-keyframe-interval", .member = &Options::export_keyframe_interval},
    {.name = "--submonitor-loop-frames", .member = &Options::submonitor_loop_frames},
    {.name = "--submonitor-dwell-frames", .member = &Options::submonitor_dwell_frames},
    {.name = "--submonitor-fade-frames", .member = &Options::submonitor_fade_frames},
}};

constexpr std::array<ClampedIntOpt, 4> kClampedIntOpts = {{
    {.name = "--export-max-frames",
     .member = &Options::export_max_frames,
     .min_ok = 0,
     .fallback = 0},
    {.name = "--export-loop-count",
     .member = &Options::export_loop_count,
     .min_ok = 1,
     .fallback = 1},
    {.name = "--blend-frames", .member = &Options::export_blend_frames, .min_ok = 0, .fallback = 0},
    {.name = "--seek-frame", .member = &Options::seek_frame, .min_ok = 0, .fallback = -1},
}};

constexpr std::array<RangedIntOpt, 2> kRangedIntOpts = {{
    {.name = "--fps", .member = &Options::render_fps, .lo = 1, .hi = 1000},
    {.name = "--qpro-fps", .member = &Options::qpro_fps, .lo = 1, .hi = 240},
}};

Handled HandleRenderSize(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--render-size", v, err)) return Handled::Error;
    int w = 0;
    int h = 0;
    if (!ParseSizePair(v, w, h) || w <= 0 || h <= 0) {
        err = "--render-size expects 'WxH' with positive ints, got '" + v + "'";
        return Handled::Error;
    }
    if (w < 64 || h < 64 || w > 8192 || h > 8192) {
        err = "--render-size out of range (need 64..8192 each), got '" + v + "'";
        return Handled::Error;
    }
    out.render_width = w;
    out.render_height = h;
    return Handled::Ok;
}

Handled HandleExportSize(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--export-size", v, err)) return Handled::Error;
    int w = 0;
    int h = 0;
    if (!ParseSizePair(v, w, h) || w < 0 || h < 0) {
        err = "--export-size expects 'WxH' with non-negative ints, got '" + v + "'";
        return Handled::Error;
    }
    out.export_width = w;
    out.export_height = h;
    return Handled::Ok;
}

Handled HandleScale(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--scale", v, err)) return Handled::Error;
    float f = 0.0F;
    if (!ParseFloatValue(v, f) || f <= 0.0F || f > 16.0F) {
        err = "--scale expects a positive factor (e.g. 1.5), got '" + v + "'";
        return Handled::Error;
    }
    out.master_scale = f;
    return Handled::Ok;
}

Handled HandleAfpSpeed(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--afp-speed", v, err)) return Handled::Error;
    float f = 0.0F;
    if (!ParseFloatValue(v, f) || f < 0.0F || f > 16.0F) {
        err = "--afp-speed expects 0..16 (e.g. 0.5 for the 60fps submonitor), got '" + v + "'";
        return Handled::Error;
    }
    out.afp_speed = f;
    return Handled::Ok;
}

Handled HandleRootLoop(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--root-loop", v, err)) return Handled::Error;
    if (v == "hold") {
        out.root_loop_mode = 0;
        return Handled::Ok;
    }
    if (v == "force") {
        out.root_loop_mode = 1;
        return Handled::Ok;
    }
    err = "--root-loop expects 'hold' or 'force', got '" + v + "'";
    return Handled::Error;
}

Handled HandleMcNameType(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--mc-name-type", v, err)) return Handled::Error;
    out.mc_name_type = (ParseIntOrZero(v) != 0) ? 1 : 0;
    return Handled::Ok;
}

Handled HandleExportFormat(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--export-format", v, err)) return Handled::Error;
    MediaSink::Format mf = MediaSink::Format::AVIF;
    if (!MediaSink::ParseToken(v.c_str(), &mf)) {
        err = "--export-format expects 'avif', 'webm' (=webm-vp9), 'webm-av1', 'webp', 'png', or "
              "'mp4', got '" +
              v + "'";
        return Handled::Error;
    }
    out.export_format = MediaSink::ToIndex(mf);
    return Handled::Ok;
}

Handled HandleExportCrop(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--export-crop", v, err)) return Handled::Error;
    std::vector<int> n;
    if (!ParseIntList(v, n, 4) || n[0] < 0 || n[1] < 0 || n[2] < 0 || n[3] < 0) {
        err = "--export-crop expects 'X,Y,W,H' with non-negative ints, got '" + v + "'";
        return Handled::Error;
    }
    out.export_crop_x = n[0];
    out.export_crop_y = n[1];
    out.export_crop_w = n[2];
    out.export_crop_h = n[3];
    return Handled::Ok;
}

Handled HandleExportBg(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--export-bg", v, err)) return Handled::Error;
    if (v == "transparent" || v == "none") {
        out.export_bg_transparent = true;
        return Handled::Ok;
    }
    std::vector<int> n;
    if (!ParseIntList(v, n, 3)) {
        err = "--export-bg expects 'transparent' or 'R,G,B' with 0..255 ints, got '" + v + "'";
        return Handled::Error;
    }
    out.export_bg_transparent = false;
    out.export_bg_r = static_cast<float>(n[0]) / 255.0F;
    out.export_bg_g = static_cast<float>(n[1]) / 255.0F;
    out.export_bg_b = static_cast<float>(n[2]) / 255.0F;
    return Handled::Ok;
}

Handled HandleScreenshotFrames(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--screenshot-frames", v, err)) return Handled::Error;
    for (const std::string& tok : SplitCsv(v)) {
        const int n = ParseIntOrZero(tok);
        if (n > 0) out.screenshot_frames.push_back(n);
    }
    return Handled::Ok;
}

Handled HandleSubmonitorFrames(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--submonitor-frames", v, err)) return Handled::Error;
    for (std::string& tok : SplitCsv(v))
        out.submonitor_frames.push_back(std::move(tok));
    return Handled::Ok;
}

Handled HandleVariant(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--variant", v, err)) return Handled::Error;
    const std::size_t eq = v.find('=');
    if (eq == std::string::npos) {
        err = "--variant expects <path>=<bitmap>, got '" + v + "'";
        return Handled::Error;
    }
    out.slot_overrides.push_back(
        {.path = v.substr(0, eq), .visible = true, .bitmap = v.substr(eq + 1)});
    return Handled::Ok;
}

Handled HandleHide(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--hide", v, err)) return Handled::Error;
    out.slot_overrides.push_back({.path = v, .visible = false, .bitmap = ""});
    return Handled::Ok;
}

Handled HandleHideSublayer(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--hide-sublayer", v, err)) return Handled::Error;
    out.sublayer_overrides.push_back({.path = v, .visible = false});
    return Handled::Ok;
}

Handled HandleShowSublayer(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--show-sublayer", v, err)) return Handled::Error;
    out.sublayer_overrides.push_back({.path = v, .visible = true});
    return Handled::Ok;
}

constexpr std::array<SpecialOpt, 15> kSpecialOpts = {{
    {.name = "--render-size", .fn = HandleRenderSize},
    {.name = "--export-size", .fn = HandleExportSize},
    {.name = "--scale", .fn = HandleScale},
    {.name = "--afp-speed", .fn = HandleAfpSpeed},
    {.name = "--root-loop", .fn = HandleRootLoop},
    {.name = "--mc-name-type", .fn = HandleMcNameType},
    {.name = "--export-format", .fn = HandleExportFormat},
    {.name = "--export-crop", .fn = HandleExportCrop},
    {.name = "--export-bg", .fn = HandleExportBg},
    {.name = "--screenshot-frames", .fn = HandleScreenshotFrames},
    {.name = "--submonitor-frames", .fn = HandleSubmonitorFrames},
    {.name = "--variant", .fn = HandleVariant},
    {.name = "--hide", .fn = HandleHide},
    {.name = "--hide-sublayer", .fn = HandleHideSublayer},
    {.name = "--show-sublayer", .fn = HandleShowSublayer},
}};

Handled TryBoolOpts(const std::string& a, Options& out) {
    for (const BoolOpt& o : kBoolOpts) {
        if (a == o.name) {
            out.*(o.member) = o.value;
            return Handled::Ok;
        }
    }
    return Handled::NotMine;
}

Handled TryStringOpts(const std::string& a, Cursor& c, Options& out, std::string& err) {
    for (const StringOpt& o : kStringOpts) {
        if (a == o.name) {
            return NextArg(c, o.name, out.*(o.member), err) ? Handled::Ok : Handled::Error;
        }
    }
    return Handled::NotMine;
}

Handled TryIntOpts(const std::string& a, Cursor& c, Options& out, std::string& err) {
    for (const IntOpt& o : kIntOpts) {
        if (a == o.name) {
            std::string v;
            if (!NextArg(c, o.name, v, err)) return Handled::Error;
            out.*(o.member) = ParseIntOrZero(v);
            return Handled::Ok;
        }
    }
    for (const ClampedIntOpt& o : kClampedIntOpts) {
        if (a == o.name) {
            std::string v;
            if (!NextArg(c, o.name, v, err)) return Handled::Error;
            const int n = ParseIntOrZero(v);
            out.*(o.member) = (n < o.min_ok) ? o.fallback : n;
            return Handled::Ok;
        }
    }
    for (const RangedIntOpt& o : kRangedIntOpts) {
        if (a == o.name) {
            std::string v;
            if (!NextArg(c, o.name, v, err)) return Handled::Error;
            const int n = ParseIntOrZero(v);
            if (n < o.lo || n > o.hi) {
                err = std::string(o.name)
                          .append(" out of range (")
                          .append(std::to_string(o.lo))
                          .append("..")
                          .append(std::to_string(o.hi))
                          .append("), got '")
                          .append(v)
                          .append("'");
                return Handled::Error;
            }
            out.*(o.member) = n;
            return Handled::Ok;
        }
    }
    return Handled::NotMine;
}

Handled TrySpecialOpts(const std::string& a, Cursor& c, Options& out, std::string& err) {
    for (const SpecialOpt& o : kSpecialOpts) {
        if (a == o.name) return o.fn(c, out, err);
    }
    return Handled::NotMine;
}

}

void PrintUsage() {
    std::fputs(kUsage, stdout);
}

bool Parse(int argc, char** argv, Options& out, std::string& err) {
    Cursor c{.args = std::span<char* const>(argv, static_cast<std::size_t>(argc)), .i = 0};
    for (c.i = 1; c.i < argc; c.i++) {
        const std::string a = c.args[static_cast<std::size_t>(c.i)];
        if (a == "--help" || a == "-h" || a == "/?") {
            PrintUsage();
            return false;
        }
        Handled res = TryBoolOpts(a, out);
        if (res == Handled::NotMine) res = TryStringOpts(a, c, out, err);
        if (res == Handled::NotMine) res = TryIntOpts(a, c, out, err);
        if (res == Handled::NotMine) res = TrySpecialOpts(a, c, out, err);
        if (res == Handled::Error) return false;
        if (res == Handled::NotMine) {
            err = "Unknown argument: " + a;
            return false;
        }
    }
    return true;
}

}
