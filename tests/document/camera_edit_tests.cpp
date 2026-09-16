#include <catch2/catch_test_macros.hpp>

#include "document/camera_edit.h"
#include "document/outline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.container_version = 1;
    animation.magic = {'2', 'P', 'A'};
    animation.data_version = 8;
    animation.strings = {""};
    animation.fps = 60;
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

AfpAnimation::Camera& CameraOn(AfpAnimation::Animation& animation, uint32_t frame) {
    const auto found = Document::CameraTag(animation.root, frame);
    REQUIRE(found.has_value());
    auto* camera = std::get_if<AfpAnimation::Camera>(&animation.root.tags[*found].body);
    REQUIRE(camera != nullptr);
    return *camera;
}

std::string ValueOf(const std::vector<Document::Field>& fields, const std::string& name) {
    const auto found = std::ranges::find(fields, name, &Document::Field::name);
    REQUIRE(found != fields.end());
    return found->value;
}

bool FramesAreConsistent(const AfpAnimation::Container& clip) {
    std::size_t expected = clip.frames.empty() ? 0 : clip.frames.front().first_tag;
    for (const AfpAnimation::Frame& frame : clip.frames) {
        if (frame.first_tag != expected) return false;
        expected += frame.tag_count;
    }
    return expected == clip.tags.size();
}

}

TEST_CASE("A camera added to a frame lands on that frame with both fields present") {
    AfpAnimation::Animation animation = Clip(4);
    REQUIRE(Document::AddCamera(animation, 2, 7).has_value());
    CHECK(FramesAreConsistent(animation.root));
    CHECK(animation.root.tags.size() == 1);
    CHECK_FALSE(Document::CameraTag(animation.root, 1).has_value());

    const AfpAnimation::Camera& camera = CameraOn(animation, 2);
    CHECK(camera.id == 7);
    REQUIRE(camera.position.has_value());
    CHECK(*camera.position == std::array<int32_t, 3>{0, 0, 0});
    REQUIRE(camera.focal_length.has_value());
    CHECK(*camera.focal_length == 0);
}

TEST_CASE("A frame holds one camera") {
    AfpAnimation::Animation animation = Clip(2);
    REQUIRE(Document::AddCamera(animation, 0, 1).has_value());
    CHECK_FALSE(Document::AddCamera(animation, 0, 2).has_value());
    CHECK_FALSE(Document::AddCamera(animation, 9, 1).has_value());
    CHECK(animation.root.tags.size() == 1);
}

TEST_CASE("Removing a camera leaves the clip consistent") {
    AfpAnimation::Animation animation = Clip(3);
    REQUIRE(Document::AddCamera(animation, 0, 1).has_value());
    REQUIRE(Document::AddCamera(animation, 2, 1).has_value());
    REQUIRE(Document::RemoveCamera(animation, 0).has_value());
    CHECK(FramesAreConsistent(animation.root));
    CHECK_FALSE(Document::CameraTag(animation.root, 0).has_value());
    CHECK(Document::CameraTag(animation.root, 2).has_value());
    CHECK_FALSE(Document::RemoveCamera(animation, 0).has_value());
}

TEST_CASE("Camera fields read back what was written into them") {
    AfpAnimation::Animation animation = Clip(1);
    REQUIRE(Document::AddCamera(animation, 0, 3).has_value());
    AfpAnimation::Camera& camera = CameraOn(animation, 0);

    REQUIRE(Document::SetCameraField(camera, "Projection centre and depth", "1280, 720, -40")
                .has_value());
    REQUIRE(Document::SetCameraField(camera, "Focal length", "18000").has_value());
    REQUIRE(Document::SetCameraField(camera, "Camera", "9").has_value());

    const std::vector<Document::Field> fields = Document::CameraFields(camera);
    CHECK(ValueOf(fields, "Camera") == "9");
    CHECK(ValueOf(fields, "Projection centre and depth") == "1280, 720, -40");
    CHECK(ValueOf(fields, "Focal length") == "18000");
    CHECK(Document::CameraFieldIsEditable("Focal length"));
    CHECK_FALSE(Document::CameraFieldIsEditable("Depth"));
}

TEST_CASE("An emptied camera field drops the value the tag carries") {
    AfpAnimation::Animation animation = Clip(1);
    REQUIRE(Document::AddCamera(animation, 0, 1).has_value());
    AfpAnimation::Camera& camera = CameraOn(animation, 0);

    REQUIRE(Document::SetCameraField(camera, "Focal length", "").has_value());
    CHECK_FALSE(camera.focal_length.has_value());
    REQUIRE(Document::SetCameraField(camera, "Projection centre and depth", "").has_value());
    CHECK_FALSE(camera.position.has_value());

    CHECK(Document::CameraFields(camera)[2].value.empty());
}

TEST_CASE("A camera field refuses a value the tag cannot hold") {
    AfpAnimation::Animation animation = Clip(1);
    REQUIRE(Document::AddCamera(animation, 0, 1).has_value());
    AfpAnimation::Camera& camera = CameraOn(animation, 0);

    CHECK_FALSE(Document::SetCameraField(camera, "Camera", "").has_value());
    CHECK_FALSE(Document::SetCameraField(camera, "Camera", "-1").has_value());
    CHECK_FALSE(Document::SetCameraField(camera, "Camera", "70000").has_value());
    CHECK_FALSE(
        Document::SetCameraField(camera, "Projection centre and depth", "1, 2").has_value());
    CHECK_FALSE(Document::SetCameraField(camera, "Focal length", "wide").has_value());
    CHECK_FALSE(Document::SetCameraField(camera, "Lens", "1").has_value());
    CHECK(camera.id == 1);
}

TEST_CASE("A camera the editor writes survives the animation format") {
    AfpAnimation::Animation animation = Clip(2);
    REQUIRE(Document::AddCamera(animation, 1, 2).has_value());
    AfpAnimation::Camera& camera = CameraOn(animation, 1);
    REQUIRE(Document::SetCameraField(camera, "Projection centre and depth", "12800, 7200, 250")
                .has_value());
    REQUIRE(Document::SetCameraField(camera, "Focal length", "-6000").has_value());

    const auto written = AfpAnimation::Write(animation);
    if (!written) FAIL(written.error());
    const auto read = AfpAnimation::Read(written->data);
    if (!read) FAIL(read.error());
    CHECK(read->root == animation.root);

    const auto tag = Document::CameraTag(read->root, 1);
    REQUIRE(tag.has_value());
    const auto* stored = std::get_if<AfpAnimation::Camera>(&read->root.tags[*tag].body);
    REQUIRE(stored != nullptr);
    CHECK(stored->id == 2);
    REQUIRE(stored->position.has_value());
    CHECK(*stored->position == std::array<int32_t, 3>{12800, 7200, 250});
    REQUIRE(stored->focal_length.has_value());
    CHECK(*stored->focal_length == -6000);
}
