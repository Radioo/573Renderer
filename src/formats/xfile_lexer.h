#pragma once

#include <string>
#include <string_view>

namespace XFile {

class Lexer {
public:
    explicit Lexer(std::string_view text) : t_(text) {}

    [[nodiscard]] bool Eof() const { return at_ >= t_.size(); }

    void SkipSpace();

    [[nodiscard]] char Peek();

    std::string Token();

    std::string QuotedString();

    float Number();

    int Integer();

    void SkipSeparators();

    std::string ScanNumeric();

    bool Expect(char c);

    void SkipBlock();

    [[nodiscard]] size_t Pos() const { return at_; }

private:
    std::string_view t_;
    size_t at_ = 0;
};

}
