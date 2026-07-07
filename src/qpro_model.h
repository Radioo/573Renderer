#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace QproModel {

enum class Category { Body, Hand, Face, Hair, Head, Back, Count };

struct CategorySel {
    bool head = true;
    bool hand = true;
    bool hair = true;
    bool face = true;
    bool body = true;
    bool back = true;
    bool any() const { return head || hand || hair || face || body || back; }
};

struct PartSelection {
    std::vector<uint8_t> sel[(int)Category::Count];

    bool selected(Category c, size_t idx) const {
        const std::vector<uint8_t>& v = sel[(int)c];
        if (v.empty()) return true;
        return idx < v.size() ? v[idx] != 0 : true;
    }
    int selectedCount(Category c, int full_count) const {
        const std::vector<uint8_t>& v = sel[(int)c];
        if (v.empty()) return full_count;
        int n = 0;
        for (int i = 0; i < full_count; ++i)
            n += (i < (int)v.size() ? (v[i] != 0) : 1);
        return n;
    }
};

}
