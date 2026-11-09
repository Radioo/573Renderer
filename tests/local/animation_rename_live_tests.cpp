#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/document.h"
#include "live_stage.h"
#include "support/env.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kFrame = 400;

}

TEST_CASE("A renamed animation loads under its new name and draws as before") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto original = Document::File::Open(LiveStage::ReadAll(dir + "/data/graphic/1/title.ifs"));
    REQUIRE(original.has_value());
    if (!original) return;
    const std::string path = LiveStage::AnimationPath(*original, "title");
    REQUIRE(!path.empty());

    Document::File renamed = *original;
    const auto moved = renamed.RenameAnimation(path, "renamed_title");
    const std::string error = moved.has_value() ? std::string() : moved.error();
    INFO(error);
    REQUIRE(moved.has_value());
    CHECK(LiveStage::AnimationPath(renamed, "renamed_title") == *moved);
    CHECK(LiveStage::AnimationPath(renamed, "title").empty());

    LiveStage::Stage before(dir, LiveStage::Target{.package = "title", .animation = "title"});
    const std::vector<uint8_t> expected = before.Render(*original, kFrame);
    LiveStage::Stage after(dir,
                           LiveStage::Target{.package = "title", .animation = "renamed_title"});
    CHECK(after.Render(renamed, kFrame) == expected);
}
