#include <catch2/catch_test_macros.hpp>

#include "support/com_ptr.h"

#include <utility>

namespace {

struct CountedObject {
    int* releases = nullptr;
    void Release() const { ++*releases; }
};

}

TEST_CASE("Move-assigning a ComPtr releases the old object and takes the new one") {
    int first_releases = 0;
    int second_releases = 0;
    CountedObject first{.releases = &first_releases};
    CountedObject second{.releases = &second_releases};

    ComPtr<CountedObject> target;
    *target.GetAddressOf() = &first;
    {
        ComPtr<CountedObject> source;
        *source.GetAddressOf() = &second;
        target = std::move(source);
    }
    CHECK(first_releases == 1);
    CHECK(second_releases == 0);
    CHECK(target.ptr == &second);

    target.Reset();
    CHECK(second_releases == 1);
}
