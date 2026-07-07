#include <catch2/catch_test_macros.hpp>

#include "state/app_state.h"
#include "state/boot_lifecycle.h"
#include "state/ifs_catalog.h"

#include <string>
#include <utility>
#include <vector>

TEST_CASE("TakeRequest returns nothing when no request is pending") {
    App::State s;
    CHECK_FALSE(s.TakeRequest().has_value());
}

TEST_CASE("PostRequest then TakeRequest hands over the request once") {
    App::State s;
    App::Request r;
    r.load_new_ifs = true;
    r.ifs_path = "a.ifs";
    s.PostRequest(r);

    const App::Request taken = s.TakeRequest().value_or(App::Request{});
    CHECK(taken.load_new_ifs);
    CHECK(taken.ifs_path == "a.ifs");
    CHECK_FALSE(s.TakeRequest().has_value());
}

TEST_CASE("PostRequest queues requests in FIFO order without dropping any") {
    App::State s;
    App::Request first;
    first.ifs_path = "first.ifs";
    App::Request second;
    second.ifs_path = "second.ifs";
    s.PostRequest(first);
    s.PostRequest(second);

    CHECK(s.TakeRequest().value_or(App::Request{}).ifs_path == "first.ifs");
    CHECK(s.TakeRequest().value_or(App::Request{}).ifs_path == "second.ifs");
    CHECK_FALSE(s.TakeRequest().has_value());
}

TEST_CASE("A burst of posts drains one per take in order") {
    App::State s;
    for (int i = 0; i < 8; i++) {
        App::Request r;
        r.ifs_path = std::to_string(i);
        s.PostRequest(r);
    }
    for (int i = 0; i < 8; i++) {
        CHECK(s.TakeRequest().value_or(App::Request{}).ifs_path == std::to_string(i));
    }
    CHECK_FALSE(s.TakeRequest().has_value());
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

TEST_CASE("Defaults match the documented boot state") {
    App::State s;
    CHECK(s.GetBootState() == App::BootState::WaitingForDir);
    CHECK(s.GetRootLoopMode() == App::State::RootLoopMode::Hold);
    CHECK(s.GetRenderFps() == 120);
    CHECK_FALSE(s.ShouldExit().load());
    CHECK_FALSE(s.IsIfsScanning());
}
