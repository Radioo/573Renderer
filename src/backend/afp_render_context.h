#pragma once

#include <cstdint>
#include <cstring>

struct IDirect3DDevice9;

struct AfpRenderContext {
    static constexpr size_t kDeviceOffset = 0x18000;
    static constexpr size_t kSize = 0x20000;
    uint8_t data[kSize];

    void InitZero() { memset(data, 0, sizeof(data)); }
    uint32_t& Flags() { return *(uint32_t*)&data[0]; }
    void*& FnAt(int offset) { return *(void**)&data[offset]; }

    IDirect3DDevice9*& DeviceAt() {
        return *reinterpret_cast<IDirect3DDevice9**>(&data[kDeviceOffset]);
    }
};
