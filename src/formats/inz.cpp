#include "formats/inz.h"

#include <array>
#include <cstddef>
#include <exception>
#include <utility>
#include <string>
#include <vector>

namespace Inz {

namespace {

int ToInt(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (const std::exception&) {
        return 0;
    }
}

std::string Trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
        b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
        e--;
    return s.substr(b, e - b);
}

std::string BaseName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string StripDottedSuffix(const std::string& name) {
    if (name.size() > 4 && name[name.size() - 4] == '.') return name.substr(0, name.size() - 4);
    return name;
}

std::string StripBmpSuffix(const std::string& name) {
    if (name.size() > 4 && name.ends_with(".bmp")) return name.substr(0, name.size() - 4);
    return name;
}

void ParseSlice(const std::string& line, Manifest& out) {
    const size_t comma = line.find_last_of(',');
    ImageSlice slice;
    if (comma == std::string::npos) {
        slice.name = BaseName(line);
    } else {
        slice.name = BaseName(line.substr(0, comma));
        slice.flag = ToInt(line.substr(comma + 1));
    }
    if (!slice.name.empty()) out.slices.push_back(std::move(slice));
}

void ParsePattern(const std::string& line, Manifest& out) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return;
    Pattern p;
    p.name = StripDottedSuffix(BaseName(Trim(line.substr(0, eq))));
    const std::string rect = Trim(line.substr(eq + 1));
    std::array<int, 4> vals = {0, 0, 0, 0};
    size_t at = 0;
    for (int& v : vals) {
        if (at >= rect.size()) return;
        const size_t comma = rect.find(',', at);
        const size_t end = (comma == std::string::npos) ? rect.size() : comma;
        v = ToInt(rect.substr(at, end - at));
        at = (comma == std::string::npos) ? rect.size() : comma + 1;
    }
    p.x = vals[0];
    p.y = vals[1];
    p.w = vals[2];
    p.h = vals[3];
    if (!p.name.empty() && p.w > 0 && p.h > 0) out.patterns.push_back(std::move(p));
}

const Pattern* FindPattern(const Manifest& manifest, const std::string& texture_name) {
    const std::string want = StripBmpSuffix(BaseName(texture_name));
    for (const auto& p : manifest.patterns) {
        if (p.name == want) return &p;
    }
    return nullptr;
}

}

bool Parse(const std::string& text, Manifest& out, std::string& err) {
    out.slices.clear();
    out.patterns.clear();

    int section = 0;
    size_t at = 0;
    while (at <= text.size()) {
        const size_t nl = text.find('\n', at);
        const std::string line = Trim(text.substr(at, nl == std::string::npos ? nl : nl - at));
        at = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
        if (line.empty()) continue;
        if (line.front() == '[') {
            if (line == "[image_file]") {
                section = 1;
            } else if (line == "[pattern_list]") {
                section = 2;
            } else {
                section = 0;
            }
            continue;
        }
        if (section == 1) ParseSlice(line, out);
        if (section == 2) ParsePattern(line, out);
    }

    if (out.slices.empty()) {
        err = "manifest lists no [image_file] slices";
        return false;
    }
    return true;
}

Region ResolveRegion(const Manifest& manifest, const std::string& texture_name,
                     const AtlasGrid& grid) {
    Region region;
    if (grid.tile_width <= 0 || grid.tile_height <= 0 || grid.tiles_per_row <= 0) return region;
    const Pattern* p = FindPattern(manifest, texture_name);
    if (p == nullptr) return region;
    region.tile = (p->x / grid.tile_width) + (grid.tiles_per_row * (p->y / grid.tile_height));
    region.u_scale = (float)p->w / (float)grid.tile_width;
    region.v_scale = (float)p->h / (float)grid.tile_height;
    region.u_bias = (float)(p->x % grid.tile_width) / (float)grid.tile_width;
    region.v_bias = (float)(p->y % grid.tile_height) / (float)grid.tile_height;
    return region;
}

}
