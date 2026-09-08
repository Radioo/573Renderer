#include "afp_package_id.h"

#include <cstdint>
#include <optional>

namespace AfpPackage {

std::optional<uint32_t> Resolve(int ngp_read_local_result, uint32_t first_package_id) {
    uint32_t const id =
        (ngp_read_local_result > 0) ? (uint32_t)ngp_read_local_result : first_package_id;
    if (id == 0U || id == kNotExist) return std::nullopt;
    return id;
}

}
