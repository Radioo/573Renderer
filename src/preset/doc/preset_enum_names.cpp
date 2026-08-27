#include "preset/doc/preset_enum_names.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace Preset::Doc {

bool IndexForName(std::span<const std::string_view> names, std::string_view name, int& index) {
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            index = (int)i;
            return true;
        }
    }
    return false;
}

std::string_view NameForIndex(std::span<const std::string_view> names, int index) {
    if (index < 0 || (std::size_t)index >= names.size()) return {};
    return names[(std::size_t)index];
}

}
