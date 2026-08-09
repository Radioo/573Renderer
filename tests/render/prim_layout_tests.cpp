#include <catch2/catch_test_macros.hpp>

#include "render/prim_layout.h"

TEST_CASE("DecodeVtxLayout derives the stride from the flag bits") {
    const Render::VtxLayout uv_col_pos2 = Render::DecodeVtxLayout(0x08 | 0x04 | 0x01);
    CHECK(uv_col_pos2.has_uv);
    CHECK(uv_col_pos2.has_vcol);
    CHECK(uv_col_pos2.pos2);
    CHECK_FALSE(uv_col_pos2.pos3);
    CHECK_FALSE(uv_col_pos2.skip2);
    CHECK(uv_col_pos2.stride == 5);

    const Render::VtxLayout full = Render::DecodeVtxLayout(0x08 | 0x10 | 0x04 | 0x02);
    CHECK(full.skip2);
    CHECK(full.pos3);
    CHECK(full.stride == 8);

    const Render::VtxLayout pos3_wins = Render::DecodeVtxLayout(0x02 | 0x01);
    CHECK(pos3_wins.stride == 3);

    CHECK(Render::DecodeVtxLayout(0).stride == 0);
}

TEST_CASE("MapPrimType maps afp primitive ids to D3D9 types and counts") {
    int prims = -1;

    CHECK(Render::MapPrimType(1, 10, prims) == D3DPT_LINELIST);
    CHECK(prims == 5);
    CHECK(Render::MapPrimType(3, 10, prims) == D3DPT_LINELIST);
    CHECK(prims == 5);

    CHECK(Render::MapPrimType(2, 10, prims) == D3DPT_LINESTRIP);
    CHECK(prims == 9);

    CHECK(Render::MapPrimType(4, 9, prims) == D3DPT_TRIANGLELIST);
    CHECK(prims == 3);

    CHECK(Render::MapPrimType(5, 10, prims) == D3DPT_TRIANGLESTRIP);
    CHECK(prims == 8);

    CHECK(Render::MapPrimType(6, 10, prims) == D3DPT_TRIANGLEFAN);
    CHECK(prims == 8);

    CHECK(Render::MapPrimType(0, 7, prims) == D3DPT_POINTLIST);
    CHECK(prims == 7);
    CHECK(Render::MapPrimType(99, 7, prims) == D3DPT_POINTLIST);
    CHECK(prims == 7);
}

TEST_CASE("MulARGB multiplies each channel with correct rounding") {
    CHECK(Render::MulARGB(0xFFFFFFFFU, 0xFFFFFFFFU) == 0xFFFFFFFFU);
    CHECK(Render::MulARGB(0xFFFFFFFFU, 0x00000000U) == 0x00000000U);
    CHECK(Render::MulARGB(0x80808080U, 0xFFFFFFFFU) == 0x80808080U);
    CHECK(Render::MulARGB(0x80808080U, 0x80808080U) == 0x40404040U);
    CHECK(Render::MulARGB(0xFF000000U, 0x00FFFFFFU) == 0x00000000U);
    CHECK(Render::MulARGB(0x01010101U, 0xFF00FF00U) == 0x01000100U);
}
