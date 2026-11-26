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
    "573Renderer: standalone AFP renderer\n\n"
    "Usage: 573Renderer.exe [options]\n\n"
    "Options:\n"
    "  --game-dir <path>       Use this game directory and skip the setup\n"
    "                          screen. Saved to settings.ini for the next\n"
    "                          launch.\n"
    "  --profile <slug>        Game profile to use (iidx33, sdvx7, ...).\n"
    "                          Overrides settings.ini. By default the\n"
    "                          profile is detected from --game-dir.\n"
    "  --render-size <WxH>     The game's native resolution, used for the\n"
    "                          window and the offscreen render target.\n"
    "                          Defaults to the last value in settings.ini\n"
    "                          (1920x1080 on first run). Use 1280x720 for\n"
    "                          IIDX 20-29, 1024x768 for jubeat, and so on.\n"
    "  --fps <N>               Live render and animation tick rate\n"
    "                          (1..1000). Defaults to render_fps in\n"
    "                          settings.ini (120). Animation speed stays\n"
    "                          the same (dt = 1/fps).\n"
    "  --ifs <path>            Load this IFS at startup instead of picking\n"
    "                          one in the GUI. Absolute, or relative to the\n"
    "                          game directory.\n"
    "  --load-alongside <path> Load an extra IFS on top of --ifs so its\n"
    "                          bitmaps fill in names the main IFS lacks\n"
    "                          (the last package loaded wins a name).\n"
    "                          Repeatable; loaded in the order given.\n"
    "  --animation <name>      After --ifs loads, switch to this animation\n"
    "                          before any --export starts. Use it when the\n"
    "                          IFS has several animations (select_bg_booth.ifs\n"
    "                          has 'select_bg_booth_bg' and 'select_bg').\n"
    "  --animation-label <l>   After the --animation switch, go to label 'l'\n"
    "                          on the master clip. SDVX backgrounds with an\n"
    "                          intro and a loop use this: bg_bpls5 takes\n"
    "                          'loop' to skip the intro and start looping.\n"
    "  --scale <factor>        Master scale applied to the AFP stream before\n"
    "                          the export starts. The 720x1280 select_bg\n"
    "                          variants of SDVX I-IV need 1.5 to fill a\n"
    "                          1080x1920 viewport.\n"
    "  --seek-frame <N>        After --ifs (and --animation) load, seek the\n"
    "                          master clip to frame N, as the debug viewer's\n"
    "                          time seek does (afp_mc_control 0xF08). The\n"
    "                          preview pauses after the seek. Below 0 means\n"
    "                          no seek.\n"
    "  --start-paused          Start the live preview paused (stream speed 0,\n"
    "                          like RETURN+SHIFT in the debug viewer).\n"
    "  --filter                Turn on the AFP layer filter at startup\n"
    "                          (F7 in the debug viewer, afp-core set-filter\n"
    "                          0x032).\n"
    "  --show-mc-names         Turn on the MC name overlay (F3 in the debug\n"
    "                          viewer) and list the master's child clips in\n"
    "                          the GUI.\n"
    "  --mc-name-type <0|1>    MC name placement (F6 in the debug viewer):\n"
    "                          0 = at the clip's position, 1 = in a fixed\n"
    "                          column. Needs --show-mc-names.\n"
    "  --variant <path>=<bitmap>\n"
    "                          Override a clip slot: swap its bitmap and\n"
    "                          make it visible. Repeatable.\n"
    "                            e.g. --variant coin=start\n"
    "  --hide <path>           Hide a clip slot. Repeatable.\n"
    "  --hide-sublayer <path>  Hide a named child clip and its siblings with\n"
    "                          the same name. Repeatable.\n"
    "                            e.g. --hide-sublayer content_usr\n"
    "  --show-sublayer <path>  Show a named child clip. Repeatable.\n"
    "  --no-gui                Open the render window without the ImGui\n"
    "                          control panel, for headless tests.\n"
    "  --headless              Create no window: run the init sequence and\n"
    "                          exit.\n"
    "  --deferred-replay       Record each frame's render commands and replay\n"
    "                          them in one pass at the end of the frame\n"
    "                          instead of issuing device calls as they come.\n"
    "                          A debug and CI mode for the command-stream\n"
    "                          executor; its pixels must match the default\n"
    "                          live mode.\n"
    "  --swap-after-frames <N>\n"
    "                          After N frames, swap to the IFS given by\n"
    "                          --ifs2. 0 means never. Used by --no-gui\n"
    "                          self-tests of unloading and loading.\n"
    "  --ifs2 <path>           The IFS to load at the swap point.\n"
    "  --exit-after-frames <N>\n"
    "                          Exit after N frames. 0 means run until the\n"
    "                          window is closed or ESC is pressed.\n"
    "  --screenshot-frames <f1,f2,...>\n"
    "                          Save the backbuffer as a PNG at each listed\n"
    "                          frame (counted from 1), to\n"
    "                          <--screenshot-prefix><frame>.png.\n"
    "  --screenshot-prefix <p> Prefix for --screenshot-frames files.\n"
    "                          Default: \"screenshots/auto_f\".\n"
    "  --export <path>         Start an export once the startup IFS loads,\n"
    "                          and exit when encoding finishes.\n"
    "  --export-fps <N>        Output frame rate. Default 60.\n"
    "  --export-quality <0-100>\n"
    "                          Quality, mapped to the codec's CRF. Default 60.\n"
    "  --export-keyframe-interval <N>\n"
    "                          Frames between keyframes (video formats only).\n"
    "                          0 (the default) means one per second. A value\n"
    "                          at or above the frame count gives a single\n"
    "                          keyframe, the smallest file for static or\n"
    "                          scrolling scenes. PNG and WebP ignore it.\n"
    "  --export-max-frames <N> Stop the export after N captured frames.\n"
    "  --export-loop-count <N> Capture N loops of the animation (default 1).\n"
    "  --root-loop <hold|force>\n"
    "                          What the modern scene background path does\n"
    "                          when the root timeline reaches its end.\n"
    "                          'hold' (the game's default) mounts once, plays\n"
    "                          the root once and holds it while nested\n"
    "                          children keep running (this fixes the child\n"
    "                          snap in select_bg's o_kazari5). 'force'\n"
    "                          replays the root (ForceReplay plus the\n"
    "                          continuous-loop flag sequence), for one-shot\n"
    "                          masters such as bg_common or to force a loop.\n"
    "                          Default: settings.ini (hold).\n"
    "  --blend-loop            Find the frame closest to the start and\n"
    "                          crossfade the seam, for backgrounds with no\n"
    "                          clean loop.\n"
    "  --blend-frames <N>      Crossfade length for --blend-loop (default 15).\n"
    "  --export-size <WxH>     Scale before encoding. 0x0 = native size.\n"
    "  --export-format <avif|webm|webm-av1|webp|png|mp4>\n"
    "                          Output container and codec.\n"
    "                            avif     : AV1 with an alpha stream,\n"
    "                                       NVENC when available (default).\n"
    "                            webm     : WebM VP9 yuva420p with alpha,\n"
    "                                       software only.\n"
    "                            webm-av1 : WebM AV1, no alpha, NVENC.\n"
    "                            webp     : animated WebP with alpha,\n"
    "                                       software.\n"
    "                            png      : a folder of frame_NNNNNN.png\n"
    "                                       (lossless, one file per frame).\n"
    "                            mp4      : MP4 H.264, no alpha, h264_nvenc\n"
    "                                       or libx264. Plays almost anywhere.\n"
    "  --export-no-hw          Do not use NVENC even when it is available;\n"
    "                          use software libaom instead. Applies to avif\n"
    "                          and webm-av1. webm-vp9 is always software.\n"
    "  --export-bg <t|R,G,B>   Background colour: 'transparent', or an R,G,B\n"
    "                          triple of 0..255 values (e.g. 30,33,43).\n"
    "  --export-dump-frames <d>\n"
    "                          Write the BGRA frames to d/ before encoding,\n"
    "                          for codec regression tests.\n"
    "  --submonitor-frames <p1,p2,...>\n"
    "                          Comma-separated image files to bind to the\n"
    "                          startup IFS's submonitor placeholder clip,\n"
    "                          after the --animation switch and before\n"
    "                          --export. Image i goes to the i-th child\n"
    "                          layer (afp ordinal 0x088). Drives the SDVX\n"
    "                          submonitor slideshow and pan render.\n"
    "  --submonitor-clip <path>\n"
    "                          Placeholder clip for --submonitor-frames.\n"
    "                          Default 'subbg_usr/bg_usr', the clip the game\n"
    "                          uses.\n"
    "  --submonitor-slideshow  Crossfade cycle mode (an oversized two-layer\n"
    "                          r3_fade dissolve). Needs\n"
    "                          --submonitor-loop-frames.\n"
    "  --submonitor-slideshow-fade\n"
    "                          Normal slideshow mode: subbg_usr centred, with\n"
    "                          the subbg_0001 fade_in and fade_out labels and\n"
    "                          no pan. Use with --animation subbg_0001\n"
    "                          --submonitor-clip subbg_usr.\n"
    "  --submonitor-dwell-frames <N>  Frames to hold each image (default 720).\n"
    "  --submonitor-fade-frames <N>   Fade in/out length (default 120).\n"
    "  --dump-anim-info <out.json>\n"
    "                          After the startup --ifs loads, switch to each\n"
    "                          animation in the IFS, read its master clip's\n"
    "                          frame count and timeline labels from the\n"
    "                          game's afp engine, write them as JSON and\n"
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

constexpr std::array<StringOpt, 30> kStringOpts = {{
    {.name = "--game-dir", .member = &Options::game_dir},
    {.name = "--profile", .member = &Options::game_profile},
    {.name = "--ifs", .member = &Options::startup_ifs},
    {.name = "--ifs2", .member = &Options::swap_ifs},
    {.name = "--animation", .member = &Options::animation_name},
    {.name = "--animation-label", .member = &Options::animation_label},
    {.name = "--goto-label", .member = &Options::goto_label},
    {.name = "--iidx-playfield", .member = &Options::iidx_playfield},
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

Handled HandleLoadBitmaps(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--load-bitmaps", v, err)) return Handled::Error;
    if (v.empty()) {
        err = "--load-bitmaps expects a package path";
        return Handled::Error;
    }
    out.bitmap_packages.push_back(std::move(v));
    return Handled::Ok;
}

Handled HandleStretch(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--stretch", v, err)) return Handled::Error;
    if (v == "0" || v == "off") {
        out.stretch_16_9 = 0;
        return Handled::Ok;
    }
    if (v == "1" || v == "on") {
        out.stretch_16_9 = 1;
        return Handled::Ok;
    }
    err = "--stretch expects 0/off or 1/on, got '" + v + "'";
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

Handled HandleLoadAlongside(Cursor& c, Options& out, std::string& err) {
    std::string v;
    if (!NextArg(c, "--load-alongside", v, err)) return Handled::Error;
    out.alongside_ifs.push_back(std::move(v));
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

constexpr std::array<SpecialOpt, 18> kSpecialOpts = {{
    {.name = "--render-size", .fn = HandleRenderSize},
    {.name = "--export-size", .fn = HandleExportSize},
    {.name = "--scale", .fn = HandleScale},
    {.name = "--afp-speed", .fn = HandleAfpSpeed},
    {.name = "--root-loop", .fn = HandleRootLoop},
    {.name = "--stretch", .fn = HandleStretch},
    {.name = "--load-bitmaps", .fn = HandleLoadBitmaps},
    {.name = "--mc-name-type", .fn = HandleMcNameType},
    {.name = "--export-format", .fn = HandleExportFormat},
    {.name = "--export-crop", .fn = HandleExportCrop},
    {.name = "--export-bg", .fn = HandleExportBg},
    {.name = "--screenshot-frames", .fn = HandleScreenshotFrames},
    {.name = "--submonitor-frames", .fn = HandleSubmonitorFrames},
    {.name = "--load-alongside", .fn = HandleLoadAlongside},
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
