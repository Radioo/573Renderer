#include "preset/preset_preview.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

Preset::Preview::SnapshotPtr Made(const std::string& key) {
    auto snapshot = std::make_shared<Preset::Preview::Snapshot>();
    snapshot->key = key;
    snapshot->total = 6;
    return snapshot;
}

Preset::Preview::Request Ask(const std::string& asset, const std::string& animation) {
    Preset::Preview::Request request;
    request.key = Preset::Preview::KeyFor(asset, animation, {});
    request.asset = asset;
    request.animation = animation;
    request.samples = 6;
    request.length = 120;
    return request;
}

}

TEST_CASE("the preview key separates the hidden parts from the animation", "[preview]") {
    CHECK(Preset::Preview::KeyFor("title", "TITLE", {}) == "title/TITLE");
    CHECK(Preset::Preview::KeyFor("title", "TITLE", {"OP_BG_U"}) == "title/TITLE|OP_BG_U");
    CHECK(Preset::Preview::KeyFor("title", "TITLE", {"OP_BG_U"}) !=
          Preset::Preview::KeyFor("title", "TITLE", {"OP_BG_D"}));
}

TEST_CASE("the preview cache keeps the eight most recently used keys", "[preview]") {
    Preset::Preview::Cache cache(8);
    for (int i = 0; i < 8; i++)
        cache.Insert("k" + std::to_string(i), Made("k" + std::to_string(i)));

    REQUIRE(cache.Find("k0") != nullptr);
    CHECK(cache.Find("k0")->key == "k0");

    cache.Insert("k8", Made("k8"));
    CHECK(cache.Find("k8") != nullptr);
    CHECK(cache.Find("k0") != nullptr);
    CHECK(cache.Find("k1") == nullptr);

    cache.Clear();
    CHECK(cache.Find("k8") == nullptr);
}

TEST_CASE("re-posting a cached key republishes it complete instead of re-rendering", "[preview]") {
    Preset::Preview::Reset();

    Preset::Preview::Post(Ask("title", "TITLE"));
    const Preset::Preview::SnapshotPtr first = Preset::Preview::Get();
    REQUIRE(first != nullptr);
    CHECK(first->key == "title/TITLE");
    CHECK(first->samples.empty());

    Preset::Preview::Post(Ask("title", "LOGIN"));
    CHECK(Preset::Preview::Get()->key == "title/LOGIN");

    Preset::Preview::Reset();
    CHECK(Preset::Preview::Get() == nullptr);
}

TEST_CASE("Get hands out the same snapshot pointer until a sample lands", "[preview]") {
    Preset::Preview::Reset();
    Preset::Preview::Post(Ask("title", "TITLE"));

    const Preset::Preview::SnapshotPtr a = Preset::Preview::Get();
    const Preset::Preview::SnapshotPtr b = Preset::Preview::Get();
    REQUIRE(a != nullptr);
    CHECK(a.get() == b.get());

    Preset::Preview::Reset();
}

TEST_CASE("teardown from another thread frees no device object, it defers to the next Pump",
          "[preview]") {
    Preset::Preview::Reset();
    Preset::Preview::Post(Ask("title", "TITLE"));
    REQUIRE(Preset::Preview::Get() != nullptr);

    Preset::Preview::Reset();
    CHECK(Preset::Preview::Get() == nullptr);
    CHECK_FALSE(Preset::Preview::Pump());
    CHECK_FALSE(Preset::Preview::Pump());

    Preset::Preview::Post(Ask("title", "TITLE"));
    CHECK(Preset::Preview::Get() != nullptr);
    Preset::Preview::Reset();
}
