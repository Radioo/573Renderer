#include "avs_xml.h"
#include "avs_funcs.h"
#include "support/log.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace AvsXml {

static constexpr unsigned kFlagsShort = 0x0011U;
static constexpr unsigned kFlagsLong = 0x1011U;

PropertyTree::~PropertyTree() {
    if (avs_ != nullptr) {
        if ((tree_ != nullptr) && (avs_->property_destroy != nullptr))
            avs_->property_destroy(tree_);
        if ((heap_ != nullptr) && (avs_->avs_gheap_free != nullptr)) avs_->avs_gheap_free(heap_);
    }
    tree_ = nullptr;
    heap_ = nullptr;
}

PropertyTree& PropertyTree::operator=(PropertyTree&& o) noexcept {
    if (this != &o) {
        this->~PropertyTree();
        avs_ = o.avs_;
        tree_ = o.tree_;
        heap_ = o.heap_;
        o.avs_ = nullptr;
        o.tree_ = nullptr;
        o.heap_ = nullptr;
    }
    return *this;
}

namespace {

bool AvsXmlExportsOk(const AvsFuncs& avs) {
    return (avs.avs_fs_open != nullptr) && (avs.avs_fs_lseek != nullptr) &&
           (avs.avs_fs_read != nullptr) && (avs.avs_fs_close != nullptr) &&
           (avs.property_read_query_memsize != nullptr) &&
           (avs.property_read_query_memsize_long != nullptr) && (avs.property_create != nullptr) &&
           (avs.property_insert_read != nullptr) && (avs.property_destroy != nullptr) &&
           (avs.avs_gheap_allocate != nullptr) && (avs.avs_gheap_free != nullptr);
}

int QueryPropertyMemsize(const AvsFuncs& avs, const char* path, int fd, unsigned& flags) {
    unsigned node_count = 0;
    int reserved = 0;
    unsigned char extra[40] = {};
    flags = kFlagsShort;

    int memsize = avs.property_read_query_memsize(avs.avs_fs_read, fd, &node_count, &reserved);
    avs.avs_fs_lseek(fd, 0, 0);

    if (memsize <= 0 || node_count > 0xFFFF) {
        memsize = avs.property_read_query_memsize_long(avs.avs_fs_read, fd, &node_count, &reserved,
                                                       extra);
        avs.avs_fs_lseek(fd, 0, 0);
        if (memsize <= 0) {
            LOG("AvsXml", "LoadFromFile('%s'): size check failed (memsize=%d)", path, memsize);
            return memsize;
        }
        flags = kFlagsLong;
    }
    return memsize;
}

}

PropertyTree LoadFromFile(const AvsFuncs& avs, const char* path) {
    PropertyTree empty;
    if (path == nullptr) return empty;
    if (!AvsXmlExportsOk(avs)) {
        LOG("AvsXml", "LoadFromFile: required AVS exports missing");
        return empty;
    }

    int const fd = avs.avs_fs_open(path, 1, 420);
    if (fd <= 0) {
        return empty;
    }
    LOG("AvsXml", "LoadFromFile('%s') fd=%d", path, fd);

    unsigned flags = kFlagsShort;
    int const memsize = QueryPropertyMemsize(avs, path, fd, flags);
    if (memsize <= 0) {
        avs.avs_fs_close(fd);
        return empty;
    }

    void* heap = avs.avs_gheap_allocate(0, (size_t)memsize, 0);
    if (heap == nullptr) {
        LOG("AvsXml", "LoadFromFile('%s'): avs_gheap_allocate(%d) failed", path, memsize);
        avs.avs_fs_close(fd);
        return empty;
    }
    T_PROPERTY* tree =
        avs.property_create(static_cast<int>(flags), heap, static_cast<unsigned>(memsize));
    if (tree == nullptr) {
        LOG("AvsXml", "LoadFromFile('%s'): property_create failed", path);
        avs.avs_gheap_free(heap);
        avs.avs_fs_close(fd);
        return empty;
    }

    int const rc = avs.property_insert_read(tree, nullptr, avs.avs_fs_read, fd);
    avs.avs_fs_close(fd);
    if (rc <= 0) {
        LOG("AvsXml", "LoadFromFile('%s'): property_insert_read rc=%d", path, rc);
        avs.property_destroy(tree);
        avs.avs_gheap_free(heap);
        return empty;
    }

    PropertyTree out;
    out.avs_ = &avs;
    out.tree_ = tree;
    out.heap_ = heap;
    return out;
}

T_PROPERTY_NODE* FindFirst(const AvsFuncs& avs, const PropertyTree& tree, const char* path) {
    if (!tree || (path == nullptr) || (avs.property_search == nullptr)) return nullptr;
    return avs.property_search(tree.Tree(), nullptr, path);
}

T_PROPERTY_NODE* NextMatch(const AvsFuncs& avs, T_PROPERTY_NODE* node) {
    if ((node == nullptr) || (avs.property_node_traversal == nullptr)) return nullptr;
    return avs.property_node_traversal(node, TRAVERSE_NEXT_MATCH);
}

bool ReadStrAttr(const AvsFuncs& avs, T_PROPERTY_NODE* node, const char* attr, char* out,
                 unsigned int out_size) {
    if ((node == nullptr) || (attr == nullptr) || (out == nullptr) || out_size == 0) return false;
    if (avs.property_node_refer == nullptr) return false;

    char path[64] = {};
    size_t const n = strlen(attr);
    if (n >= sizeof(path) - 1) return false;
    memcpy(path, attr, n);
    path[n] = '@';
    path[n + 1] = '\0';

    out[0] = '\0';
    int const rc = avs.property_node_refer(nullptr, node, path, TYPE_ATTR, out, out_size);
    return rc > 0 && out[0] != '\0';
}

bool ReadChild4U16(const AvsFuncs& avs, T_PROPERTY_NODE* node, const char* child, uint16_t out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if ((node == nullptr) || (child == nullptr) || (avs.property_node_refer == nullptr))
        return false;
    int const rc = avs.property_node_refer(nullptr, node, child, TYPE_4U16, out,
                                           (unsigned)(4 * sizeof(uint16_t)));
    return rc > 0;
}

int GatherMatchAttr(const AvsFuncs& avs, const PropertyTree& tree, const char* path,
                    const char* attr, std::vector<std::string>& out) {
    if (!tree || (path == nullptr) || (attr == nullptr)) return 0;
    int count = 0;
    T_PROPERTY_NODE* node = FindFirst(avs, tree, path);
    while (node != nullptr) {
        count++;
        char value[256] = {};
        if (ReadStrAttr(avs, node, attr, value, sizeof(value))) {
            out.emplace_back(value);
        }
        node = NextMatch(avs, node);
    }
    return count;
}

int CountMatches(const AvsFuncs& avs, const PropertyTree& tree, const char* path) {
    if (!tree || (path == nullptr)) return 0;
    int count = 0;
    T_PROPERTY_NODE* node = FindFirst(avs, tree, path);
    while (node != nullptr) {
        count++;
        node = NextMatch(avs, node);
    }
    return count;
}

}
