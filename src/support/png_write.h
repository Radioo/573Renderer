#pragma once

#include <cstdint>
#include <string>

namespace Support {

bool WritePngBGRA(const std::string& path, const uint8_t* bgra, int w, int h);

}
