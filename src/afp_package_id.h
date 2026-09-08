#pragma once

#include <cstdint>
#include <optional>

namespace AfpPackage {

constexpr uint32_t kNotExist = 0xFFFFFFFEU;

std::optional<uint32_t> Resolve(int ngp_read_local_result, uint32_t first_package_id);

}
