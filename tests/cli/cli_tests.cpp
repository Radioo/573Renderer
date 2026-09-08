#include <catch2/catch_test_macros.hpp>

#include "cli/cli.h"
#include "cli/tool_command.h"
#include "media/media_format.h"
#include "state/commands.h"

#include <string>
#include <vector>

namespace {

struct ParseResult {
    bool ok = false;
    Cli::Options opts;
    std::string err;
};

ParseResult Run(const std::vector<std::string>& args) {
    std::vector<std::string> storage;
    storage.emplace_back("573Renderer.exe");
    storage.insert(storage.end(), args.begin(), args.end());
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (std::string& s : storage)
        argv.push_back(s.data());
    ParseResult r;
    r.ok = Cli::Parse(static_cast<int>(argv.size()), argv.data(), r.opts, r.err);
    return r;
}

}

TEST_CASE("Parse accepts empty argv") {
    const ParseResult r = Run({});
    CHECK(r.ok);
    CHECK(r.opts.game_dir.empty());
    CHECK(r.opts.export_fps == 60);
}

TEST_CASE("Parse handles flags and string options") {
    const ParseResult r =
        Run({"--headless", "--no-gui", "--boot-ifses", "--start-paused", "--filter",
             "--show-mc-names", "--game-dir", "D:/games/sdvx", "--profile", "sdvx7", "--ifs",
             "a.ifs", "--animation", "bg", "--goto-label", "loop"});
    REQUIRE(r.ok);
    CHECK(r.opts.headless);
    CHECK(r.opts.no_gui);
    CHECK(r.opts.boot_ifses);
    CHECK(r.opts.start_paused);
    CHECK(r.opts.filter_enabled);
    CHECK(r.opts.show_mc_names);
    CHECK(r.opts.game_dir == "D:/games/sdvx");
    CHECK(r.opts.game_profile == "sdvx7");
    CHECK(r.opts.startup_ifs == "a.ifs");
    CHECK(r.opts.animation_name == "bg");
    CHECK(r.opts.goto_label == "loop");
}

TEST_CASE("Parse handles dump-anim-info") {
    const ParseResult r = Run({"--dump-anim-info", "out.json"});
    REQUIRE(r.ok);
    CHECK(r.opts.dump_anim_info == "out.json");
    CHECK(Run({}).opts.dump_anim_info.empty());
}

TEST_CASE("Parse rejects unknown arguments") {
    const ParseResult r = Run({"--frobnicate"});
    CHECK_FALSE(r.ok);
    CHECK(r.err == "Unknown argument: --frobnicate");
}

TEST_CASE("Parse rejects a missing value") {
    const ParseResult r = Run({"--game-dir"});
    CHECK_FALSE(r.ok);
    CHECK(r.err == "Missing value for --game-dir");
}

TEST_CASE("render-size validates format and range") {
    CHECK(Run({"--render-size", "1080x1920"}).opts.render_height == 1920);
    CHECK_FALSE(Run({"--render-size", "1080"}).ok);
    CHECK_FALSE(Run({"--render-size", "ax b"}).ok);
    CHECK_FALSE(Run({"--render-size", "32x32"}).ok);
    CHECK_FALSE(Run({"--render-size", "9000x9000"}).ok);
}

TEST_CASE("fps and qpro-fps enforce ranges") {
    CHECK(Run({"--fps", "120"}).opts.render_fps == 120);
    CHECK_FALSE(Run({"--fps", "0"}).ok);
    CHECK_FALSE(Run({"--fps", "1001"}).ok);
    CHECK(Run({"--qpro-fps", "60"}).opts.qpro_fps == 60);
    CHECK_FALSE(Run({"--qpro-fps", "241"}).ok);
}

TEST_CASE("scale and afp-speed validate as floats") {
    CHECK(Run({"--scale", "1.5"}).opts.master_scale == 1.5F);
    CHECK_FALSE(Run({"--scale", "0"}).ok);
    CHECK_FALSE(Run({"--scale", "banana"}).ok);
    CHECK(Run({"--afp-speed", "0.5"}).opts.afp_speed == 0.5F);
    CHECK_FALSE(Run({"--afp-speed", "17"}).ok);
}

TEST_CASE("root-loop accepts hold and force only") {
    CHECK(Run({"--root-loop", "hold"}).opts.root_loop_mode == 0);
    CHECK(Run({"--root-loop", "force"}).opts.root_loop_mode == 1);
    CHECK_FALSE(Run({"--root-loop", "maybe"}).ok);
    CHECK(Run({}).opts.root_loop_mode == -1);
}

TEST_CASE("clamped int options keep their floors") {
    CHECK(Run({"--export-max-frames", "-5"}).opts.export_max_frames == 0);
    CHECK(Run({"--export-loop-count", "0"}).opts.export_loop_count == 1);
    CHECK(Run({"--blend-frames", "-1"}).opts.export_blend_frames == 0);
    CHECK(Run({"--seek-frame", "-3"}).opts.seek_frame == -1);
    CHECK(Run({"--mc-name-type", "7"}).opts.mc_name_type == 1);
}

TEST_CASE("screenshot-frames parses permissively") {
    const ParseResult r = Run({"--screenshot-frames", "1,30,,0,120"});
    REQUIRE(r.ok);
    CHECK(r.opts.screenshot_frames == std::vector<int>{1, 30, 120});
}

TEST_CASE("export-format resolves tokens and aliases") {
    CHECK(Run({"--export-format", "avif"}).opts.export_format == 0);
    CHECK(Run({"--export-format", "webm"}).opts.export_format == 1);
    CHECK(Run({"--export-format", "mp4"}).opts.export_format == 5);
    CHECK_FALSE(Run({"--export-format", "gif"}).ok);
}

TEST_CASE("export-bg parses transparent and rgb") {
    CHECK(Run({"--export-bg", "transparent"}).opts.export_bg_transparent);
    const ParseResult r = Run({"--export-bg", "30,33,43"});
    REQUIRE(r.ok);
    CHECK_FALSE(r.opts.export_bg_transparent);
    CHECK(r.opts.export_bg_r == 30.0F / 255.0F);
    CHECK(r.opts.export_bg_b == 43.0F / 255.0F);
    CHECK_FALSE(Run({"--export-bg", "1,2"}).ok);
}

TEST_CASE("export-crop needs four non-negative ints") {
    const ParseResult r = Run({"--export-crop", "10,20,300,400"});
    REQUIRE(r.ok);
    CHECK(r.opts.export_crop_x == 10);
    CHECK(r.opts.export_crop_h == 400);
    CHECK_FALSE(Run({"--export-crop", "1,2,3"}).ok);
    CHECK_FALSE(Run({"--export-crop", "1,2,3,x"}).ok);
}

TEST_CASE("variant and hide build slot overrides") {
    const ParseResult r = Run({"--variant", "coin=start", "--hide", "logo"});
    REQUIRE(r.ok);
    REQUIRE(r.opts.slot_overrides.size() == 2);
    CHECK(r.opts.slot_overrides[0].path == "coin");
    CHECK(r.opts.slot_overrides[0].bitmap == "start");
    CHECK(r.opts.slot_overrides[0].visible);
    CHECK(r.opts.slot_overrides[1].path == "logo");
    CHECK_FALSE(r.opts.slot_overrides[1].visible);
    CHECK_FALSE(Run({"--variant", "nobitmap"}).ok);
}

TEST_CASE("sublayer overrides record visibility") {
    const ParseResult r = Run({"--hide-sublayer", "content_usr", "--show-sublayer", "frame"});
    REQUIRE(r.ok);
    REQUIRE(r.opts.sublayer_overrides.size() == 2);
    CHECK_FALSE(r.opts.sublayer_overrides[0].visible);
    CHECK(r.opts.sublayer_overrides[1].visible);
}

TEST_CASE("load-alongside collects paths in the order given") {
    const ParseResult r = Run({"--ifs", "title.ifs", "--load-alongside", "title_j.ifs",
                               "--load-alongside", "graphic/extra.ifs"});
    REQUIRE(r.ok);
    CHECK(r.opts.startup_ifs == "title.ifs");
    CHECK(r.opts.alongside_ifs == std::vector<std::string>{"title_j.ifs", "graphic/extra.ifs"});
}

TEST_CASE("load-alongside needs a path") {
    const ParseResult r = Run({"--load-alongside"});
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.err.empty());
}

TEST_CASE("submonitor options parse") {
    const ParseResult r =
        Run({"--submonitor-frames", "a.png,b.png,", "--submonitor-clip", "subbg_usr",
             "--submonitor-slideshow-fade", "--submonitor-dwell-frames", "600"});
    REQUIRE(r.ok);
    CHECK(r.opts.submonitor_frames == std::vector<std::string>{"a.png", "b.png"});
    CHECK(r.opts.submonitor_clip == "subbg_usr");
    CHECK(r.opts.submonitor_slideshow_fade);
    CHECK(r.opts.submonitor_dwell_frames == 600);
}

TEST_CASE("help returns false without error") {
    const ParseResult r = Run({"--help"});
    CHECK_FALSE(r.ok);
    CHECK(r.err.empty());
}

TEST_CASE("ParseToolCommand recognizes ddr-test with defaults and explicit args") {
    const std::vector<std::string> full = {"exe",    "--ddr-test", "C:/mods",
                                           "bg.arc", "out.png",    "12"};
    Cli::ToolCommand c = Cli::ParseToolCommand(full);
    CHECK(c.kind == Cli::ToolKind::DdrTest);
    CHECK(c.in_path == "C:/mods");
    CHECK(c.arc_path == "bg.arc");
    CHECK(c.out_path == "out.png");
    CHECK(c.frames == 12);

    const std::vector<std::string> minimal = {"exe", "--ddr-test", "C:/mods"};
    c = Cli::ParseToolCommand(minimal);
    CHECK(c.kind == Cli::ToolKind::DdrTest);
    CHECK(c.arc_path.empty());
    CHECK(c.out_path == "ddr_out.png");
    CHECK(c.frames == 120);
}

TEST_CASE("ParseToolCommand handles the single-path tools") {
    const std::vector<std::string> arc = {"exe", "--extract-arc", "D:/data"};
    CHECK(Cli::ParseToolCommand(arc).kind == Cli::ToolKind::ExtractArc);
    CHECK(Cli::ParseToolCommand(arc).in_path == "D:/data");

    const std::vector<std::string> cust = {"exe", "--extract-customize", "D:/data"};
    CHECK(Cli::ParseToolCommand(cust).kind == Cli::ToolKind::ExtractCustomize);

    const std::vector<std::string> scan = {"exe", "--qpro-scan", "bm2dx.dll"};
    CHECK(Cli::ParseToolCommand(scan).kind == Cli::ToolKind::QproScan);
    CHECK(Cli::ParseToolCommand(scan).in_path == "bm2dx.dll");
}

TEST_CASE("ParseToolCommand qpro-json output defaults unless a non-flag follows") {
    const std::vector<std::string> with_out = {"exe", "--extract-qpro-json", "bm2dx.dll",
                                               "parts.json"};
    Cli::ToolCommand c = Cli::ParseToolCommand(with_out);
    CHECK(c.kind == Cli::ToolKind::ExtractQproJson);
    CHECK(c.out_path == "parts.json");

    const std::vector<std::string> flag_follows = {"exe", "--extract-qpro-json", "bm2dx.dll",
                                                   "--no-gui"};
    c = Cli::ParseToolCommand(flag_follows);
    CHECK(c.out_path == "2dx_qpro.json");
}

TEST_CASE("ParseToolCommand keeps the historical priority order and value rule") {
    const std::vector<std::string> both = {"exe", "--extract-arc", "D:/data", "--ddr-test",
                                           "C:/mods"};
    CHECK(Cli::ParseToolCommand(both).kind == Cli::ToolKind::DdrTest);

    const std::vector<std::string> trailing = {"exe", "--extract-arc"};
    CHECK(Cli::ParseToolCommand(trailing).kind == Cli::ToolKind::None);

    const std::vector<std::string> none = {"exe", "--no-gui"};
    CHECK(Cli::ParseToolCommand(none).kind == Cli::ToolKind::None);
}

TEST_CASE("ParseToolCommand parses the headless preset document tools") {
    const std::vector<std::string> dump = {"exe", "--preset-dump-defaults", "out/presets"};
    Cli::ToolCommand c = Cli::ParseToolCommand(dump);
    CHECK(c.kind == Cli::ToolKind::PresetDumpDefaults);
    CHECK(c.out_path == "out/presets");

    const std::vector<std::string> one = {"exe", "--preset-export-json", "iidx11", "iidx11-attract",
                                          "attract.json"};
    c = Cli::ParseToolCommand(one);
    CHECK(c.kind == Cli::ToolKind::PresetExportJson);
    CHECK(c.build == "iidx11");
    CHECK(c.preset_id == "iidx11-attract");
    CHECK(c.out_path == "attract.json");

    const std::vector<std::string> validate = {"exe", "--preset-validate", "mine.json"};
    c = Cli::ParseToolCommand(validate);
    CHECK(c.kind == Cli::ToolKind::PresetValidate);
    CHECK(c.in_path == "mine.json");
}

TEST_CASE("ParseToolCommand never takes a flag as a positional of a document tool") {
    const std::vector<std::string> partial = {"exe", "--preset-export-json", "iidx11",
                                              "iidx11-attract", "--force"};
    Cli::ToolCommand c = Cli::ParseToolCommand(partial);
    CHECK(c.kind == Cli::ToolKind::PresetExportJson);
    CHECK(c.build == "iidx11");
    CHECK(c.preset_id == "iidx11-attract");
    CHECK(c.out_path == "iidx11-attract.json");

    const std::vector<std::string> only_build = {"exe", "--preset-export-json", "iidx11",
                                                 "--force"};
    c = Cli::ParseToolCommand(only_build);
    CHECK(c.build == "iidx11");
    CHECK(c.preset_id.empty());
    CHECK(c.out_path.empty());

    const std::vector<std::string> dump = {"exe", "--preset-dump-defaults", "--force"};
    c = Cli::ParseToolCommand(dump);
    CHECK(c.kind == Cli::ToolKind::PresetDumpDefaults);
    CHECK(c.out_path.empty());
}

TEST_CASE("ParseToolCommand takes a document file in place of a preset id") {
    const std::vector<std::string> test = {"exe", "--preset-test", "D:/iidx", "--preset-json",
                                           "mine.json"};
    Cli::ToolCommand c = Cli::ParseToolCommand(test);
    CHECK(c.kind == Cli::ToolKind::PresetTest);
    CHECK(c.in_path == "D:/iidx");
    CHECK(c.preset_id.empty());
    CHECK(c.json_path == "mine.json");
    CHECK_FALSE(c.force);

    const std::vector<std::string> forced = {"exe",           "--preset-export", "D:/iidx",
                                             "--preset-json", "mine.json",       "--force"};
    c = Cli::ParseToolCommand(forced);
    CHECK(c.kind == Cli::ToolKind::PresetExport);
    CHECK(c.json_path == "mine.json");
    CHECK(c.force);

    const std::vector<std::string> with_output = {"exe", "--preset-test", "D:/iidx",  "out.png",
                                                  "120", "--preset-json", "mine.json"};
    c = Cli::ParseToolCommand(with_output);
    CHECK(c.preset_id.empty());
    CHECK(c.out_path == "out.png");
    CHECK(c.frames == 120);
    CHECK(c.json_path == "mine.json");
}

TEST_CASE("ParseToolCommand collects repeated preset options") {
    const std::vector<std::string> args = {
        "exe", "--preset-test",   "D:/iidx",     "iidx11-mode-select", "out.png",
        "120", "--preset-option", "mode=EXPERT", "--preset-option",    "attack=1"};
    const Cli::ToolCommand c = Cli::ParseToolCommand(args);
    CHECK(c.kind == Cli::ToolKind::PresetTest);
    CHECK(c.preset_id == "iidx11-mode-select");
    CHECK(c.out_path == "out.png");
    CHECK(c.frames == 120);
    REQUIRE(c.options.size() == 2);
    CHECK(c.options[0] == "mode=EXPERT");
    CHECK(c.options[1] == "attack=1");
}

TEST_CASE("ParseToolCommand rejects the retired tweak file flag") {
    const std::vector<std::string> args = {
        "exe",     "--preset-test",   "D:/iidx",   "iidx11-attract",
        "out.png", "--preset-tweaks", "tweaks.txt"};
    const Cli::ToolCommand c = Cli::ParseToolCommand(args);
    CHECK(c.kind == Cli::ToolKind::RetiredFlag);
    CHECK(c.retired == "--preset-tweaks");
}

TEST_CASE("Parse handles every string-valued qpro flag") {
    const ParseResult r =
        Run({"--extract-qpro",        "out/qpro", "--qpro-parts",          "head,hand",
             "--qpro-only",           "body",     "--qpro-dump",           "qpro_body.ifs",
             "--qpro-body-one",       "b1",       "--qpro-back-one",       "k1",
             "--qpro-head-one",       "h1",       "--qpro-hand-one",       "n1",
             "--qpro-hair-one",       "r1",       "--qpro-face-one",       "f1",
             "--qpro-clip-one",       "c1",       "--qpro-hand-composite", "hc",
             "--qpro-head-composite", "dc",       "--qpro-back-composite", "bc"});
    REQUIRE(r.ok);
    CHECK(r.opts.extract_qpro_dir == "out/qpro");
    CHECK(r.opts.qpro_parts == "head,hand");
    CHECK(r.opts.qpro_only == "body");
    CHECK(r.opts.qpro_dump_ifs == "qpro_body.ifs");
    CHECK(r.opts.qpro_body_one == "b1");
    CHECK(r.opts.qpro_back_one == "k1");
    CHECK(r.opts.qpro_head_one == "h1");
    CHECK(r.opts.qpro_hand_one == "n1");
    CHECK(r.opts.qpro_hair_one == "r1");
    CHECK(r.opts.qpro_face_one == "f1");
    CHECK(r.opts.qpro_clip_one == "c1");
    CHECK(r.opts.qpro_hand_composite == "hc");
    CHECK(r.opts.qpro_head_composite == "dc");
    CHECK(r.opts.qpro_back_composite == "bc");
}

TEST_CASE("Parse handles the qpro hue-scope switch") {
    const ParseResult r = Run({"--qpro-no-hue-scope"});
    REQUIRE(r.ok);
    CHECK(r.opts.qpro_no_hue_scope);
    CHECK_FALSE(Run({}).opts.qpro_no_hue_scope);
}

TEST_CASE("Parse handles the full export option set") {
    const ParseResult r =
        Run({"--export", "out/clip.webm", "--export-fps", "48", "--export-quality", "82",
             "--export-keyframe-interval", "250", "--export-size", "960x540",
             "--export-dump-frames", "out/frames", "--blend-loop", "--export-no-hw"});
    REQUIRE(r.ok);
    CHECK(r.opts.export_path == "out/clip.webm");
    CHECK(r.opts.export_fps == 48);
    CHECK(r.opts.export_quality == 82);
    CHECK(r.opts.export_keyframe_interval == 250);
    CHECK(r.opts.export_width == 960);
    CHECK(r.opts.export_height == 540);
    CHECK(r.opts.export_dump_frames_dir == "out/frames");
    CHECK(r.opts.export_blend_loop);
    CHECK_FALSE(r.opts.export_prefer_hardware);
}

TEST_CASE("Parse accepts the export software alias") {
    const ParseResult r = Run({"--export-sw"});
    REQUIRE(r.ok);
    CHECK_FALSE(r.opts.export_prefer_hardware);
    CHECK(Run({}).opts.export_prefer_hardware);
}

TEST_CASE("Parse handles the submonitor option set") {
    const ParseResult r =
        Run({"--submonitor-slideshow", "--submonitor-swap-layers", "--submonitor-loop-frames",
             "300", "--submonitor-fade-frames", "45", "--submonitor-fade-in-label", "in_lbl",
             "--submonitor-fade-out-label", "out_lbl"});
    REQUIRE(r.ok);
    CHECK(r.opts.submonitor_slideshow);
    CHECK(r.opts.submonitor_swap_layers);
    CHECK(r.opts.submonitor_loop_frames == 300);
    CHECK(r.opts.submonitor_fade_frames == 45);
    CHECK(r.opts.submonitor_fade_in_label == "in_lbl");
    CHECK(r.opts.submonitor_fade_out_label == "out_lbl");
}

TEST_CASE("Parse handles the remaining playback and diagnostic flags") {
    const ParseResult r =
        Run({"--animation-label", "chorus", "--continuous-loop", "1", "--deferred-replay",
             "--cmd-trace", "trace.txt", "--exit-after-frames", "900", "--swap-after-frames", "120",
             "--ifs2", "second.ifs", "--screenshot-prefix", "shots/f"});
    REQUIRE(r.ok);
    CHECK(r.opts.animation_label == "chorus");
    CHECK(r.opts.continuous_loop_mode == 1);
    CHECK(r.opts.deferred_replay);
    CHECK(r.opts.cmd_trace_path == "trace.txt");
    CHECK(r.opts.exit_after_frames == 900);
    CHECK(r.opts.swap_after_frames == 120);
    CHECK(r.opts.swap_ifs == "second.ifs");
    CHECK(r.opts.screenshot_prefix == "shots/f");
}

TEST_CASE("Parse defaults match the documented option table") {
    const ParseResult r = Run({});
    REQUIRE(r.ok);
    CHECK(r.opts.export_quality == 60);
    CHECK(r.opts.export_loop_count == 1);
    CHECK(r.opts.export_blend_frames == 15);
    CHECK(r.opts.export_bg_transparent);
    CHECK(r.opts.submonitor_dwell_frames == 720);
    CHECK(r.opts.submonitor_fade_frames == 120);
    CHECK(r.opts.submonitor_clip == "subbg_usr/bg_usr");
    CHECK(r.opts.submonitor_fade_in_label == "fade_in");
    CHECK(r.opts.submonitor_fade_out_label == "fade_out");
    CHECK(r.opts.screenshot_prefix == "screenshots/auto_f");
    CHECK(r.opts.root_loop_mode == -1);
    CHECK(r.opts.seek_frame == -1);
}

TEST_CASE("every export default has exactly one definition, shared by the UI and the CLI") {
    const App::ExportRequest canonical;
    const Cli::ToolCommand tool;

    REQUIRE(tool.bg_transparent == canonical.bg_transparent);
    REQUIRE(tool.bg_rgb[0] == canonical.bg_r);
    REQUIRE(tool.bg_rgb[1] == canonical.bg_g);
    REQUIRE(tool.bg_rgb[2] == canonical.bg_b);

    REQUIRE(canonical.fps == 60);
    REQUIRE(canonical.format == MediaSink::ToIndex(MediaSink::kDefaultFormat));
    REQUIRE(canonical.prefer_hardware);
    REQUIRE(canonical.loop_count == 1);
    REQUIRE(canonical.max_frames == 0);
    REQUIRE(canonical.width == 0);
    REQUIRE(canonical.height == 0);
}
