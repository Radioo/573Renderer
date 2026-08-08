#pragma once

#include "afp_ddr_funcs.h"
#include "avs_funcs.h"
#include "formats/txp2.h"

#include <cstdint>
#include <string>
#include <vector>

namespace DdrAfp {

struct Txp2Clip {
    std::string name;
    uint32_t stream_id = 0;
};

struct Txp2Texture {
    std::string name;
    int width = 0;
    int height = 0;
    int format = 0;
    std::vector<uint8_t> pixels;
};

struct Txp2Loaded {
    Txp2::Package package;
    std::vector<uint8_t> core;
    std::vector<Txp2Clip> clips;
    std::vector<Txp2Texture> textures;
};

bool InflateAvsLz(AvsFuncs& avs, const uint8_t* blob, size_t blob_size, std::vector<uint8_t>& out,
                  std::string& err);

bool ReadAvsFile(AvsFuncs& avs, const std::string& vfs_path, std::vector<uint8_t>& out,
                 std::string& err);

bool LoadTxp2Package(AvsFuncs& avs, const AfpDdrFuncs& afp, const std::string& vfs_path,
                     int package_slot, Txp2Loaded& out, std::string& err);

}
