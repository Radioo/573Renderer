#include "formats/ifs_names.h"

#include "support/expected.h"

#include <md5.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace Ifs {

namespace {

constexpr std::string_view kEscapedCharacters = " $+-.:@~";
constexpr std::string_view kEscapeLetters = "ABCDEFGH";
constexpr std::string_view kListableAfterUnderscore = "ABCDEFGH_0123456789";

bool IsAsciiLetter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

}

Support::Expected<std::string, std::string> EscapeName(std::string_view component) {
    if (component.empty()) return Support::Unexpected(std::string("empty IFS name"));
    std::string out;
    out.reserve(component.size() + 4);
    if (IsDigit(component.front())) out.push_back('_');
    for (const char c : component) {
        if (IsAsciiLetter(c) || IsDigit(c)) {
            out.push_back(c);
        } else if (c == '_') {
            out.append("__");
        } else if (const std::size_t index = kEscapedCharacters.find(c);
                   index != std::string_view::npos) {
            out.push_back('_');
            out.push_back(kEscapeLetters[index]);
        } else {
            return Support::Unexpected("character not allowed in an IFS name: " +
                                       std::string(component));
        }
    }
    return out;
}

Support::Expected<std::string, std::string> UnescapeName(std::string_view node_name) {
    if (node_name.empty()) return Support::Unexpected(std::string("empty IFS name"));
    std::string out;
    out.reserve(node_name.size());
    std::size_t i = 0;
    if (node_name.front() == '_' && node_name.size() > 1 && IsDigit(node_name[1])) i = 1;
    while (i < node_name.size()) {
        const char c = node_name[i];
        if (IsAsciiLetter(c) || IsDigit(c)) {
            out.push_back(c);
            i++;
            continue;
        }
        const std::string bad = "not an escaped IFS name: " + std::string(node_name);
        if (c != '_' || i + 1 >= node_name.size()) return Support::Unexpected(bad);
        const char next = node_name[i + 1];
        if (next == '_') {
            out.push_back('_');
        } else if (const std::size_t index = kEscapeLetters.find(next);
                   index != std::string_view::npos) {
            out.push_back(kEscapedCharacters[index]);
        } else {
            return Support::Unexpected(bad);
        }
        i += 2;
    }
    return out;
}

std::string HashedName(std::string_view logical_name) {
    MD5 md5;
    return *EscapeName(md5(logical_name.data(), logical_name.size()));
}

bool IsSpecialName(std::string_view name) {
    return name.size() >= 2 && name[0] == '_' &&
           kListableAfterUnderscore.find(name[1]) == std::string_view::npos;
}

}
