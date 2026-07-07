#pragma once

#include <cstdint>
#include <windows.h>

namespace DllOffsets {

namespace AfpCore {
constexpr uintptr_t kCallbackTable = 0xE0E08;
constexpr uintptr_t kNearFarSlot = 0xE0E70;
constexpr uintptr_t kRenderFlags = 0xE1134;
}

namespace AfpUtils {
constexpr uintptr_t kDataStruct = 0x281F0;
constexpr uintptr_t kCmdBufUsed = 0x28810;
constexpr uintptr_t kCmdBufSize = 0x287FC;
constexpr uintptr_t kRenderContext = 0x28880;
constexpr uintptr_t kSetScreenRectFn = 0x18550;
}

template <typename T> T* At(HMODULE base, uintptr_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + offset);
}

}
