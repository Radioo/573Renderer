#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GameFingerprint {

struct Build {
    const char* id;
    const char* name;
    const char* file;
    std::uint64_t size;
    std::uint32_t crc;
};

struct Match {
    const Build* build = nullptr;
    std::string file;
};

Match Identify(const std::string& game_dir);

}
