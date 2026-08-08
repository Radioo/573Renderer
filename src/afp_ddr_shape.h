#pragma once

#include <cstdint>

namespace DdrAfp {

struct GeoTriBatch {
    int texture_slot = -1;
    uint32_t modulate = 0xFFFFFFFFU;
    const float* positions = nullptr;
    const float* uvs = nullptr;
    const uint8_t* colors = nullptr;
    const uint16_t* indices = nullptr;
    int index_count = 0;
    int vertex_count = 0;
};

using GeoSink = void (*)(const GeoTriBatch&);

struct ShapeProvider {
    bool (*find)(const char* name, uint32_t* out_id) = nullptr;
    bool (*rect)(uint32_t id, float* out4) = nullptr;
    bool (*draw)(uint32_t id, const float* modulate, GeoSink sink) = nullptr;
};

}
