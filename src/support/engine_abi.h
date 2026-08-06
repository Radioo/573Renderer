#pragma once

#include <cstddef>

#ifdef _WIN64
#define AFP_CB __fastcall
#else
#define AFP_CB __cdecl
#endif

namespace Support {

constexpr std::size_t kEngineSlot = sizeof(void*);

constexpr std::size_t SlotOffset(std::size_t slot_index) {
    return slot_index * kEngineSlot;
}

}
