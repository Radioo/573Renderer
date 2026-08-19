#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/xfile.h"
#include "scene3d/anim.h"

#include <vector>

namespace {

XFile::AnimationKey PositionKey(int time, float x) {
    XFile::AnimationKey key;
    key.time = time;
    key.value[0] = x;
    return key;
}

XFile::AnimationChannel Slide(int last, float reach) {
    XFile::AnimationChannel channel;
    channel.position = {PositionKey(0, 0.0F), PositionKey(last, reach)};
    return channel;
}

float X(const XFile::AnimationChannel& channel, float time) {
    return Scene3d::SampleChannel(channel, time)[12];
}

}

TEST_CASE("a key track loops on its own last key time instead of holding its last key") {
    const XFile::AnimationChannel slide = Slide(30, 30.0F);
    CHECK(X(slide, 15.0F) == Catch::Approx(15.0F));
    CHECK(X(slide, 30.0F) == Catch::Approx(0.0F));
    CHECK(X(slide, 45.0F) == Catch::Approx(15.0F));
    CHECK(X(slide, 300.0F) == Catch::Approx(0.0F));
    CHECK(X(slide, -15.0F) == Catch::Approx(15.0F));
}

TEST_CASE("a scene's loop length is the least common multiple of its key tracks, not their max") {
    XFile::Scene scene;
    scene.channels.push_back(Slide(30, 1.0F));
    scene.channels.push_back(Slide(300, 1.0F));
    CHECK(Scene3d::LoopTicks(scene) == 300);
    XFile::Scene dan;
    dan.channels.push_back(Slide(600, 1.0F));
    dan.channels.push_back(Slide(1000, 1.0F));
    dan.channels.push_back(Slide(1500, 1.0F));
    CHECK(Scene3d::LoopTicks(dan) == 3000);
    CHECK(Scene3d::LoopTicks(XFile::Scene{}) == 0);
}
