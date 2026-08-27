#include "editor/library_model.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

namespace {

constexpr std::string_view kFallbackId = "preset";

std::string Lowered(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text)
        out.push_back((char)std::tolower((unsigned char)c));
    return out;
}

bool Matches(const LibraryEntry& entry, const std::string& filter) {
    if (filter.empty()) return true;
    return Lowered(entry.name).find(filter) != std::string::npos ||
           Lowered(entry.id).find(filter) != std::string::npos;
}

bool Taken(const std::vector<std::string>& taken, const std::string& id) {
    return std::ranges::find(taken, id) != taken.end();
}

}

std::vector<LibraryGroup> GroupLibrary(const std::vector<LibraryEntry>& entries,
                                       std::string_view filter) {
    const std::string needle = Lowered(filter);
    const std::array<LibraryGroup, 3> order = {
        LibraryGroup{.label = "Built-in", .source = LibrarySource::BuiltIn},
        LibraryGroup{.label = "User", .source = LibrarySource::User},
        LibraryGroup{.label = "Other builds", .source = LibrarySource::OtherBuild}};

    std::vector<LibraryGroup> groups;
    for (const LibraryGroup& shape : order) {
        LibraryGroup group = shape;
        for (const LibraryEntry& entry : entries) {
            if (entry.source != shape.source || !Matches(entry, needle)) continue;
            group.entries.push_back(entry);
        }
        if (!group.entries.empty()) groups.push_back(group);
    }
    return groups;
}

std::string Slug(std::string_view name) {
    std::string out;
    for (const char c : name) {
        const auto raw = (unsigned char)c;
        if (std::isalnum(raw) != 0) {
            out.push_back((char)std::tolower(raw));
            continue;
        }
        if (!out.empty() && out.back() != '-') out.push_back('-');
    }
    while (!out.empty() && out.back() == '-')
        out.pop_back();
    if (out.empty()) return std::string(kFallbackId);
    return out;
}

std::string UniqueId(std::string_view wanted, const std::vector<std::string>& taken) {
    std::string base(wanted);
    if (base.empty()) base = std::string(kFallbackId);
    if (!Taken(taken, base)) return base;
    for (int suffix = 2; suffix < 1000; suffix++) {
        std::string candidate = base + "-" + std::to_string(suffix);
        if (!Taken(taken, candidate)) return candidate;
    }
    return base;
}

std::string CopyId(std::string_view id, const std::vector<std::string>& taken) {
    return UniqueId(std::string(id) + "-copy", taken);
}

std::string UserRelativePath(std::string_view build, std::string_view id) {
    return std::string(build) + "/" + std::string(id) + ".json";
}

}
