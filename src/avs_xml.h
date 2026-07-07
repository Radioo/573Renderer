#pragma once

#include "avs_funcs.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace AvsXml {

enum Traverse {
    TRAVERSE_PARENT = 0,
    TRAVERSE_FIRST_CHILD = 1,
    TRAVERSE_FIRST_ATTR = 2,
    TRAVERSE_FIRST_SIBLING = 3,
    TRAVERSE_NEXT_SIBLING = 4,
    TRAVERSE_PREV_SIBLING = 5,
    TRAVERSE_LAST_SIBLING = 6,
    TRAVERSE_NEXT_MATCH = 7,
    TRAVERSE_LAST_MATCH = 8,
};

constexpr int TYPE_ATTR = 0x2E;
constexpr int TYPE_STR = 0x0B;
constexpr int TYPE_4U16 = 0x27;

class PropertyTree {
public:
    PropertyTree() = default;
    ~PropertyTree();
    PropertyTree(const PropertyTree&) = delete;
    PropertyTree& operator=(const PropertyTree&) = delete;
    PropertyTree(PropertyTree&& o) noexcept { *this = std::move(o); }
    PropertyTree& operator=(PropertyTree&& o) noexcept;

    explicit operator bool() const { return tree_ != nullptr; }

    T_PROPERTY* Tree() const { return tree_; }

private:
    friend PropertyTree LoadFromFile(const AvsFuncs& avs, const char* path);
    const AvsFuncs* avs_ = nullptr;
    T_PROPERTY* tree_ = nullptr;
    void* heap_ = nullptr;
};

PropertyTree LoadFromFile(const AvsFuncs& avs, const char* path);

T_PROPERTY_NODE* FindFirst(const AvsFuncs& avs, const PropertyTree& tree, const char* path);

T_PROPERTY_NODE* NextMatch(const AvsFuncs& avs, T_PROPERTY_NODE* node);

bool ReadStrAttr(const AvsFuncs& avs, T_PROPERTY_NODE* node, const char* attr, char* out,
                 unsigned int out_size);

bool ReadChild4U16(const AvsFuncs& avs, T_PROPERTY_NODE* node, const char* child, uint16_t out[4]);

int GatherMatchAttr(const AvsFuncs& avs, const PropertyTree& tree, const char* path,
                    const char* attr, std::vector<std::string>& out);

int CountMatches(const AvsFuncs& avs, const PropertyTree& tree, const char* path);

}
