#include "formats/afp_script_names.h"

#include "formats/afp_script_names_data.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace AfpScript {

namespace {

struct Entry {
    std::string_view name;
    uint16_t id;
};

std::vector<Entry> Parsed() {
    std::vector<Entry> out;
    for (const Detail::NameRun& run : Detail::kNameRuns) {
        uint16_t id = run.first;
        std::size_t at = 0;
        while (at <= run.names.size()) {
            const std::size_t space = run.names.find(' ', at);
            const std::size_t end = space == std::string_view::npos ? run.names.size() : space;
            out.push_back(Entry{.name = run.names.substr(at, end - at), .id = id});
            id++;
            at = end + 1;
        }
    }
    return out;
}

const std::vector<Entry>& ById() {
    static const std::vector<Entry> all = Parsed();
    return all;
}

const std::vector<Entry>& ByName() {
    static const std::vector<Entry> all = [] {
        std::vector<Entry> sorted = ById();
        std::ranges::sort(sorted, {}, &Entry::name);
        return sorted;
    }();
    return all;
}

}

std::optional<std::string_view> BuiltinName(uint16_t id) {
    const std::vector<Entry>& all = ById();
    const auto found = std::ranges::lower_bound(all, id, {}, &Entry::id);
    if (found == all.end() || found->id != id) return std::nullopt;
    return found->name;
}

std::optional<uint16_t> BuiltinNamed(std::string_view name) {
    const std::vector<Entry>& all = ByName();
    const auto found = std::ranges::lower_bound(all, name, {}, &Entry::name);
    if (found == all.end() || found->name != name) return std::nullopt;
    return found->id;
}

std::span<const std::string_view> BuiltinNames() {
    static const std::vector<std::string_view> all = [] {
        std::vector<std::string_view> names;
        names.reserve(ByName().size());
        for (const Entry& one : ByName())
            names.push_back(one.name);
        return names;
    }();
    return all;
}

}
