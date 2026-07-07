#include <catch2/catch_test_macros.hpp>

#include "state/fifo_queue.h"

#include <memory>
#include <string>

TEST_CASE("FifoQueue takes nothing when empty") {
    App::FifoQueue<std::string> q;
    CHECK_FALSE(q.Take().has_value());
    CHECK(q.Size() == 0);
}

TEST_CASE("FifoQueue preserves post order") {
    App::FifoQueue<std::string> q;
    q.Post("a");
    q.Post("b");
    q.Post("c");
    CHECK(q.Size() == 3);
    CHECK(q.Take().value_or("") == "a");
    CHECK(q.Take().value_or("") == "b");
    CHECK(q.Take().value_or("") == "c");
    CHECK_FALSE(q.Take().has_value());
}

TEST_CASE("FifoQueue moves move-only payloads through intact") {
    App::FifoQueue<std::unique_ptr<int>> q;
    q.Post(std::make_unique<int>(7));
    auto taken = q.Take();
    REQUIRE(taken.has_value());
    if (taken.has_value()) {
        REQUIRE(*taken != nullptr);
        if (*taken != nullptr) CHECK(**taken == 7);
    }
    CHECK_FALSE(q.Take().has_value());
}
