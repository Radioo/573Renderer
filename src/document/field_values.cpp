#include "document/field_values.h"

#include "support/expected.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Document {

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) {
        if (!out.empty()) out += ", ";
        out += part;
    }
    return out;
}

Support::Expected<std::vector<int64_t>, std::string> Numbers(std::string_view value,
                                                             std::size_t count) {
    std::vector<int64_t> out;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        std::string_view part = value.substr(
            start, comma == std::string_view::npos ? value.size() - start : comma - start);
        while (!part.empty() && part.front() == ' ')
            part.remove_prefix(1);
        while (!part.empty() && part.back() == ' ')
            part.remove_suffix(1);
        int64_t number = 0;
        const auto* end = part.data() + part.size();
        const auto parsed = std::from_chars(part.data(), end, number);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            return Support::Unexpected("not a number: " + std::string(part));
        out.push_back(number);
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    if (out.size() != count) {
        return Support::Unexpected("expected " + std::to_string(count) + " numbers, got " +
                                   std::to_string(out.size()));
    }
    return out;
}

}
