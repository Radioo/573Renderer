#include <catch2/catch_test_macros.hpp>

#include "state/app_state.h"
#include "state/boot_lifecycle.h"
#include "state/commands.h"
#include "state/ifs_catalog.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

std::string TakenLoadPath(App::State& s) {
    auto cmd = s.TakeCommand();
    if (!cmd) return {};
    const auto* load = std::get_if<App::Cmd::LoadContent>(&*cmd);
    return load != nullptr ? load->path : std::string{};
}

}

TEST_CASE("TakeCommand returns nothing when no command is pending") {
    App::State s;
    CHECK_FALSE(s.TakeCommand().has_value());
}

TEST_CASE("PostCommand then TakeCommand hands over the command once") {
    App::State s;
    s.PostCommand(App::Cmd::LoadContent{.path = "a.ifs", .from_arc = true});

    auto cmd = s.TakeCommand();
    REQUIRE(cmd.has_value());
    const auto* load = std::get_if<App::Cmd::LoadContent>(&*cmd);
    REQUIRE(load != nullptr);
    CHECK(load->path == "a.ifs");
    CHECK(load->from_arc);
    CHECK_FALSE(s.TakeCommand().has_value());
}

TEST_CASE("PostCommand queues commands in FIFO order without dropping any") {
    App::State s;
    s.PostCommand(App::Cmd::LoadContent{.path = "first.ifs"});
    s.PostCommand(App::Cmd::LoadContent{.path = "second.ifs"});

    CHECK(TakenLoadPath(s) == "first.ifs");
    CHECK(TakenLoadPath(s) == "second.ifs");
    CHECK_FALSE(s.TakeCommand().has_value());
}

TEST_CASE("A burst of posts drains one per take in order") {
    App::State s;
    for (int i = 0; i < 8; i++) {
        s.PostCommand(App::Cmd::LoadContent{.path = std::to_string(i)});
    }
    for (int i = 0; i < 8; i++) {
        CHECK(TakenLoadPath(s) == std::to_string(i));
    }
    CHECK_FALSE(s.TakeCommand().has_value());
}

TEST_CASE("Command variants round-trip their payloads through the queue") {
    App::State s;
    App::Cmd::BootGame boot_cmd;
    boot_cmd.game_dir = "F:/game";
    boot_cmd.profile_slug = "sdvx7";
    s.PostCommand(std::move(boot_cmd));
    App::ExportRequest exp_req;
    exp_req.output_path = "out.webm";
    exp_req.fps = 60;
    s.PostCommand(App::Cmd::StartExport{.req = std::move(exp_req)});
    s.PostCommand(App::Cmd::CancelExport{});

    auto boot = s.TakeCommand();
    REQUIRE(boot.has_value());
    const auto* b = std::get_if<App::Cmd::BootGame>(&*boot);
    REQUIRE(b != nullptr);
    CHECK(b->game_dir == "F:/game");
    CHECK(b->profile_slug == "sdvx7");

    auto exp = s.TakeCommand();
    REQUIRE(exp.has_value());
    const auto* e = std::get_if<App::Cmd::StartExport>(&*exp);
    REQUIRE(e != nullptr);
    CHECK(e->req.output_path == "out.webm");
    CHECK(e->req.fps == 60);

    auto cancel = s.TakeCommand();
    REQUIRE(cancel.has_value());
    CHECK(std::holds_alternative<App::Cmd::CancelExport>(*cancel));
}

TEST_CASE("MutConfig creates and FindConfig locates by filename") {
    App::State s;
    CHECK(s.FindConfig("title.ifs") == nullptr);
    App::IfsConfig& cfg = s.MutConfig("title.ifs");
    cfg.bitmap_names.emplace_back("coin");

    const App::IfsConfig* found = s.FindConfig("title.ifs");
    REQUIRE(found != nullptr);
    REQUIRE(found->bitmap_names.size() == 1);
    CHECK(found->bitmap_names[0] == "coin");
}

TEST_CASE("Sublayer overrides upsert and copy out") {
    App::State s;
    s.SetSublayerOverride("bg.ifs", "content_usr", false);
    s.SetSublayerOverride("bg.ifs", "frame", true);
    s.SetSublayerOverride("bg.ifs", "content_usr", true);

    const std::vector<std::pair<std::string, bool>> ov = s.GetSublayerOverrides("bg.ifs");
    REQUIRE(ov.size() == 2);
    CHECK(ov[0].first == "content_usr");
    CHECK(ov[0].second);
    CHECK(ov[1].first == "frame");
    CHECK(s.GetSublayerOverrides("other.ifs").empty());
}

TEST_CASE("Sublayer expanded set toggles without duplicates") {
    App::State s;
    s.SetSublayerExpanded("a/b", true);
    s.SetSublayerExpanded("a/b", true);
    s.SetSublayerExpanded("a/c", true);
    CHECK(s.GetSublayerExpanded() == std::vector<std::string>{"a/b", "a/c"});
    s.SetSublayerExpanded("a/b", false);
    CHECK(s.GetSublayerExpanded() == std::vector<std::string>{"a/c"});
}

TEST_CASE("LoadProgress computes a clamped determinate fraction") {
    App::State s;
    s.BeginLoad("title.ifs");
    s.SetTexturesExpected(4);
    s.BumpTexturesLoaded();
    s.BumpTexturesLoaded();
    App::LoadProgress p = s.GetLoadProgress();
    CHECK(p.active);
    CHECK(p.target == "title.ifs");
    CHECK(p.fraction == 0.5F);

    s.BumpTexturesLoaded();
    s.BumpTexturesLoaded();
    s.BumpTexturesLoaded();
    p = s.GetLoadProgress();
    CHECK(p.fraction == 1.0F);
}

TEST_CASE("LoadProgress holds a Finalizing overlay after EndLoad") {
    App::State s;
    s.BeginLoad("title.ifs");
    s.UpdateLoadStage("Mounting IFS", 0.25F);
    s.EndLoad();

    const App::LoadProgress held = s.GetLoadProgress();
    CHECK(held.active);
    CHECK(held.stage == "Finalizing");
    CHECK(held.fraction == 1.0F);
    CHECK(held.target == "title.ifs");

    s.BeginLoad("next.ifs");
    const App::LoadProgress fresh = s.GetLoadProgress();
    CHECK(fresh.active);
    CHECK(fresh.stage.empty());
    CHECK(fresh.target == "next.ifs");
}

TEST_CASE("UpdateLoadStage keeps the fraction sentinel unless given one") {
    App::State s;
    s.BeginLoad("x.ifs");
    s.UpdateLoadStage("Scanning");
    CHECK(s.GetLoadProgress().fraction == -1.0F);
    s.UpdateLoadStage("Scanning", 0.75F);
    CHECK(s.GetLoadProgress().fraction == 0.75F);
}

TEST_CASE("SetMasterScale clamps to the sane range") {
    App::State s;
    s.SetMasterScale(0.05F);
    CHECK(s.GetMasterScale() == 0.1F);
    s.SetMasterScale(9.0F);
    CHECK(s.GetMasterScale() == 8.0F);
    s.SetMasterScale(1.5F);
    CHECK(s.GetMasterScale() == 1.5F);
}

TEST_CASE("SetLiveOverrides clamps loop mode and trim") {
    App::State s;
    App::State::LiveOverrides o;
    o.continuous_loop_mode = 5;
    o.trim_frames = -3;
    s.SetLiveOverrides(o);
    const App::State::LiveOverrides back = s.GetLiveOverrides();
    CHECK(back.continuous_loop_mode == 1);
    CHECK(back.trim_frames == 0);
}

TEST_CASE("SetRenderSize ignores non-positive values") {
    App::State s;
    int w = 0;
    int h = 0;
    s.GetRenderSize(w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
    s.SetRenderSize(0, -5);
    s.GetRenderSize(w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
    s.SetRenderSize(1080, 1920);
    s.GetRenderSize(w, h);
    CHECK(w == 1080);
    CHECK(h == 1920);
}

TEST_CASE("ActiveBackendId is empty before boot and round-trips") {
    App::State s;
    CHECK(s.ActiveBackendId().empty());
    s.SetActiveBackendId("afp_modern");
    CHECK(s.ActiveBackendId() == "afp_modern");
}

TEST_CASE("Defaults match the documented boot state") {
    App::State s;
    CHECK(s.GetBootState() == App::BootState::WaitingForDir);
    CHECK(s.GetRootLoopMode() == App::State::RootLoopMode::Hold);
    CHECK(s.GetRenderFps() == 120);
    CHECK_FALSE(s.ShouldExit().load());
    CHECK_FALSE(s.IsIfsScanning());
}
