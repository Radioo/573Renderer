#include "formats/ifs_names.h"

#include "support/expected.h"

#include <md5.h>

#include <string>
#include <string_view>

namespace Ifs {

namespace {

constexpr std::string_view kEscapedCharacters = " $+-.:@~";
constexpr std::string_view kEscapeLetters = "ABCDEFGH";

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

std::string HashedName(std::string_view logical_name) {
    MD5 md5;
    return *EscapeName(md5(logical_name.data(), logical_name.size()));
}

}
