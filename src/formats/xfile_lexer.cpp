#include "formats/xfile_lexer.h"

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>

namespace XFile {

namespace {

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsTokenChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-' || c == '.' || c == '+';
}

}

void Lexer::SkipSpace() {
    while (at_ < t_.size()) {
        const char c = t_[at_];
        if (IsSpace(c)) {
            at_++;
            continue;
        }
        if (c == '#' || (c == '/' && at_ + 1 < t_.size() && t_[at_ + 1] == '/')) {
            while (at_ < t_.size() && t_[at_] != '\n')
                at_++;
            continue;
        }
        break;
    }
}

char Lexer::Peek() {
    SkipSpace();
    return at_ < t_.size() ? t_[at_] : '\0';
}

std::string Lexer::Token() {
    SkipSpace();
    const size_t b = at_;
    while (at_ < t_.size() && IsTokenChar(t_[at_]))
        at_++;
    if (b == at_ && at_ < t_.size()) at_++;
    return std::string(t_.substr(b, at_ - b));
}

std::string Lexer::QuotedString() {
    SkipSpace();
    if (at_ >= t_.size() || t_[at_] != '"') return {};
    at_++;
    const size_t b = at_;
    while (at_ < t_.size() && t_[at_] != '"')
        at_++;
    const std::string s(t_.substr(b, at_ - b));
    if (at_ < t_.size()) at_++;
    return s;
}

std::string Lexer::ScanNumeric() {
    SkipSpace();
    const size_t b = at_;
    if (at_ < t_.size() && (t_[at_] == '-' || t_[at_] == '+')) at_++;
    while (at_ < t_.size() && t_[at_] >= '0' && t_[at_] <= '9')
        at_++;
    if (at_ < t_.size() && t_[at_] == '.') {
        at_++;
        while (at_ < t_.size() && t_[at_] >= '0' && t_[at_] <= '9')
            at_++;
    }
    if (at_ < t_.size() && (t_[at_] == 'e' || t_[at_] == 'E')) {
        at_++;
        if (at_ < t_.size() && (t_[at_] == '-' || t_[at_] == '+')) at_++;
        while (at_ < t_.size() && t_[at_] >= '0' && t_[at_] <= '9')
            at_++;
    }
    return std::string(t_.substr(b, at_ - b));
}

float Lexer::Number() {
    const std::string tok = ScanNumeric();
    SkipSeparators();
    if (tok.empty()) return 0.0F;
    try {
        return std::stof(tok);
    } catch (const std::exception&) {
        return 0.0F;
    }
}

int Lexer::Integer() {
    const std::string tok = ScanNumeric();
    SkipSeparators();
    if (tok.empty()) return 0;
    try {
        return (int)std::stol(tok);
    } catch (const std::exception&) {
        return 0;
    }
}

void Lexer::SkipSeparators() {
    while (at_ < t_.size()) {
        const char c = t_[at_];
        if (IsSpace(c) || c == ',' || c == ';') {
            at_++;
            continue;
        }
        break;
    }
}

bool Lexer::Expect(char c) {
    SkipSpace();
    if (at_ < t_.size() && t_[at_] == c) {
        at_++;
        return true;
    }
    return false;
}

void Lexer::SkipBlock() {
    int depth = 0;
    while (at_ < t_.size()) {
        const char c = t_[at_];
        if (c == '"') {
            at_++;
            while (at_ < t_.size() && t_[at_] != '"')
                at_++;
            if (at_ < t_.size()) at_++;
            continue;
        }
        at_++;
        if (c == '{') depth++;
        if (c == '}') {
            depth--;
            if (depth <= 0) return;
        }
    }
}

}
