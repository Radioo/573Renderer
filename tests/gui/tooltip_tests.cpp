#include "gui_test_harness.h"
#include "gui_gpu.h"
#include "gui_mock_assets.h"

#include "gui_icons.h"
#include "video_encoder.h"
#include "warp_device.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "media/media_format.h"
#include "scene3d/scene3d_host.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <utility>

#ifdef Yield
#undef Yield
#endif

namespace {

void ExpectTooltips(ImGuiTestContext* ctx, std::initializer_list<const char*> paths) {
    for (const char* path : paths) {
        bool const shown = GuiTest::HoverShowsTooltip(ctx, path);
        if (!shown) ctx->LogError("no tooltip for '%s'", path);
        IM_CHECK_SILENT(shown);
    }
}

}

TEST_CASE("setup view controls explain themselves on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("tip_setup");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ExpectTooltips(ctx, {"##game_profile", "##render_fps", "##render_preset"});
    };
    harness.Run(test);
}

TEST_CASE("top bar Export explains itself on hover when enabled", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_topbar");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        IM_CHECK(GuiTest::HoverShowsTooltip(ctx, ICON_EXPORT "  Export..."));
    };
    harness.Run(test);
}

TEST_CASE("status strip export tag and reveal button explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip2.ifs", 0, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Done;
    ex.output_path = "C:/out/tip.webm";
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("tip_status_strip");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        ExpectTooltips(ctx, {"export done", "Open folder"});
    };
    harness.Run(test);
}

TEST_CASE("status strip render error explains itself on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    App::Status status = App::Global().GetStatus();
    status.scene_loaded = true;
    status.last_error = "afp_stream_create failed for a very long path that gets shortened";
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("tip_status_error");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        const ImGuiWindow* strip = ctx->GetWindowByRef("");
        IM_CHECK(strip != nullptr);
        ctx->MouseMoveToPos(ImVec2(strip->Pos.x + 340.0F, strip->Pos.y + 12.0F));
        ctx->Yield(4);
        IM_CHECK(GuiTest::TooltipShown(ctx));
    };
    harness.Run(test);
}

TEST_CASE("timeline transport buttons all explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip3.ifs", 40, 300);

    ImGuiTest* test = harness.NewTest("tip_transport");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        ExpectTooltips(ctx,
                       {ICON_JUMP_BACK, ICON_STEP_BACK, ICON_PAUSE, ICON_STEP_FWD, ICON_JUMP_FWD});
    };
    harness.Run(test);
}

TEST_CASE("timeline track and label combo explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip4.ifs", 40, 300);
    App::Status status = App::Global().GetStatus();
    status.labels.push_back({.name = "intro", .frame = 0});
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("tip_track");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        ExpectTooltips(ctx, {"##tl_labels", "##tl_track"});
    };
    harness.Run(test);
}

TEST_CASE("inspector render tab controls all explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip5.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_inspector_render");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Render");
        ctx->Yield(2);
        ctx->ItemCheck("##inspector_tabs/Render/Show MC names (F3)");
        ctx->Yield(2);
        ExpectTooltips(ctx, {
                                "##inspector_tabs/Render/Loop master animation",
                                "##inspector_tabs/Render/##root_loop/$$1/Force loop",
                                "##inspector_tabs/Render/##live_cont/$$2/ON",
                                "##inspector_tabs/Render/##live_trim",
                                "##inspector_tabs/Render/1.5x##scale_sdvx_old",
                                "##inspector_tabs/Render/##bg_color/$$5/blue",
                                "##inspector_tabs/Render/Filter (F7)",
                                "##inspector_tabs/Render/Show MC names (F3)",
                            });
    };
    harness.Run(test);
}

TEST_CASE("qpro controls explain themselves on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");

    ImGuiTest* test = harness.NewTest("tip_qpro");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("qpro");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view");
        ExpectTooltips(ctx, {"Scan parts from bm2dx.dll", "Keep static base fills un-hue-shifted",
                             "Output fps"});
    };
    harness.Run(test);
}

TEST_CASE("export modal controls all explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip6.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_export");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick(ICON_EXPORT "  Export...");
        ctx->SetRef("Export");
        GuiTest::ComboPick(ctx, "##exp_fmt", "WebM AV1 (video, opaque, NVENC)");
        ImGuiTestItemInfo const adv = ctx->ItemInfo("Advanced");
        if ((adv.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) ctx->ItemClick("Advanced");
        ctx->ItemCheck("Limit frames##exp_limit");
        ctx->ItemCheck("Blend loop seam##exp_blend");
        ctx->Yield(3);
        ExpectTooltips(ctx, {"##exp_stem", "##exp_fmt", "fps##exp_fps", "##exp_q", "##exp_sz",
                             "Transparent bg", "HW accel##exp_hw", "keyframe interval##exp_keyint",
                             "Limit frames##exp_limit", "frames##exp_maxf",
                             "Continuous loop count##exp_loops", "Blend loop seam##exp_blend",
                             "Blend frames##exp_blendN", "Pick region##crop_pick"});
        ctx->ItemClick("Close");
    };
    harness.Run(test);
}

TEST_CASE("scene pane rows explain themselves on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip7.ifs", 0, 300);
    {
        auto& cfg = App::Global().MutConfig("bg_tip7.ifs");
        cfg.filename = "bg_tip7.ifs";
        cfg.anim_names = {"bg_main"};
    }

    ImGuiTest* test = harness.NewTest("tip_scene_pane");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemInputValue("##scene_filter", "");
        IM_CHECK(GuiTest::HoverShowsTooltip(ctx, "Add"));
        GuiTest::FocusChild(ctx, "scene_scroll");
        IM_CHECK(GuiTest::HoverShowsTooltip(ctx, "$$0/##layer"));
    };
    harness.Run(test);
}

TEST_CASE("hardware-accel tooltip names the reason for the chosen format", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_hwtip.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_hw_branches");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        struct Case {
            const char* label;
            MediaSink::Format format;
        };
        const Case cases[] = {
            {.label = "AVIF (image, alpha)", .format = MediaSink::Format::AVIF},
            {.label = "WebM VP9 (video, alpha, software)", .format = MediaSink::Format::WebM_VP9},
            {.label = "WebM AV1 (video, opaque, NVENC)", .format = MediaSink::Format::WebM_AV1},
            {.label = "WebP (image, alpha, software) - recommended",
             .format = MediaSink::Format::WebP_Anim},
            {.label = "PNG sequence (folder of frames, lossless)",
             .format = MediaSink::Format::PNG_Sequence},
            {.label = "MP4 H.264 (video, opaque, most compatible)",
             .format = MediaSink::Format::MP4_H264},
            {.label = "MP4 HEVC alpha (video, alpha, Safari)",
             .format = MediaSink::Format::MP4_HEVC_Alpha},
        };

        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick(ICON_EXPORT "  Export...");
        ctx->SetRef("Export");

        for (const Case& c : cases) {
            const bool hw =
                VideoEncoder::HardwareAvailable(MediaSink::HardwareProbeFormat(c.format));
            const bool is_h264 = (c.format == MediaSink::Format::MP4_H264);
            const char* expect = nullptr;
            if (c.format == MediaSink::Format::MP4_HEVC_Alpha) {
                expect = "MP4 HEVC-alpha is software only";
            } else if (!hw && is_h264) {
                expect = "h264_nvenc unavailable";
            } else if (!hw) {
                expect = "Hardware acceleration unavailable";
            } else if (c.format == MediaSink::Format::WebM_VP9) {
                expect = "WebM VP9 has no hardware encoder";
            } else if (c.format == MediaSink::Format::WebP_Anim) {
                expect = "WebP (libwebp_anim) is software only";
            } else if (c.format == MediaSink::Format::PNG_Sequence) {
                expect = "PNG sequence writes lossless";
            } else if (is_h264) {
                expect = "Encode H.264 with NVENC";
            } else {
                expect = "Encode the AV1 stream with NVENC";
            }

            GuiTest::ComboPick(ctx, "##exp_fmt", c.label);
            std::string const text = GuiTest::HoverAndCaptureText(ctx, "HW accel##exp_hw");
            if (text.find(expect) == std::string::npos) {
                ctx->LogError("format '%s' expected '%s', captured: %s", c.label, expect,
                              text.c_str());
            }
            IM_CHECK_SILENT(text.find(expect) != std::string::npos);
        }
        ctx->ItemClick("Close");
    };
    harness.Run(test);
}

TEST_CASE("animate-camera tooltip explains why it is greyed out", "[gui][tooltip][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("tip_camera_branches");
    GuiTest::WriteMock3dScene(assets.path(), false);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");
    REQUIRE(Scene3dHost::Load(assets.path()));
    REQUIRE_FALSE(Scene3dHost::GetStatus().has_authored_camera);

    ImGuiTest* no_cam = harness.NewTest("tip_cam_none");
    no_cam->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/3D scene");
        ctx->Yield(2);
        std::string const text =
            GuiTest::HoverAndCaptureText(ctx, "##inspector_tabs/3D scene/Animate camera##s3d");
        if (text.find("no camera in its .x file") == std::string::npos) {
            ctx->LogError("captured: %s", text.c_str());
        }
        IM_CHECK_SILENT(text.find("no camera in its .x file") != std::string::npos);
    };
    harness.Run(no_cam);
    Scene3dHost::Unload();

    GuiTest::TempAssetDir const with_cam("tip_camera_branches2");
    GuiTest::WriteMock3dScene(with_cam.path(), true);
    REQUIRE(Scene3dHost::Load(with_cam.path()));

    ImGuiTest* enabled = harness.NewTest("tip_cam_enabled");
    enabled->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/3D scene");
        ctx->Yield(2);
        ctx->ItemUncheck("##inspector_tabs/3D scene/Free camera##s3d");
        ctx->Yield(2);
        std::string const text =
            GuiTest::HoverAndCaptureText(ctx, "##inspector_tabs/3D scene/Animate camera##s3d");
        IM_CHECK_SILENT(text.find("Freeze the camera animated inside") != std::string::npos);
    };
    harness.Run(enabled);

    ImGuiTest* freecam = harness.NewTest("tip_cam_freecam");
    freecam->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemCheck("##inspector_tabs/3D scene/Free camera##s3d");
        ctx->Yield(2);
        std::string const text =
            GuiTest::HoverAndCaptureText(ctx, "##inspector_tabs/3D scene/Animate camera##s3d");
        IM_CHECK_SILENT(text.find("Free camera is on") != std::string::npos);
    };
    harness.Run(freecam);

    Scene3dHost::Unload();
}

TEST_CASE("export status tag tooltip carries the output path or the error", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tagtip.ifs", 0, 300);
    App::ExportState done;
    done.phase = App::ExportPhase::Done;
    done.output_path = "C:/out/unique_done_name.webm";
    App::Global().SetExport(done);

    ImGuiTest* ok = harness.NewTest("tip_tag_done");
    ok->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        std::string const text = GuiTest::HoverAndCaptureText(ctx, "export done");
        IM_CHECK_SILENT(text.find("unique_done_name.webm") != std::string::npos);
    };
    harness.Run(ok);

    App::ExportState failed;
    failed.phase = App::ExportPhase::Failed;
    failed.error = "encoder_rejected_the_stream";
    App::Global().SetExport(failed);

    ImGuiTest* bad = harness.NewTest("tip_tag_failed");
    bad->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        std::string const text = GuiTest::HoverAndCaptureText(ctx, "export failed");
        IM_CHECK_SILENT(text.find("encoder_rejected_the_stream") != std::string::npos);
    };
    harness.Run(bad);
}

TEST_CASE("export status tag invites a click while a capture is running", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tagbusy.ifs", 0, 300);
    App::ExportState busy;
    busy.phase = App::ExportPhase::Capturing;
    busy.frames_captured = 30;
    App::Global().SetExport(busy);

    ImGuiTest* test = harness.NewTest("tip_tag_capturing");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        std::string const text = GuiTest::HoverAndCaptureText(ctx, "capturing 30");
        IM_CHECK_SILENT(text.find("Click to open the export dialog") != std::string::npos);
    };
    harness.Run(test);

    App::Global().SetExport({});
}

TEST_CASE("timeline label tick names the label under the pointer", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_ticktip.ifs", 0, 300);
    App::Status status = App::Global().GetStatus();
    status.labels.clear();
    status.labels.push_back({.name = "breakdown_marker", .frame = 150});
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("tip_label_tick");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        std::string const text = GuiTest::HoverAndCaptureText(ctx, "##tl_track");
        IM_CHECK_SILENT(text.find("breakdown_marker") != std::string::npos);
        IM_CHECK_SILENT(text.find("click to play from here") != std::string::npos);
    };
    harness.Run(test);

    status.labels.clear();
    App::Global().SetStatus(status);
}

TEST_CASE("slot bitmap combo explains the default restore", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_slottip.ifs", 0, 300);
    {
        auto& cfg = App::Global().MutConfig("bg_slottip.ifs");
        cfg.filename = "bg_slottip.ifs";
        cfg.anim_names = {"bg_main"};
        cfg.bitmap_names = {"tex_a", "tex_b"};
        App::VariantSlot slot;
        slot.path = "slottip_coin";
        slot.default_bitmap = "tex_a";
        cfg.slots.push_back(std::move(slot));
    }
    App::Status tree = App::Global().GetStatus();
    tree.playing_animation = "bg_main";
    tree.mc_tree = {};
    tree.mc_tree.name = "root";
    tree.mc_tree.path = "root";
    tree.mc_tree.enumerated = true;
    App::SubLayerNode leaf;
    leaf.name = "gear";
    leaf.path = "gear";
    leaf.enumerated = true;
    tree.mc_tree.children.push_back(leaf);
    App::Global().SetStatus(tree);

    ImGuiTest* test = harness.NewTest("tip_slot_bitmap");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemInputValue("##scene_filter", "");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center/scene_scroll");
        ctx->ItemClick("$$0/##layer/slottip_coin/   slottip_coin");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Properties");
        ctx->Yield(2);
        std::string const text =
            GuiTest::HoverAndCaptureText(ctx, "##inspector_tabs/Properties/##bitmap");
        IM_CHECK_SILENT(text.find("Swap the clip's bitmap") != std::string::npos);
    };
    harness.Run(test);

    tree.playing_animation.clear();
    tree.mc_tree = {};
    App::Global().SetStatus(tree);
}

TEST_CASE("export scale buttons say what they divide", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_scaletip.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_scale_buttons");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick(ICON_EXPORT "  Export...");
        ctx->SetRef("Export");
        ctx->ItemInputValue("##exp_w", 1920);
        ctx->ItemInputValue("##exp_h", 1080);
        ctx->Yield(2);
        std::string const half =
            GuiTest::HoverAndCaptureText(ctx, "x0.5 (960x540)##exp_scl_x0.5##exp_scl");
        IM_CHECK_SILENT(half.find("Halve the current size") != std::string::npos);
        std::string const quarter =
            GuiTest::HoverAndCaptureText(ctx, "x0.25 (480x270)##exp_scl_x0.25##exp_scl");
        IM_CHECK_SILENT(quarter.find("Quarter the current size") != std::string::npos);
        std::string const tenth =
            GuiTest::HoverAndCaptureText(ctx, "x0.1 (192x108)##exp_scl_x0.1##exp_scl");
        IM_CHECK_SILENT(tenth.find("size") != std::string::npos);
        ctx->ItemClick("Close");
    };
    harness.Run(test);
}

TEST_CASE("export modal reveal button explains where it takes you", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_revealtip.ifs", 0, 300);
    App::ExportState done;
    done.phase = App::ExportPhase::Done;
    done.output_path = "C:/out/reveal_tip.avif";
    App::Global().SetExport(done);

    ImGuiTest* test = harness.NewTest("tip_modal_reveal");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick(ICON_EXPORT "  Export...");
        ctx->SetRef("Export");
        std::string const text = GuiTest::HoverAndCaptureText(ctx, "Open folder");
        IM_CHECK_SILENT(text.find("Show the exported file in Explorer") != std::string::npos);
        ctx->ItemClick("Close");
    };
    harness.Run(test);

    App::Global().SetExport({});
}

TEST_CASE("qpro animated-parts note lists the three browser outputs", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");

    ImGuiTest* test = harness.NewTest("tip_qpro_animated");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("qpro");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view");
        ImGuiTestItemInfo const fps = ctx->ItemInfo("Output fps");
        IM_CHECK_NE(fps.ID, 0U);
        ImGuiStyle const& style = ctx->UiContext->Style;
        float const y =
            fps.RectFull.Max.y + (style.ItemSpacing.y * 2.0F) + (ctx->UiContext->FontSize * 0.5F);
        ctx->MouseMove("Output fps");
        ctx->MouseMoveToPos(ImVec2(fps.RectFull.Min.x + 40.0F, y));
        ctx->Yield(3);
        std::string const text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK_SILENT(text.find("transparent WebM") != std::string::npos);
    };
    harness.Run(test);
}
