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

TEST_CASE("A duplicated animation and its original both draw as the original did") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    auto original = Document::File::Open(LiveStage::ReadAll(dir + "/data/graphic/1/title.ifs"));
    REQUIRE(original.has_value());
    if (!original) return;
    const std::string path = LiveStage::AnimationPath(*original, "title");
    REQUIRE(!path.empty());

    Document::File duplicated = *original;
    const auto copied = duplicated.DuplicateAnimation(path, "copied_title");
    const std::string error = copied.has_value() ? std::string() : copied.error();
    INFO(error);
    REQUIRE(copied.has_value());
    CHECK(LiveStage::AnimationPath(duplicated, "copied_title") == *copied);
    CHECK(LiveStage::AnimationPath(duplicated, "title") == path);

    LiveStage::Stage before(dir, LiveStage::Target{.package = "title", .animation = "title"});
    const std::vector<uint8_t> expected = before.Render(*original, kFrame);
    CHECK(before.Render(duplicated, kFrame) == expected);
    LiveStage::Stage copy(dir, LiveStage::Target{.package = "title", .animation = "copied_title"});
    CHECK(copy.Render(duplicated, kFrame) == expected);
}
