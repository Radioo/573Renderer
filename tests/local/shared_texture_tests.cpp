#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preview/shared_texture.h"
#include "render_backend.h"
#include "shared_frame.h"

#include <windows.h>

#include <d3d9.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 48;
constexpr uint8_t kBlue = 10;
constexpr uint8_t kGreen = 200;
constexpr uint8_t kRed = 30;
constexpr std::size_t kBytesPerPixel = 4;

HWND HiddenWindow() {
    const char* kClass = "r573_shared_texture_test";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "shared texture", WS_OVERLAPPED, 0, 0, kWidth, kHeight,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

}

TEST_CASE("The reader gets back the frame another device drew into a shared texture") {
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    D3D9State host;
    host.width = kWidth;
    host.height = kHeight;
    REQUIRE(host.Init(hwnd));
    REQUIRE(SUCCEEDED(host.device->ColorFill(host.offscreen_rt, nullptr,
                                             D3DCOLOR_ARGB(255, kRed, kGreen, kBlue))));

    const auto target = SharedFrame::Create(host.device, kWidth, kHeight);
    const std::string create_error = target.has_value() ? std::string() : target.error();
    INFO(create_error);
    REQUIRE(target.has_value());
    const auto copied = SharedFrame::Copy(host.device, host.offscreen_rt, *target);
    INFO((copied ? std::string() : copied.error()));
    REQUIRE(copied.has_value());

    auto reader = SharedTexture::Reader::Create();
    const std::string reader_error = reader.has_value() ? std::string() : reader.error();
    INFO(reader_error);
    REQUIRE(reader.has_value());

    const auto handle = static_cast<uint64_t>(std::bit_cast<uintptr_t>(target->handle));
    const auto pixels = reader->Read(handle, kWidth, kHeight);
    const std::string read_error = pixels.has_value() ? std::string() : pixels.error();
    INFO(read_error);
    REQUIRE(pixels.has_value());
    REQUIRE(pixels->size() == static_cast<std::size_t>(kWidth) * kHeight * kBytesPerPixel);
    CHECK((*pixels)[0] == kBlue);
    CHECK((*pixels)[1] == kGreen);
    CHECK((*pixels)[2] == kRed);
    CHECK((*pixels)[3] == 255);

    const auto again = reader->Read(handle, kWidth, kHeight);
    REQUIRE(again.has_value());
    CHECK((*again)[1] == kGreen);

    CHECK_FALSE(reader->Read(handle, 0, kHeight).has_value());

    host.Shutdown();
    DestroyWindow(hwnd);
}
