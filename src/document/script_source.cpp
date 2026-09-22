#include "document/script_source.h"

#include "document/animation_strings.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"
#include "formats/afp_script_names.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr uint16_t kLibraryId = 0x390;
constexpr std::string_view kThis = "this";
constexpr std::string_view kKeep = "keep";
constexpr std::string_view kLet = "let";
constexpr std::string_view kSetMark = " = ";
constexpr std::string_view kBuiltinLead = "builtin_0x";
constexpr std::string_view kItemLead = "item(";
constexpr std::string_view kStoreLead = "store ";
constexpr int kHexBase = 16;
constexpr int kNibble = 4;

bool SameItem(const AfpScript::Item& one, const AfpScript::Item& other) {
    return one.type == other.type && one.operand == other.operand;
}

struct Named {
    std::string_view word;
    uint8_t opcode;
};

constexpr std::array<Named, 6> kBareWords{{
    {.word = "get_variable", .opcode = AfpScript::Op::kGetVariable},
    {.word = "call_method", .opcode = AfpScript::Op::kCallMethod},
    {.word = "call_function", .opcode = AfpScript::Op::kCallFunction},
    {.word = "set_member", .opcode = AfpScript::Op::kSetMember},
    {.word = "pop", .opcode = AfpScript::Op::kPop},
    {.word = "end", .opcode = AfpScript::Op::kEnd},
}};

constexpr uint8_t kJumpCarriesBias = 0x2;

constexpr std::string_view kPushLead = "push(";
constexpr std::string_view kJumpLead = "goto_frame2(";

constexpr std::string_view Heading(std::string_view lead) {
    return lead.substr(0, lead.size() - 1);
}

constexpr auto kInstructions = [] {
    std::array<std::string_view, kBareWords.size() + 3> out{};
    for (std::size_t i = 0; i < kBareWords.size(); i++)
        out[i] = kBareWords[i].word;
    out[kBareWords.size()] = Heading(kPushLead);
    out[kBareWords.size() + 1] = Heading(kJumpLead);
    out[kBareWords.size() + 2] = Heading(kStoreLead);
    return out;
}();

constexpr std::array<std::string_view, 4> kWords{
    {kItemLead.substr(0, kItemLead.size() - 1), kKeep, kLet, kThis}};

std::string_view Trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);
    return text;
}

std::string RawBuiltinName(uint16_t id) {
    std::string digits;
    for (int shift = 12; shift >= 0; shift -= kNibble) {
        const auto nibble = static_cast<uint8_t>((id >> shift) & 0xF);
        if (digits.empty() && nibble == 0 && shift > 0) continue;
        digits += "0123456789abcdef"[nibble];
    }
    return std::string(kBuiltinLead) + digits;
}

std::string HexBytes(const std::vector<uint8_t>& bytes) {
    std::string out;
    for (const uint8_t byte : bytes) {
        out += "0123456789abcdef"[byte >> kNibble];
        out += "0123456789abcdef"[byte & 0xF];
    }
    return out;
}

std::optional<std::vector<uint8_t>> BytesFrom(std::string_view text) {
    if (text.size() % 2 != 0) return std::nullopt;
    std::vector<uint8_t> out;
    out.reserve(text.size() / 2);
    for (std::size_t at = 0; at < text.size(); at += 2) {
        uint8_t byte = 0;
        const auto* end = text.data() + at + 2;
        const auto read = std::from_chars(text.data() + at, end, byte, kHexBase);
        if (read.ec != std::errc{} || read.ptr != end) return std::nullopt;
        out.push_back(byte);
    }
    return out;
}

std::optional<uint16_t> IdAfter(std::string_view text, std::string_view lead) {
    if (!text.starts_with(lead)) return std::nullopt;
    const std::string_view digits = text.substr(lead.size());
    uint16_t value = 0;
    const auto* end = digits.data() + digits.size();
    const auto read = std::from_chars(digits.data(), end, value, kHexBase);
    if (read.ec != std::errc{} || read.ptr != end) return std::nullopt;
    return value;
}

std::optional<int32_t> WholeNumber(std::string_view text) {
    int32_t value = 0;
    const auto* end = text.data() + text.size();
    const auto read = std::from_chars(text.data(), end, value);
    if (read.ec != std::errc{} || read.ptr != end) return std::nullopt;
    return value;
}

std::optional<uint8_t> RegisterNumber(std::string_view text) {
    if (text.size() < 2 || text.front() != 'r') return std::nullopt;
    uint8_t value = 0;
    const auto* end = text.data() + text.size();
    const auto read = std::from_chars(text.data() + 1, end, value);
    if (read.ec != std::errc{} || read.ptr != end) return std::nullopt;
    return value;
}

std::optional<uint16_t> StringSlot(AfpAnimation::Animation& animation,
                                   AfpAnimation::Bytecode& bytecode, std::string_view text) {
    if (!bytecode.strings) bytecode.strings = std::vector<AfpAnimation::StringId>{};
    const AfpAnimation::StringId id = InternString(animation, text);
    const auto found = std::ranges::find(*bytecode.strings, id);
    if (found != bytecode.strings->end())
        return static_cast<uint16_t>(found - bytecode.strings->begin());
    if (bytecode.strings->size() > 0xFFFE) return std::nullopt;
    bytecode.strings->push_back(id);
    return static_cast<uint16_t>(bytecode.strings->size() - 1);
}

AfpScript::Item StringItem(uint16_t slot) {
    if (slot <= 0xFF) {
        return AfpScript::Item{.type = AfpScript::PushType::kShortString,
                               .operand = {static_cast<uint8_t>(slot)}};
    }
    return AfpScript::Item{
        .type = AfpScript::PushType::kLongString,
        .operand = {static_cast<uint8_t>(slot >> 8U), static_cast<uint8_t>(slot & 0xFFU)}};
}

bool PlainName(std::string_view name) {
    if (name.empty() || (name.front() >= '0' && name.front() <= '9')) return false;
    return std::ranges::all_of(name, [](const char letter) {
        return letter == '_' || (letter >= '0' && letter <= '9') ||
               (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
    });
}

bool Reserved(std::string_view name) {
    if (RegisterNumber(name)) return true;
    if (name.starts_with(kBuiltinLead)) return true;
    if (std::ranges::find(kInstructions, name) != kInstructions.end()) return true;
    return std::ranges::find(kWords, name) != kWords.end();
}

bool UsableName(std::string_view name) {
    return PlainName(name) && !Reserved(name);
}

std::string CallName(uint16_t id) {
    const std::optional<std::string_view> name = AfpScript::BuiltinName(id);
    if (name && UsableName(*name)) return std::string(*name);
    return RawBuiltinName(id);
}

std::optional<uint16_t> CallId(std::string_view name) {
    if (UsableName(name)) return AfpScript::BuiltinNamed(name);
    return IdAfter(name, kBuiltinLead);
}

std::string RawItemText(const AfpScript::Item& item) {
    return std::string(kItemLead) + std::to_string(item.type) + ", " + HexBytes(item.operand) + ")";
}

std::optional<std::string> ItemText(const AfpAnimation::Animation& animation,
                                    const AfpAnimation::Bytecode& bytecode,
                                    const AfpScript::Item& item) {
    if (item.type == AfpScript::PushType::kStoredObject && item.operand.empty())
        return std::string(kThis);
    if (item.type == AfpScript::PushType::kRegister && item.operand.size() == 1)
        return "r" + std::to_string(item.operand.front());
    if (AfpScript::ItemIsString(item)) {
        const uint16_t index = AfpScript::StringIndex(item);
        if (!bytecode.strings || index >= bytecode.strings->size()) return std::nullopt;
        if (!SameItem(StringItem(index), item)) return std::nullopt;
        const std::string text = StringText(animation, (*bytecode.strings)[index]);
        if (text.find('"') != std::string::npos || text.find('\n') != std::string::npos)
            return std::nullopt;
        return "\"" + text + "\"";
    }
    if (const auto number = AfpScript::ItemNumber(item)) {
        if (SameItem(AfpScript::NumberItem(*number), item)) return std::to_string(*number);
    }
    if (const std::optional<uint16_t> id = AfpScript::BuiltinId(item)) {
        const std::optional<AfpScript::Item> same = AfpScript::BuiltinItem(*id);
        if (same && SameItem(*same, item)) return CallName(*id);
    }
    return RawItemText(item);
}

Support::Expected<AfpScript::Item, std::string> ItemFrom(AfpAnimation::Animation& animation,
                                                         AfpAnimation::Bytecode& bytecode,
                                                         std::string_view text) {
    if (text == kThis)
        return AfpScript::Item{.type = AfpScript::PushType::kStoredObject, .operand = {}};
    if (const std::optional<uint8_t> reg = RegisterNumber(text))
        return AfpScript::Item{.type = AfpScript::PushType::kRegister, .operand = {*reg}};
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        const std::optional<uint16_t> slot =
            StringSlot(animation, bytecode, text.substr(1, text.size() - 2));
        if (!slot) return Support::Unexpected(std::string("the script holds too many strings"));
        return StringItem(*slot);
    }
    if (const std::optional<uint16_t> id = CallId(text)) {
        const std::optional<AfpScript::Item> item = AfpScript::BuiltinItem(*id);
        if (!item)
            return Support::Unexpected(std::string(text) + " is not a builtin this build knows");
        return *item;
    }
    if (text.starts_with(kItemLead) && text.back() == ')') {
        const std::string_view inside =
            text.substr(kItemLead.size(), text.size() - kItemLead.size() - 1);
        const std::size_t comma = inside.find(',');
        if (comma == std::string_view::npos)
            return Support::Unexpected("a raw item is item(type, bytes), not " + std::string(text));
        const std::optional<int32_t> type = WholeNumber(Trimmed(inside.substr(0, comma)));
        const std::optional<std::vector<uint8_t>> bytes =
            BytesFrom(Trimmed(inside.substr(comma + 1)));
        if (!type || *type < 0 || *type > 0xFF || !bytes)
            return Support::Unexpected("a raw item is item(type, bytes), not " + std::string(text));
        return AfpScript::Item{.type = static_cast<uint8_t>(*type), .operand = *bytes};
    }
    if (const std::optional<int32_t> number = WholeNumber(text))
        return AfpScript::NumberItem(*number);
    if (text.find('"') != std::string_view::npos) {
        return Support::Unexpected("a piece of text needs quotes around all of it: " +
                                   std::string(text));
    }
    return Support::Unexpected(
        "an argument is a number, quoted text, this, a register or item(...), not " +
        std::string(text));
}

std::vector<std::string_view> SplitArguments(std::string_view inside) {
    std::vector<std::string_view> out;
    if (Trimmed(inside).empty()) return out;
    std::size_t start = 0;
    bool quoted = false;
    int depth = 0;
    for (std::size_t at = 0; at <= inside.size(); at++) {
        if (at < inside.size()) {
            if (inside[at] == '"') quoted = !quoted;
            if (!quoted && inside[at] == '(') depth++;
            if (!quoted && inside[at] == ')') depth--;
            if (inside[at] != ',' || quoted || depth > 0) continue;
        }
        out.push_back(Trimmed(inside.substr(start, at - start)));
        start = at + 1;
    }
    return out;
}

AfpScript::Instruction Bare(uint8_t opcode) {
    return AfpScript::Instruction{.opcode = opcode,
                                  .items = {},
                                  .registers = {},
                                  .flags = 0,
                                  .frame_bias = 0,
                                  .has_frame_bias = false};
}

struct Match {
    std::string line;
    std::size_t length = 0;
};

std::optional<Match> LibraryCallAt(const AfpAnimation::Animation& animation,
                                   const AfpAnimation::Bytecode& bytecode,
                                   const std::vector<AfpScript::Instruction>& code,
                                   std::size_t at) {
    if (at + 3 >= code.size()) return std::nullopt;
    if (code[at].opcode != AfpScript::Op::kPush) return std::nullopt;
    if (code[at + 1].opcode != AfpScript::Op::kGetVariable) return std::nullopt;
    if (code[at + 2].opcode != AfpScript::Op::kPush) return std::nullopt;
    if (code[at + 3].opcode != AfpScript::Op::kCallMethod) return std::nullopt;

    const std::vector<AfpScript::Item>& pushed = code[at].items;
    if (pushed.size() < 2 || code[at + 2].items.size() != 1) return std::nullopt;
    const std::optional<uint16_t> object = AfpScript::BuiltinId(pushed.back());
    const std::optional<uint16_t> method = AfpScript::BuiltinId(code[at + 2].items.front());
    if (!object || *object != kLibraryId || !method) return std::nullopt;
    const std::optional<AfpScript::Item> library = AfpScript::BuiltinItem(kLibraryId);
    const std::optional<AfpScript::Item> named = AfpScript::BuiltinItem(*method);
    if (!library || !SameItem(*library, pushed.back())) return std::nullopt;
    if (!named || !SameItem(*named, code[at + 2].items.front())) return std::nullopt;

    const auto count = AfpScript::ItemNumber(pushed[pushed.size() - 2]);
    if (!count || *count < 0 || static_cast<std::size_t>(*count) != pushed.size() - 2)
        return std::nullopt;
    if (!SameItem(AfpScript::NumberItem(*count), pushed[pushed.size() - 2])) return std::nullopt;

    const bool pops = at + 4 < code.size() && code[at + 4].opcode == AfpScript::Op::kPop;
    std::string line = pops ? std::string() : std::string(kKeep) + " ";
    line += CallName(*method) + "(";
    const auto arguments = static_cast<std::size_t>(*count);
    for (std::size_t i = arguments; i > 0; i--) {
        if (i != arguments) line += ", ";
        const std::optional<std::string> text = ItemText(animation, bytecode, pushed[i - 1]);
        if (!text) return std::nullopt;
        line += *text;
    }
    return Match{.line = line + ")", .length = pops ? std::size_t{5} : std::size_t{4}};
}

std::optional<std::string> BuiltinText(const AfpScript::Item& item) {
    const std::optional<uint16_t> id = AfpScript::BuiltinId(item);
    if (!id) return std::nullopt;
    const std::optional<AfpScript::Item> same = AfpScript::BuiltinItem(*id);
    if (!same || !SameItem(*same, item)) return std::nullopt;
    return CallName(*id);
}

std::optional<std::size_t> CountAt(const std::vector<AfpScript::Item>& items, std::size_t at) {
    const auto number = AfpScript::ItemNumber(items[at]);
    if (!number || *number < 0) return std::nullopt;
    const auto count = static_cast<std::size_t>(*number);
    if (count > at) return std::nullopt;
    if (!SameItem(AfpScript::NumberItem(*number), items[at])) return std::nullopt;
    return count;
}

std::optional<std::string> ArgumentText(const AfpAnimation::Animation& animation,
                                        const AfpAnimation::Bytecode& bytecode,
                                        const std::vector<AfpScript::Item>& items,
                                        std::size_t below, std::size_t count) {
    std::string out = "(";
    for (std::size_t i = 0; i < count; i++) {
        if (i != 0) out += ", ";
        const std::optional<std::string> text = ItemText(animation, bytecode, items[below - 1 - i]);
        if (!text) return std::nullopt;
        out += *text;
    }
    return out + ")";
}

std::optional<Match> MemberWriteAt(const AfpAnimation::Animation& animation,
                                   const AfpAnimation::Bytecode& bytecode,
                                   const std::vector<AfpScript::Instruction>& code,
                                   std::size_t at) {
    if (at + 1 >= code.size()) return std::nullopt;
    if (code[at].opcode != AfpScript::Op::kPush) return std::nullopt;
    if (code[at + 1].opcode != AfpScript::Op::kSetMember) return std::nullopt;
    const std::vector<AfpScript::Item>& items = code[at].items;
    if (items.size() != 3 || !AfpScript::ItemIsString(items[1])) return std::nullopt;

    const std::optional<std::string> object = ItemText(animation, bytecode, items[0]);
    if (!object || (*object != kThis && !RegisterNumber(*object))) return std::nullopt;
    const std::optional<std::string> quoted = ItemText(animation, bytecode, items[1]);
    if (!quoted || quoted->size() < 3 || quoted->front() != '"') return std::nullopt;
    const std::string member = quoted->substr(1, quoted->size() - 2);
    if (!PlainName(member)) return std::nullopt;
    const std::optional<std::string> value = ItemText(animation, bytecode, items[2]);
    if (!value) return std::nullopt;
    return Match{.line = *object + "." + member + " = " + *value, .length = 2};
}

std::optional<Match> MethodOnResult(const AfpAnimation::Animation& animation,
                                    const AfpAnimation::Bytecode& bytecode,
                                    const std::vector<AfpScript::Instruction>& code, std::size_t at,
                                    std::size_t left, std::string_view bound) {
    if (at + 4 >= code.size()) return std::nullopt;
    if (code[at + 3].opcode != AfpScript::Op::kPush || code[at + 3].items.size() != 1)
        return std::nullopt;
    if (code[at + 4].opcode != AfpScript::Op::kCallMethod) return std::nullopt;
    const std::vector<AfpScript::Item>& items = code[at].items;
    const std::optional<std::size_t> count = CountAt(items, left - 1);
    if (!count || left - 1 != *count) return std::nullopt;
    const std::optional<std::string> name = BuiltinText(code[at + 3].items.front());
    const std::optional<std::string> arguments =
        ArgumentText(animation, bytecode, items, left - 1, *count);
    if (!name || !arguments) return std::nullopt;

    const bool pops = at + 5 < code.size() && code[at + 5].opcode == AfpScript::Op::kPop;
    std::string line = pops ? std::string() : std::string(kKeep) + " ";
    line += std::string(bound) + "." + *name + *arguments;
    return Match{.line = line, .length = pops ? std::size_t{6} : std::size_t{5}};
}

std::optional<Match> RegisterChainAt(const AfpAnimation::Animation& animation,
                                     const AfpAnimation::Bytecode& bytecode,
                                     const std::vector<AfpScript::Instruction>& code,
                                     std::size_t at) {
    if (at + 2 >= code.size()) return std::nullopt;
    if (code[at].opcode != AfpScript::Op::kPush) return std::nullopt;
    if (code[at + 1].opcode != AfpScript::Op::kCallFunction) return std::nullopt;
    if (code[at + 2].opcode != AfpScript::Op::kStoreRegister) return std::nullopt;
    if (code[at + 2].registers.size() != 1) return std::nullopt;
    const std::vector<AfpScript::Item>& items = code[at].items;
    if (items.size() < 2) return std::nullopt;

    const std::optional<std::string> name = BuiltinText(items.back());
    const std::optional<std::size_t> count = CountAt(items, items.size() - 2);
    if (!name || !count) return std::nullopt;
    const std::size_t below = items.size() - 2;
    const std::optional<std::string> arguments =
        ArgumentText(animation, bytecode, items, below, *count);
    if (!arguments) return std::nullopt;

    const std::string bound = "r" + std::to_string(code[at + 2].registers.front());
    const std::string line = std::string(kLet) + " " + bound + " = " + *name + *arguments;
    const std::size_t left = below - *count;
    if (left == 0) return Match{.line = line, .length = 3};

    const std::optional<Match> method = MethodOnResult(animation, bytecode, code, at, left, bound);
    if (!method) return std::nullopt;
    return Match{.line = line + "\n" + method->line, .length = method->length};
}

std::optional<std::string> InstructionLine(const AfpAnimation::Animation& animation,
                                           const AfpAnimation::Bytecode& bytecode,
                                           const AfpScript::Instruction& one) {
    if (one.opcode == AfpScript::Op::kPush) {
        std::string line(kPushLead);
        for (std::size_t at = 0; at < one.items.size(); at++) {
            if (at != 0) line += ", ";
            const std::optional<std::string> text = ItemText(animation, bytecode, one.items[at]);
            if (!text) return std::nullopt;
            line += *text;
        }
        return line + ")";
    }
    if (one.opcode == AfpScript::Op::kStoreRegister) {
        std::string line(kStoreLead);
        for (std::size_t at = 0; at < one.registers.size(); at++) {
            if (at != 0) line += ", ";
            line += "r" + std::to_string(one.registers[at]);
        }
        return line;
    }
    if (one.opcode == AfpScript::Op::kGotoFrame2) {
        std::string line = std::string(kJumpLead) + std::to_string(one.flags);
        if (one.has_frame_bias) line += ", " + std::to_string(one.frame_bias);
        return line + ")";
    }
    const auto found = std::ranges::find(kBareWords, one.opcode, &Named::opcode);
    if (found == kBareWords.end()) return std::nullopt;
    return std::string(found->word);
}

Support::Expected<AfpScript::Instruction, std::string> ReadStore(std::string_view line) {
    AfpScript::Instruction store = Bare(AfpScript::Op::kStoreRegister);
    for (const std::string_view word : SplitArguments(line.substr(kStoreLead.size()))) {
        const std::optional<uint8_t> reg = RegisterNumber(word);
        if (!reg) return Support::Unexpected("a register is written r0, not " + std::string(word));
        store.registers.push_back(*reg);
    }
    return store;
}

Support::Expected<AfpScript::Instruction, std::string> ReadPush(AfpAnimation::Animation& animation,
                                                                AfpAnimation::Bytecode& bytecode,
                                                                std::string_view line) {
    AfpScript::Instruction push = Bare(AfpScript::Op::kPush);
    const std::string_view inside =
        line.substr(kPushLead.size(), line.size() - kPushLead.size() - 1);
    for (const std::string_view word : SplitArguments(inside)) {
        auto item = ItemFrom(animation, bytecode, word);
        if (!item) return Support::Unexpected(item.error());
        push.items.push_back(std::move(*item));
    }
    return push;
}

Support::Expected<AfpScript::Instruction, std::string> ReadBias(AfpScript::Instruction jump,
                                                                std::string_view written) {
    const std::optional<int32_t> bias = WholeNumber(written);
    if (!bias || *bias < 0 || *bias > 0xFFFF)
        return Support::Unexpected(std::string("a goto_frame2 bias is two bytes"));
    jump.frame_bias = static_cast<uint16_t>(*bias);
    jump.has_frame_bias = true;
    return jump;
}

Support::Expected<AfpScript::Instruction, std::string> ReadJump(std::string_view line) {
    const std::vector<std::string_view> parts =
        SplitArguments(line.substr(kJumpLead.size(), line.size() - kJumpLead.size() - 1));
    if (parts.empty() || parts.size() > 2)
        return Support::Unexpected(std::string("goto_frame2 takes flags and an optional bias"));
    const std::optional<int32_t> flags = WholeNumber(parts.front());
    if (!flags || *flags < 0 || *flags > 0xFF)
        return Support::Unexpected(std::string("goto_frame2 flags are one byte"));

    AfpScript::Instruction jump = Bare(AfpScript::Op::kGotoFrame2);
    jump.flags = static_cast<uint8_t>(*flags);
    const bool wants_bias = (jump.flags & kJumpCarriesBias) != 0;
    if (wants_bias != (parts.size() == 2)) {
        return Support::Unexpected(
            wants_bias ? std::string("goto_frame2 flags with bit 1 set carry a bias as well")
                       : std::string("goto_frame2 carries a bias only when bit 1 of its flags "
                                     "is set"));
    }
    if (!wants_bias) return jump;
    return ReadBias(std::move(jump), parts.back());
}

Support::Expected<void, std::string> ReadInstruction(AfpAnimation::Animation& animation,
                                                     AfpAnimation::Bytecode& bytecode,
                                                     std::string_view line,
                                                     std::vector<AfpScript::Instruction>& out) {
    const bool closed = line.back() == ')';
    auto keep = [&out](Support::Expected<AfpScript::Instruction, std::string> one)
        -> Support::Expected<void, std::string> {
        if (!one) return Support::Unexpected(one.error());
        out.push_back(std::move(*one));
        return {};
    };
    if (line.starts_with(kStoreLead)) return keep(ReadStore(line));
    if (closed && line.starts_with(kPushLead)) return keep(ReadPush(animation, bytecode, line));
    if (closed && line.starts_with(kJumpLead)) return keep(ReadJump(line));

    const auto found = std::ranges::find(kBareWords, line, &Named::word);
    if (found == kBareWords.end())
        return Support::Unexpected("this is not a call or an instruction: " + std::string(line));
    out.push_back(Bare(found->opcode));
    return {};
}

Support::Expected<void, std::string> ReadCall(AfpAnimation::Animation& animation,
                                              AfpAnimation::Bytecode& bytecode,
                                              std::string_view line,
                                              std::vector<AfpScript::Instruction>& out) {
    const bool kept =
        line.starts_with(kKeep) && line.size() > kKeep.size() && line[kKeep.size()] == ' ';
    const std::string_view called = kept ? Trimmed(line.substr(kKeep.size())) : line;
    const std::size_t open = called.find('(');
    const std::optional<uint16_t> id = open == std::string_view::npos || called.back() != ')'
                                           ? std::nullopt
                                           : CallId(Trimmed(called.substr(0, open)));
    if (!id) {
        if (kept)
            return Support::Unexpected("keep goes in front of a call, not " + std::string(called));
        return ReadInstruction(animation, bytecode, line, out);
    }
    const bool pops = !kept;
    line = called;

    const std::optional<AfpScript::Item> library = AfpScript::BuiltinItem(kLibraryId);
    const std::optional<AfpScript::Item> method = AfpScript::BuiltinItem(*id);
    if (!library || !method) {
        return Support::Unexpected(std::string(called.substr(0, open)) +
                                   " has no push item in this build");
    }

    const std::vector<std::string_view> arguments =
        SplitArguments(line.substr(open + 1, line.size() - open - 2));
    AfpScript::Instruction push = Bare(AfpScript::Op::kPush);
    for (std::size_t at = arguments.size(); at > 0; at--) {
        auto item = ItemFrom(animation, bytecode, arguments[at - 1]);
        if (!item) return Support::Unexpected(item.error());
        push.items.push_back(std::move(*item));
    }
    push.items.push_back(AfpScript::NumberItem(static_cast<int32_t>(arguments.size())));
    push.items.push_back(*library);
    out.push_back(std::move(push));
    out.push_back(Bare(AfpScript::Op::kGetVariable));
    AfpScript::Instruction named = Bare(AfpScript::Op::kPush);
    named.items.push_back(*method);
    out.push_back(std::move(named));
    out.push_back(Bare(AfpScript::Op::kCallMethod));
    if (pops) out.push_back(Bare(AfpScript::Op::kPop));
    return {};
}

std::vector<std::string_view> SourceLines(std::string_view source) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t at = 0; at <= source.size(); at++) {
        if (at < source.size() && source[at] != '\n') continue;
        const std::string_view line = Trimmed(source.substr(start, at - start));
        start = at + 1;
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

struct Called {
    std::string_view name;
    std::vector<std::string_view> arguments;
};

std::optional<Called> CalledFrom(std::string_view text) {
    const std::size_t open = text.find('(');
    if (open == std::string_view::npos || text.back() != ')') return std::nullopt;
    return Called{.name = Trimmed(text.substr(0, open)),
                  .arguments = SplitArguments(text.substr(open + 1, text.size() - open - 2))};
}

std::size_t MarkAt(std::string_view line, std::string_view mark) {
    bool quoted = false;
    for (std::size_t at = 0; at + mark.size() <= line.size(); at++) {
        if (line[at] == '"') quoted = !quoted;
        if (!quoted && line.substr(at, mark.size()) == mark) return at;
    }
    return std::string_view::npos;
}

Support::Expected<void, std::string> PushArguments(AfpAnimation::Animation& animation,
                                                   AfpAnimation::Bytecode& bytecode,
                                                   const std::vector<std::string_view>& arguments,
                                                   AfpScript::Instruction& push) {
    for (std::size_t at = arguments.size(); at > 0; at--) {
        auto item = ItemFrom(animation, bytecode, arguments[at - 1]);
        if (!item) return Support::Unexpected(item.error());
        push.items.push_back(std::move(*item));
    }
    push.items.push_back(AfpScript::NumberItem(static_cast<int32_t>(arguments.size())));
    return {};
}

bool Kept(std::string_view line) {
    return line.starts_with(kKeep) && line.size() > kKeep.size() && line[kKeep.size()] == ' ';
}

std::optional<Called> MethodOn(std::string_view line, std::string_view bound) {
    const std::string_view called = Kept(line) ? Trimmed(line.substr(kKeep.size())) : line;
    if (!called.starts_with(bound) || called.size() <= bound.size()) return std::nullopt;
    if (called[bound.size()] != '.') return std::nullopt;
    return CalledFrom(called.substr(bound.size() + 1));
}

Support::Expected<void, std::string> ReadMemberWrite(AfpAnimation::Animation& animation,
                                                     AfpAnimation::Bytecode& bytecode,
                                                     std::string_view line, std::size_t mark,
                                                     std::vector<AfpScript::Instruction>& out) {
    const std::string_view target = Trimmed(line.substr(0, mark));
    const std::string_view value = Trimmed(line.substr(mark + kSetMark.size()));
    const std::size_t dot = target.find('.');
    const std::string_view object = target.substr(0, dot);
    const std::string_view member = target.substr(dot + 1);

    AfpScript::Instruction push = Bare(AfpScript::Op::kPush);
    auto held = ItemFrom(animation, bytecode, object);
    if (!held) return Support::Unexpected(held.error());
    const std::optional<uint16_t> slot = StringSlot(animation, bytecode, member);
    if (!slot) return Support::Unexpected(std::string("the script holds too many strings"));
    auto written = ItemFrom(animation, bytecode, value);
    if (!written) return Support::Unexpected(written.error());

    push.items.push_back(std::move(*held));
    push.items.push_back(StringItem(*slot));
    push.items.push_back(std::move(*written));
    out.push_back(std::move(push));
    out.push_back(Bare(AfpScript::Op::kSetMember));
    return {};
}

Support::Expected<std::size_t, std::string> ReadLet(AfpAnimation::Animation& animation,
                                                    AfpAnimation::Bytecode& bytecode,
                                                    const std::vector<std::string_view>& lines,
                                                    std::size_t at,
                                                    std::vector<AfpScript::Instruction>& out) {
    const std::string_view line = lines[at];
    const std::size_t mark = MarkAt(line, kSetMark);
    if (mark == std::string_view::npos)
        return Support::Unexpected(std::string("a let binds a register: let r1 = stop()"));
    const std::string_view bound = Trimmed(line.substr(kLet.size(), mark - kLet.size()));
    const std::optional<uint8_t> reg = RegisterNumber(bound);
    if (!reg) return Support::Unexpected("a let binds a register, not " + std::string(bound));

    const std::optional<Called> call = CalledFrom(Trimmed(line.substr(mark + kSetMark.size())));
    if (!call) return Support::Unexpected(std::string("a let binds the result of a call"));
    const std::optional<uint16_t> id = CallId(call->name);
    const std::optional<AfpScript::Item> named = id ? AfpScript::BuiltinItem(*id) : std::nullopt;
    if (!named)
        return Support::Unexpected(std::string(call->name) + " has no push item in this build");

    const std::optional<Called> second =
        at + 1 < lines.size() ? MethodOn(lines[at + 1], bound) : std::nullopt;
    const std::optional<uint16_t> other = second ? CallId(second->name) : std::nullopt;
    const std::optional<AfpScript::Item> tail =
        other ? AfpScript::BuiltinItem(*other) : std::nullopt;
    if (second && !tail)
        return Support::Unexpected(std::string(second->name) + " is not a call this build knows");

    AfpScript::Instruction push = Bare(AfpScript::Op::kPush);
    if (second) {
        auto more = PushArguments(animation, bytecode, second->arguments, push);
        if (!more) return Support::Unexpected(more.error());
    }
    auto first = PushArguments(animation, bytecode, call->arguments, push);
    if (!first) return Support::Unexpected(first.error());
    push.items.push_back(*named);
    out.push_back(std::move(push));
    out.push_back(Bare(AfpScript::Op::kCallFunction));
    AfpScript::Instruction store = Bare(AfpScript::Op::kStoreRegister);
    store.registers.push_back(*reg);
    out.push_back(std::move(store));
    if (!tail) return std::size_t{1};

    AfpScript::Instruction calling = Bare(AfpScript::Op::kPush);
    calling.items.push_back(*tail);
    out.push_back(std::move(calling));
    out.push_back(Bare(AfpScript::Op::kCallMethod));
    if (!Kept(lines[at + 1])) out.push_back(Bare(AfpScript::Op::kPop));
    return std::size_t{2};
}

bool WritesMember(std::string_view line, std::size_t mark) {
    if (mark == std::string_view::npos) return false;
    const std::string_view target = Trimmed(line.substr(0, mark));
    const std::size_t dot = target.find('.');
    if (dot == std::string_view::npos) return false;
    const std::string_view object = target.substr(0, dot);
    if (object != kThis && !RegisterNumber(object)) return false;
    return PlainName(target.substr(dot + 1));
}

Support::Expected<std::size_t, std::string>
ReadStatement(AfpAnimation::Animation& animation, AfpAnimation::Bytecode& bytecode,
              const std::vector<std::string_view>& lines, std::size_t at,
              std::vector<AfpScript::Instruction>& out) {
    const std::string_view line = lines[at];
    if (line.starts_with(kLet) && line.size() > kLet.size() && line[kLet.size()] == ' ')
        return ReadLet(animation, bytecode, lines, at, out);
    const std::size_t mark = MarkAt(line, kSetMark);
    if (WritesMember(line, mark)) {
        auto written = ReadMemberWrite(animation, bytecode, line, mark, out);
        if (!written) return Support::Unexpected(written.error());
        return std::size_t{1};
    }
    auto read = ReadCall(animation, bytecode, line, out);
    if (!read) return Support::Unexpected(read.error());
    return std::size_t{1};
}

}

std::span<const std::string_view> ScriptCalls() {
    static const std::vector<std::string_view> all = [] {
        std::vector<std::string_view> out;
        for (const std::string_view name : AfpScript::BuiltinNames()) {
            if (UsableName(name)) out.push_back(name);
        }
        return out;
    }();
    return all;
}

std::span<const std::string_view> ScriptInstructions() {
    return kInstructions;
}

std::span<const std::string_view> ScriptWords() {
    return kWords;
}

std::optional<uint16_t> ScriptCallId(std::string_view name) {
    return CallId(name);
}

std::vector<std::string> ScriptCallsUsed(std::string_view source) {
    std::vector<std::string> out;
    for (const std::string_view line : SourceLines(source)) {
        std::string_view rest = Kept(line) ? Trimmed(line.substr(kKeep.size())) : line;
        const std::size_t mark = MarkAt(rest, kSetMark);
        if (rest.starts_with(kLet) && mark != std::string_view::npos)
            rest = Trimmed(rest.substr(mark + kSetMark.size()));
        const std::optional<Called> call = CalledFrom(rest);
        if (!call) continue;
        std::string_view name = call->name;
        const std::size_t dot = name.rfind('.');
        if (dot != std::string_view::npos) name = name.substr(dot + 1);
        if (!CallId(name)) continue;
        if (std::ranges::find(out, name) == out.end()) out.emplace_back(name);
    }
    return out;
}

bool ScriptIsCalls(std::string_view source) {
    const std::vector<std::string_view> lines = SourceLines(source);
    if (lines.empty()) return false;
    return std::ranges::all_of(lines, [](const std::string_view line) {
        const std::string_view called = Kept(line) ? Trimmed(line.substr(kKeep.size())) : line;
        const std::optional<Called> call = CalledFrom(called);
        return call && CallId(call->name).has_value();
    });
}

Support::Expected<AfpAnimation::Bytecode, std::string>
CompileScript(AfpAnimation::Animation& animation, std::string_view source) {
    const std::vector<std::string_view> lines = SourceLines(source);

    AfpAnimation::Bytecode out;
    AfpScript::Script script;
    for (std::size_t at = 0; at < lines.size();) {
        auto read = ReadStatement(animation, out, lines, at, script.instructions);
        if (!read) return Support::Unexpected(read.error());
        at += *read;
    }
    if (script.instructions.empty() || script.instructions.back().opcode != AfpScript::Op::kEnd) {
        script.instructions.push_back(Bare(AfpScript::Op::kEnd));
    }

    auto code = AfpScript::Write(script);
    if (!code) return Support::Unexpected(code.error());
    out.flags = 0;
    out.code = std::move(*code);
    return out;
}

namespace {

std::optional<Match> Statement(const AfpAnimation::Animation& animation,
                               const AfpAnimation::Bytecode& bytecode,
                               const std::vector<AfpScript::Instruction>& code, std::size_t at) {
    if (const std::optional<Match> call = LibraryCallAt(animation, bytecode, code, at)) return call;
    if (const std::optional<Match> write = MemberWriteAt(animation, bytecode, code, at))
        return write;
    return RegisterChainAt(animation, bytecode, code, at);
}

}

std::optional<std::string> ScriptSourceText(const AfpAnimation::Animation& animation,
                                            const AfpAnimation::Bytecode& bytecode) {
    const auto script = AfpScript::Read(bytecode.code);
    if (!script) return std::nullopt;
    const std::vector<AfpScript::Instruction>& code = script->instructions;
    if (code.empty() || code.back().opcode != AfpScript::Op::kEnd) return std::nullopt;

    std::string out;
    std::size_t at = 0;
    while (at + 1 < code.size()) {
        if (const std::optional<Match> shaped = Statement(animation, bytecode, code, at)) {
            out += shaped->line + "\n";
            at += shaped->length;
            continue;
        }
        const std::optional<std::string> line = InstructionLine(animation, bytecode, code[at]);
        if (!line) return std::nullopt;
        out += *line + "\n";
        at++;
    }
    return out;
}

}
