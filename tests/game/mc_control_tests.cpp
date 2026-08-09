#include <catch2/catch_test_macros.hpp>

#include "afp_funcs.h"
#include "mc_control.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

struct PropCall {
    int mc_id;
    uint32_t prop;
    intptr_t arg;
};

struct FakeAfp {
    int path_result = -1;
    std::string last_path;
    uint32_t last_stream = 0;
    std::vector<int> sibling_chain;
    std::vector<int> dfs_chain;
    int first_in_tree = -1;
    std::vector<PropCall> props;
    std::vector<std::pair<int, std::string>> bitmaps;
    std::vector<std::pair<int, std::vector<uint8_t>>> images;
};

FakeAfp& Fake() {
    static FakeAfp fake;
    return fake;
}

void ResetFake() {
    Fake() = FakeAfp{};
}

int FakeGetIdByPath(uint32_t stream_id, const char* path) {
    Fake().last_stream = stream_id;
    Fake().last_path = path != nullptr ? path : "";
    return Fake().path_result;
}

int FakeRelativeId(int mc_id, int direction) {
    if (direction == McControl::kDir_FirstInTree) return Fake().first_in_tree;
    const std::vector<int>& chain =
        (direction == McControl::kDir_NextInDFS) ? Fake().dfs_chain : Fake().sibling_chain;
    for (size_t i = 0; i + 1 < chain.size(); i++) {
        if (chain[i] == mc_id) return chain[i + 1];
    }
    return -1;
}

int FakeMcGet(int mc_id, uint32_t prop_id, intptr_t arg) {
    Fake().props.push_back({.mc_id = mc_id, .prop = prop_id, .arg = arg});
    return 0;
}

int FakeLoadBitmap(int mc_id, const char* bitmap_name, int attach) {
    (void)attach;
    Fake().bitmaps.emplace_back(mc_id, bitmap_name != nullptr ? bitmap_name : "");
    return 0;
}

int FakeLoadImage(int mc_id, const void* image_info, int attach) {
    (void)attach;
    std::vector<uint8_t> blob(32);
    std::memcpy(blob.data(), image_info, blob.size());
    Fake().images.emplace_back(mc_id, std::move(blob));
    return 0;
}

AfpFuncs MakeTable() {
    AfpFuncs afp;
    afp.afp_mc_get_id_by_path = &FakeGetIdByPath;
    afp.afp_mc_get_relative_id = &FakeRelativeId;
    afp.afp_mc_get = &FakeMcGet;
    afp.afp_play_work_load_bitmap = &FakeLoadBitmap;
    afp.afp_play_work_load_image = &FakeLoadImage;
    return afp;
}

uint32_t ReadU32(const std::vector<uint8_t>& b, size_t off) {
    uint32_t v = 0;
    std::memcpy(&v, &b.at(off), sizeof(v));
    return v;
}

uint16_t ReadU16(const std::vector<uint8_t>& b, size_t off) {
    uint16_t v = 0;
    std::memcpy(&v, &b.at(off), sizeof(v));
    return v;
}

float ReadF32(const std::vector<uint8_t>& b, size_t off) {
    float v = 0;
    std::memcpy(&v, &b.at(off), sizeof(v));
    return v;
}

}

TEST_CASE("FindClip forwards the stream and path, and guards a null table") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 7;
    CHECK(McControl::FindClip(afp, 0x1234, "qp_body_f") == 7);
    CHECK(Fake().last_stream == 0x1234);
    CHECK(Fake().last_path == "qp_body_f");

    CHECK(McControl::FindClip(afp, 1, nullptr) == -1);
    const AfpFuncs empty;
    CHECK(McControl::FindClip(empty, 1, "x") == -1);
}

TEST_CASE("SetClipVisible walks the sibling chain setting visible plus invalidate") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 10;
    Fake().sibling_chain = {10, 11, 12};

    McControl::SetClipVisible(afp, 1, "qp_hand_l_neutral", false);

    REQUIRE(Fake().props.size() == 6);
    CHECK(Fake().props[0].mc_id == 10);
    CHECK(Fake().props[0].prop == McControl::kProp_Visible);
    CHECK(Fake().props[0].arg == 0);
    CHECK(Fake().props[1].prop == McControl::kProp_Invalidate);
    CHECK(Fake().props[1].arg == 1);
    CHECK(Fake().props[4].mc_id == 12);
    CHECK(Fake().props[4].arg == 0);
}

TEST_CASE("SetClipVisible does nothing when the clip is missing") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = -1;
    McControl::SetClipVisible(afp, 1, "nope", true);
    CHECK(Fake().props.empty());
}

TEST_CASE("SetClipVisible stops at the 64-sibling cap") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 0;
    for (int i = 0; i < 200; i++)
        Fake().sibling_chain.push_back(i);

    McControl::SetClipVisible(afp, 1, "deep", true);
    CHECK(Fake().props.size() == size_t{128});
}

TEST_CASE("SetClipBitmap loads on every sibling and reports whether it ran") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 3;
    Fake().sibling_chain = {3, 4};

    CHECK(McControl::SetClipBitmap(afp, 1, "qp_bg", "bg_0002"));
    REQUIRE(Fake().bitmaps.size() == 2);
    CHECK(Fake().bitmaps[0].first == 3);
    CHECK(Fake().bitmaps[0].second == "bg_0002");
    CHECK(Fake().bitmaps[1].first == 4);

    Fake().path_result = -1;
    CHECK_FALSE(McControl::SetClipBitmap(afp, 1, "missing", "bg_0002"));
    CHECK_FALSE(McControl::SetClipBitmap(afp, 1, "qp_bg", nullptr));
}

TEST_CASE("BindImageToMc writes the 32-byte afp image-info blob") {
    ResetFake();
    const AfpFuncs afp = MakeTable();

    McControl::BindImageToMc(afp, 5, {.slot = 3, .w = 640, .h = 480});
    REQUIRE(Fake().images.size() == 1);
    CHECK(Fake().images[0].first == 5);
    const std::vector<uint8_t>& blob = Fake().images[0].second;
    CHECK(ReadU32(blob, 0) == 2);
    CHECK(ReadU16(blob, 4) == 640);
    CHECK(ReadU16(blob, 8) == 480);
    CHECK(ReadF32(blob, 12) == 0.0F);
    CHECK(ReadF32(blob, 16) == 1.0F);
    CHECK(ReadF32(blob, 20) == 0.0F);
    CHECK(ReadF32(blob, 24) == 1.0F);
    CHECK(ReadU16(blob, 6) == 0);

    REQUIRE(Fake().props.size() == 1);
    CHECK(Fake().props[0].prop == McControl::kProp_Invalidate);
}

TEST_CASE("BindImageToMc rejects a zero slot or a negative mc id") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    McControl::BindImageToMc(afp, 5, {.slot = 0, .w = 1, .h = 1});
    McControl::BindImageToMc(afp, -1, {.slot = 1, .w = 1, .h = 1});
    CHECK(Fake().images.empty());
}

TEST_CASE("ResolveSiblings fills the caller buffer up to its limit") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 2;
    Fake().sibling_chain = {2, 3, 4, 5};

    std::array<int, 3> ids = {-9, -9, -9};
    CHECK(McControl::ResolveSiblings(afp, 1, "clip", ids.data(), (int)ids.size()) == 3);
    CHECK(ids[0] == 2);
    CHECK(ids[2] == 4);

    CHECK(McControl::ResolveSiblings(afp, 1, "clip", ids.data(), 0) == 0);
    CHECK(McControl::ResolveSiblings(afp, 1, "clip", nullptr, 3) == 0);
}

TEST_CASE("BindClipImages binds slot zero to every sibling") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 1;
    Fake().sibling_chain = {1, 2, 3};
    const std::array<McControl::ImageSlot, 1> slots = {McControl::ImageSlot{
        .slot = 1,
        .w = 8,
        .h = 4,
    }};

    CHECK(McControl::BindClipImages(afp, 1, "clip", slots.data(), (int)slots.size()) == 3);
    REQUIRE(Fake().images.size() == 3);
    CHECK(ReadU32(Fake().images[0].second, 0) == 0);
    CHECK(ReadU16(Fake().images[2].second, 4) == 8);
}

TEST_CASE("BindClipImages refuses an empty or unslotted request") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 1;
    const std::array<McControl::ImageSlot, 1> bad = {McControl::ImageSlot{
        .slot = 0,
        .w = 8,
        .h = 4,
    }};
    CHECK(McControl::BindClipImages(afp, 1, "clip", bad.data(), 1) == 0);
    CHECK(McControl::BindClipImages(afp, 1, "clip", nullptr, 1) == 0);
    CHECK(McControl::BindClipImages(afp, 1, "clip", bad.data(), 0) == 0);
}

TEST_CASE("EnumerateClips walks first-in-tree then DFS and honours an early break") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 100;
    Fake().first_in_tree = 200;
    Fake().dfs_chain = {200, 201, 202, 203};

    std::vector<int> seen;
    const int n = McControl::EnumerateClips(
        afp, 1,
        [](int mc_id, void* user) {
            static_cast<std::vector<int>*>(user)->push_back(mc_id);
            return true;
        },
        &seen);
    CHECK(n == 4);
    CHECK(seen == std::vector<int>{200, 201, 202, 203});

    seen.clear();
    const int stopped = McControl::EnumerateClips(
        afp, 1,
        [](int mc_id, void* user) {
            static_cast<std::vector<int>*>(user)->push_back(mc_id);
            return mc_id < 201;
        },
        &seen);
    CHECK(stopped == 2);
    CHECK(seen == std::vector<int>{200, 201});
}

TEST_CASE("EnumerateClips falls back to the root when there is no first-in-tree") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 42;
    Fake().first_in_tree = -1;
    Fake().dfs_chain = {42, 43};

    std::vector<int> seen;
    CHECK(McControl::EnumerateClips(
              afp, 1,
              [](int mc_id, void* user) {
                  static_cast<std::vector<int>*>(user)->push_back(mc_id);
                  return true;
              },
              &seen) == 2);
    CHECK(seen == std::vector<int>{42, 43});
}

TEST_CASE("EnumerateClips returns zero when the root path does not resolve") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = -1;
    int calls = 0;
    CHECK(McControl::EnumerateClips(
              afp, 1,
              [](int, void* user) {
                  (*static_cast<int*>(user))++;
                  return true;
              },
              &calls) == 0);
    CHECK(calls == 0);

    const AfpFuncs empty;
    CHECK(McControl::EnumerateClips(empty, 1, [](int, void*) { return true; }, nullptr) == 0);
}

TEST_CASE("EnumerateClips stops at the caller's max_clips") {
    ResetFake();
    const AfpFuncs afp = MakeTable();
    Fake().path_result = 0;
    Fake().first_in_tree = 0;
    for (int i = 0; i < 50; i++)
        Fake().dfs_chain.push_back(i);

    int calls = 0;
    CHECK(McControl::EnumerateClips(
              afp, 1,
              [](int, void* user) {
                  (*static_cast<int*>(user))++;
                  return true;
              },
              &calls, 5) == 5);
    CHECK(calls == 5);
}
