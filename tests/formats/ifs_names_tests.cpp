#include <catch2/catch_test_macros.hpp>

#include "formats/ifs_names.h"

#include <string>

TEST_CASE("EscapeName maps path characters to manifest node names") {
    CHECK(Ifs::EscapeName("texturelist.xml") == "texturelist_Exml");
    CHECK(Ifs::EscapeName("08023_pre.2dx") == "_08023__pre_E2dx");
    CHECK(Ifs::EscapeName("a b$+-:@~") == "a_Ab_B_C_D_F_G_H");
    CHECK_FALSE(Ifs::EscapeName("a*b").has_value());
    CHECK_FALSE(Ifs::EscapeName(std::string("\x80")).has_value());
    CHECK_FALSE(Ifs::EscapeName("").has_value());
}

TEST_CASE("HashedName is the escaped MD5 of the logical name") {
    CHECK(Ifs::HashedName("bg03") == "bb595a0fb223760acd747d1bb1a277b0");
    CHECK(Ifs::HashedName("texturelist.xml") == "_6b95ffa0055ad5753a317bc477207969");
}

TEST_CASE("IsSpecialName matches the names the imagefs directory listing skips") {
    CHECK(Ifs::IsSpecialName("_info_"));
    CHECK(Ifs::IsSpecialName("_super_"));
    CHECK_FALSE(Ifs::IsSpecialName("_E2dx"));
    CHECK_FALSE(Ifs::IsSpecialName("__pre"));
    CHECK_FALSE(Ifs::IsSpecialName("_08023"));
    CHECK_FALSE(Ifs::IsSpecialName("tex"));
    CHECK_FALSE(Ifs::IsSpecialName("_"));
}
