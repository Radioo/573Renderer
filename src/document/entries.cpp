#include "document/entries.h"

#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kAnimationDirectory = "afp";
constexpr std::string_view kScriptDirectory = "bsi";

bool Named(const Ifs::Entry& entry, std::string_view name) {
    const auto unescaped = Ifs::UnescapeName(entry.name);
    return (unescaped ? *unescaped : entry.name) == name;
}

std::string_view Head(std::string_view path) {
    return path.substr(0, path.find('/'));
}

bool Descend(std::string_view& path) {
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos) return false;
    path.remove_prefix(slash + 1);
    return true;
}

}

std::string JoinPath(std::string_view prefix, std::string_view name) {
    if (prefix.empty()) return std::string(name);
    return std::string(prefix) + "/" + std::string(name);
}

std::string ScriptPath(std::string_view animation_path) {
    if (!animation_path.starts_with(kAnimationDirectory) ||
        animation_path.size() <= kAnimationDirectory.size() + 1) {
        return {};
    }
    const std::string_view tail = animation_path.substr(kAnimationDirectory.size() + 1);
    return JoinPath(JoinPath(kAnimationDirectory, kScriptDirectory), tail);
}

const Ifs::Entry* FindEntry(const Ifs::Archive& archive, std::string_view path) {
    const std::vector<Ifs::Entry>* level = &archive.entries;
    while (!path.empty()) {
        const Ifs::Entry* found = nullptr;
        for (const Ifs::Entry& candidate : *level) {
            if (!Named(candidate, Head(path))) continue;
            found = &candidate;
            break;
        }
        if (found == nullptr) return nullptr;
        if (!Descend(path)) return found;
        level = &found->children;
    }
    return nullptr;
}

Ifs::Entry* FindEntry(Ifs::Archive& archive, std::string_view path) {
    std::vector<Ifs::Entry>* level = &archive.entries;
    while (!path.empty()) {
        Ifs::Entry* found = nullptr;
        for (Ifs::Entry& candidate : *level) {
            if (!Named(candidate, Head(path))) continue;
            found = &candidate;
            break;
        }
        if (found == nullptr) return nullptr;
        if (!Descend(path)) return found;
        level = &found->children;
    }
    return nullptr;
}

}
