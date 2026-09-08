#include "afp_package_id.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <optional>

TEST_CASE("afpu_ngp_read_local result wins when it found a package") {
    REQUIRE(AfpPackage::Resolve(0x1E180034, AfpPackage::kNotExist) ==
            std::optional<uint32_t>{0x1E180034});
}

TEST_CASE("first loaded package is used when the hint lookup found nothing") {
    REQUIRE(AfpPackage::Resolve(-1, 0x20250001) == std::optional<uint32_t>{0x20250001});
}

TEST_CASE("an archive with no afp package resolves to nothing") {
    REQUIRE_FALSE(AfpPackage::Resolve(-1, AfpPackage::kNotExist).has_value());
}

TEST_CASE("package id zero is not a package") {
    REQUIRE_FALSE(AfpPackage::Resolve(0, 0).has_value());
}
