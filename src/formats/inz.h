#pragma once

#include <string>
#include <vector>

namespace Inz {

struct ImageSlice {
    std::string name;
    int flag = 0;
};

struct Pattern {
    std::string name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Manifest {
    std::vector<ImageSlice> slices;
    std::vector<Pattern> patterns;
};

bool Parse(const std::string& text, Manifest& out, std::string& err);

const Pattern* FindPattern(const Manifest& manifest, const std::string& texture_name);

}
