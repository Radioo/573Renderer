#include "formats/xfile_binary.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace XFile {

namespace {

constexpr size_t kHeaderBytes = 16;

constexpr uint16_t kTokName = 1;
constexpr uint16_t kTokString = 2;
constexpr uint16_t kTokInteger = 3;
constexpr uint16_t kTokGuid = 5;
constexpr uint16_t kTokIntegerList = 6;
constexpr uint16_t kTokFloatList = 7;
constexpr uint16_t kTokOBrace = 10;
constexpr uint16_t kTokCBrace = 11;
constexpr uint16_t kTokOParen = 12;
constexpr uint16_t kTokCParen = 13;
constexpr uint16_t kTokOBracket = 14;
constexpr uint16_t kTokCBracket = 15;
constexpr uint16_t kTokOAngle = 16;
constexpr uint16_t kTokCAngle = 17;
constexpr uint16_t kTokDot = 18;
constexpr uint16_t kTokComma = 19;
constexpr uint16_t kTokSemicolon = 20;
constexpr uint16_t kTokTemplate = 31;
constexpr uint16_t kTokWord = 40;
constexpr uint16_t kTokDword = 41;
constexpr uint16_t kTokFloat = 42;
constexpr uint16_t kTokDouble = 43;
constexpr uint16_t kTokChar = 44;
constexpr uint16_t kTokUchar = 45;
constexpr uint16_t kTokSword = 46;
constexpr uint16_t kTokSdword = 47;
constexpr uint16_t kTokVoid = 48;
constexpr uint16_t kTokLpstr = 49;
constexpr uint16_t kTokUnicode = 50;
constexpr uint16_t kTokCstring = 51;
constexpr uint16_t kTokArray = 52;

std::string_view Keyword(uint16_t token) {
    switch (token) {
    case kTokTemplate:
        return "template";
    case kTokWord:
        return "WORD";
    case kTokDword:
        return "DWORD";
    case kTokFloat:
        return "FLOAT";
    case kTokDouble:
        return "DOUBLE";
    case kTokChar:
        return "CHAR";
    case kTokUchar:
        return "UCHAR";
    case kTokSword:
        return "SWORD";
    case kTokSdword:
        return "SDWORD";
    case kTokVoid:
        return "VOID";
    case kTokLpstr:
        return "STRING";
    case kTokUnicode:
        return "UNICODE";
    case kTokCstring:
        return "CSTRING";
    case kTokArray:
        return "array";
    default:
        return {};
    }
}

std::string_view Punctuation(uint16_t token) {
    switch (token) {
    case kTokOBrace:
        return " {\n";
    case kTokCBrace:
        return "}\n";
    case kTokOParen:
        return "(";
    case kTokCParen:
        return ")";
    case kTokOBracket:
        return "[";
    case kTokCBracket:
        return "]";
    case kTokOAngle:
        return "<";
    case kTokCAngle:
        return ">";
    case kTokDot:
        return ".";
    case kTokComma:
        return ",";
    case kTokSemicolon:
        return ";";
    default:
        return {};
    }
}

class Reader {
public:
    Reader(std::string_view data, size_t at) : d_(data), at_(at) {}

    [[nodiscard]] bool Done() const { return at_ >= d_.size(); }
    [[nodiscard]] size_t Pos() const { return at_; }

    bool Take(size_t n, std::string_view& out) {
        if (at_ + n > d_.size()) return false;
        out = d_.substr(at_, n);
        at_ += n;
        return true;
    }

    bool U16(uint16_t& out) {
        std::string_view b;
        if (!Take(2, b)) return false;
        out = (uint16_t)(Byte(b, 0) | (Byte(b, 1) << 8U));
        return true;
    }

    bool U32(uint32_t& out) {
        std::string_view b;
        if (!Take(4, b)) return false;
        out = Byte(b, 0) | (Byte(b, 1) << 8U) | (Byte(b, 2) << 16U) | (Byte(b, 3) << 24U);
        return true;
    }

    static uint32_t Byte(std::string_view b, size_t at) { return (uint32_t)(uint8_t)b[at]; }

private:
    std::string_view d_;
    size_t at_ = 0;
};

void AppendEscaped(std::string& out, std::string_view bytes) {
    out.push_back('"');
    for (const char c : bytes) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
}

bool AppendFloats(Reader& reader, size_t float_bytes, std::string& out, std::string& err) {
    uint32_t count = 0;
    if (!reader.U32(count)) {
        err = "truncated float list header";
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        std::string_view raw;
        if (!reader.Take(float_bytes, raw)) {
            err = "truncated float list body";
            return false;
        }
        double value = 0.0;
        if (float_bytes == 4) {
            uint32_t bits = 0;
            for (size_t k = 0; k < 4; k++)
                bits |= Reader::Byte(raw, k) << (8U * k);
            value = (double)std::bit_cast<float>(bits);
        } else {
            uint64_t bits = 0;
            for (size_t k = 0; k < 8; k++)
                bits |= (uint64_t)Reader::Byte(raw, k) << (8U * k);
            value = std::bit_cast<double>(bits);
        }
        out += std::format("{};", value);
    }
    out.push_back('\n');
    return true;
}

bool AppendName(Reader& reader, std::string& out, std::string& err) {
    uint32_t len = 0;
    std::string_view name;
    if (!reader.U32(len) || !reader.Take(len, name)) {
        err = "truncated name token";
        return false;
    }
    out.push_back(' ');
    out += name;
    out.push_back(' ');
    return true;
}

bool AppendString(Reader& reader, std::string& out, std::string& err) {
    uint32_t len = 0;
    std::string_view str;
    uint16_t terminator = 0;
    if (!reader.U32(len) || !reader.Take(len, str) || !reader.U16(terminator)) {
        err = "truncated string token";
        return false;
    }
    AppendEscaped(out, str);
    out += (terminator == kTokComma) ? "," : ";";
    return true;
}

bool AppendIntegers(Reader& reader, uint32_t count, std::string& out, std::string& err) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t value = 0;
        if (!reader.U32(value)) {
            err = "truncated integer list";
            return false;
        }
        out += std::format("{};", value);
    }
    out.push_back('\n');
    return true;
}

bool AppendDataToken(Reader& reader, uint16_t token, size_t float_bytes, std::string& out,
                     std::string& err) {
    switch (token) {
    case kTokName:
        return AppendName(reader, out, err);
    case kTokString:
        return AppendString(reader, out, err);
    case kTokInteger: {
        uint32_t value = 0;
        if (!reader.U32(value)) {
            err = "truncated integer token";
            return false;
        }
        out += std::format("{};", value);
        return true;
    }
    case kTokGuid: {
        std::string_view guid;
        if (!reader.Take(16, guid)) {
            err = "truncated guid token";
            return false;
        }
        return true;
    }
    case kTokIntegerList: {
        uint32_t count = 0;
        if (!reader.U32(count)) {
            err = "truncated integer list header";
            return false;
        }
        return AppendIntegers(reader, count, out, err);
    }
    case kTokFloatList:
        return AppendFloats(reader, float_bytes, out, err);
    default:
        return false;
    }
}

bool IsDataToken(uint16_t token) {
    return token == kTokName || token == kTokString || token == kTokInteger || token == kTokGuid ||
           token == kTokIntegerList || token == kTokFloatList;
}

}

bool BinaryToText(std::string_view data, std::string& out, std::string& err) {
    if (data.size() < kHeaderBytes) {
        err = ".x file is shorter than its 16-byte header";
        return false;
    }
    const size_t float_bytes = (data[12] == '0' && data[13] == '0' && data[14] == '6') ? 8 : 4;

    out.clear();
    out.reserve(data.size() * 4);
    out += "xof 0303txt 0032\n";

    Reader reader(data, kHeaderBytes);
    while (!reader.Done()) {
        uint16_t token = 0;
        if (!reader.U16(token)) break;

        if (IsDataToken(token)) {
            if (!AppendDataToken(reader, token, float_bytes, out, err)) return false;
            continue;
        }
        const std::string_view punctuation = Punctuation(token);
        if (!punctuation.empty()) {
            out += punctuation;
            continue;
        }
        const std::string_view keyword = Keyword(token);
        if (keyword.empty()) {
            err = "unknown binary .x token " + std::to_string(token) + " at offset " +
                  std::to_string(reader.Pos() - 2);
            return false;
        }
        out.push_back(' ');
        out += keyword;
        out.push_back(' ');
    }
    return true;
}

}
