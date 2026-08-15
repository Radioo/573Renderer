#include "preset_golden_format.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace PresetGolden {

namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr std::size_t kHexDigits = 16;

std::string HexOf(std::uint64_t value) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string out(kHexDigits, '0');
    for (std::size_t i = kHexDigits; i > 0; i--) {
        out[i - 1] = kDigits[value & 0xFU];
        value >>= 4U;
    }
    return out;
}

std::vector<std::string> ReadStrings(const nlohmann::json& parent, const std::string& key) {
    std::vector<std::string> out;
    if (!parent.contains(key) || !parent[key].is_array()) return out;
    const nlohmann::json& node = parent[key];
    out.reserve(node.size());
    for (const auto& item : node) {
        if (item.is_string()) out.push_back(item.get<std::string>());
    }
    return out;
}

std::vector<std::string> ReadStringArray(const nlohmann::json& node) {
    std::vector<std::string> out;
    if (!node.is_array()) return out;
    out.reserve(node.size());
    for (const auto& item : node) {
        if (item.is_string()) out.push_back(item.get<std::string>());
    }
    return out;
}

}

std::string FormatFloat(float value) {
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ec != std::errc()) return "?";
    return {buffer.data(), result.ptr};
}

std::string FormatCall(const std::string& name, const std::vector<std::string>& args) {
    std::string out = name;
    out += "(";
    for (std::size_t i = 0; i < args.size(); i++) {
        if (i > 0) out += ", ";
        out += args[i];
    }
    out += ")";
    return out;
}

std::vector<std::string> WithoutModelTransforms(const std::vector<std::string>& calls) {
    constexpr std::string_view kTransformCall = "Scene3dHost::SetModelTransform(";
    std::vector<std::string> out;
    out.reserve(calls.size());
    for (const std::string& call : calls) {
        if (call.starts_with(kTransformCall)) continue;
        out.push_back(call);
    }
    return out;
}

std::string HashFrame(const std::vector<std::string>& calls) {
    std::uint64_t hash = kFnvOffset;
    for (const std::string& call : calls) {
        for (const char ch : call) {
            hash ^= (std::uint64_t)(unsigned char)ch;
            hash *= kFnvPrime;
        }
        hash ^= (std::uint64_t)'\n';
        hash *= kFnvPrime;
    }
    return HexOf(hash);
}

std::string FixtureName(const std::string& preset, int choice) {
    if (choice < 0) return preset + ".json";
    return preset + "." + std::to_string(choice) + ".json";
}

bool Parse(const std::string& text, Recording& out, std::string& err) {
    const nlohmann::json doc = nlohmann::json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        err = "not a JSON object";
        return false;
    }
    if (!doc.contains("preset") || !doc.contains("frames") || !doc.contains("hashes")) {
        err = "missing preset, frames or hashes";
        return false;
    }
    out.preset = doc["preset"].get<std::string>();
    out.build = doc.value("build", std::string());
    out.choice =
        (!doc.contains("choice") || doc["choice"].is_null()) ? -1 : doc["choice"].get<int>();
    out.frames = doc["frames"].get<int>();
    out.setup = ReadStrings(doc, "setup");
    out.hashes = ReadStrings(doc, "hashes");
    out.hashes_no_transform = ReadStrings(doc, "hashes_no_transform");
    out.detail.clear();
    if (doc.contains("detail") && doc["detail"].is_object()) {
        for (const auto& [key, value] : doc["detail"].items())
            out.detail[std::stoi(key)] = ReadStringArray(value);
    }
    return true;
}

}
