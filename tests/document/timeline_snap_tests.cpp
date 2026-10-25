#include <catch2/catch_test_macros.hpp>

#include "document/timeline.h"
#include "document/timeline_snap.h"

#include <cstdint>
#include <vector>

namespace {

std::vector<Document::DepthRow> Rows() {
    return {Document::DepthRow{
                .depth = 1,
                .spans = {{.first_frame = 0, .last_frame = 3}, {.first_frame = 8, .last_frame = 9}},
                .shows = {}},
            Document::DepthRow{
                .depth = 2, .spans = {{.first_frame = 2, .last_frame = 5}}, .shows = {}}};
}

}

TEST_CASE("A dragged span snaps to where other spans start and end and to the marks") {
    const std::vector<uint32_t> targets =
        Document::SnapTargets(Rows(), 1, {.first_frame = 8, .last_frame = 9}, {7, 12, 0});
    CHECK(targets == std::vector<uint32_t>{0, 2, 4, 6, 7, 12});
}

TEST_CASE("A span's move snaps whichever of its ends lands nearest a target") {
    const std::vector<uint32_t> targets{6, 20};
    const Document::Span span{.first_frame = 10, .last_frame = 13};
    CHECK(Document::SnapShift(span, -3, targets, 2) == -4);
    CHECK(Document::SnapShift(span, 5, targets, 2) == 6);
    CHECK(Document::SnapShift(span, -9, targets, 2) == -8);
    CHECK(Document::SnapShift(span, 3, targets, 2) == 3);
    CHECK(Document::SnapShift(span, -3, targets, 0) == -3);
    CHECK(Document::SnapShift({.first_frame = 10, .last_frame = 11}, 1, {12, 14}, 2) == 2);
}

TEST_CASE("A trimmed edge snaps to the nearest target within reach") {
    const std::vector<uint32_t> targets{4, 9};
    CHECK(Document::SnapEdge(5, targets, 1) == 4);
    CHECK(Document::SnapEdge(7, targets, 2) == 9);
    CHECK(Document::SnapEdge(6, targets, 1) == 6);
    CHECK(Document::SnapEdge(10, targets, 3) == 9);
    CHECK(Document::SnapEdge(6, {4, 8}, 2) == 4);
}
