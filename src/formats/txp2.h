#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Txp2 {

struct AfpEntry {
    std::string name;
    uint32_t size = 0;
    uint32_t data_offset = 0;
};

struct TextureEntry {
    std::string name;
    uint32_t size = 0;
    uint32_t file_offset = 0;
};

struct CellEntry {
    uint16_t texture_index = 0;
    uint16_t x0 = 0;
    uint16_t y0 = 0;
    uint16_t x1 = 0;
    uint16_t y1 = 0;
};

struct GeoPrim {
    uint8_t flags = 0;
    uint8_t bitmap_ref = 0;
    uint16_t index_count = 0;
    std::array<uint8_t, 4> rgba = {255, 255, 255, 255};
    std::vector<uint16_t> indices;
};

struct GeoShape {
    std::string name;
    uint16_t vertex_count = 0;
    uint16_t uv_count = 0;
    uint16_t color_count = 0;
    std::vector<float> positions;
    std::vector<float> uvs;
    std::vector<uint8_t> colors;
    std::vector<std::string> bitmap_names;
    std::vector<GeoPrim> prims;
};

struct CellName {
    std::string name;
    uint32_t cell_index = 0;
};

struct Package {
    bool big_endian = true;
    uint32_t flags = 0;
    uint32_t header_size = 0;
    uint32_t core_size = 0;

    std::vector<AfpEntry> afp_entries;
    std::vector<TextureEntry> textures;
    std::vector<CellEntry> cells;
    std::vector<CellName> cell_names;
    std::vector<GeoShape> shapes;

    uint32_t appended_block_offset = 0;
    bool textures_lz_compressed = false;
    bool names_obfuscated = false;
    bool legacy_lz = false;
};

bool Parse(const std::vector<uint8_t>& core, Package& out, std::string& err);

uint32_t SectionDwordCount(uint32_t flag_bit);

}
