#pragma once

#include "qpro_model.h"

#include <cstdint>
#include <string>
#include <vector>

namespace QproDll {

using Category = QproModel::Category;

struct Part {
    std::string ifs;
    uint8_t flag0 = 0;
};

struct Parts {
    std::vector<Part> cats[(int)Category::Count];
    std::string dll_path;
    std::string error;

    bool ok() const { return error.empty(); }
    const std::vector<Part>& of(Category c) const { return cats[(int)c]; }
    int total() const;
};

const char* JsonKey(Category c);
const char* Prefix(Category c);

Parts Read(const std::string& dll_or_game_dir);

std::string ToJson(const Parts& p);

}
