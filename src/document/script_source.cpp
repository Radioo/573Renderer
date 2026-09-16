#include "document/script_source.h"

#include "document/animation_strings.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"
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
constexpr std::string_view kLibraryName = "aeplib";

struct Call {
    std::string_view name;
    uint16_t id;
};

constexpr std::array<Call, 8> kCalls{{
    {.name = "aep_set_frame_control", .id = 0x832},
    {.name = "aep_set_rect_mask", .id = 0x833},
    {.name = "aep_set_set_frame", .id = 0x836},
    {.name = "deepGotoAndPlay", .id = 0x815},
    {.name = "deepStop", .id = 0x814},
    {.name = "gotoAndPlay", .id = 0x442},
    {.name = "gotoAndStop", .id = 0x443},
    {.name = "stop", .id = 0x440},
}};

constexpr auto kCallNames = [] {
    std::array<std::string_view, kCalls.size()> out{};
    for (std::size_t i = 0; i < kCalls.size(); i++)
        out[i] = kCalls[i].name;
    return out;
}();

enum class ArgumentKind : uint8_t { Number, Text, This };

constexpr std::string_view kThis = "this";

struct Argument {
    ArgumentKind kind = ArgumentKind::Number;
    std::string text;
    int32_t number = 0;
};

struct Parsed {
    uint16_t id = 0;
    std::vector<Argument> arguments;
};

std::string_view Trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);
    return text;
}

Support::Expected<Argument, std::string> ReadArgument(std::string_view text) {
    if (text == kThis) return Argument{.kind = ArgumentKind::This, .text = {}, .number = 0};
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return Argument{.kind = ArgumentKind::Text,
                        .text = std::string(text.substr(1, text.size() - 2)),
                        .number = 0};
    }
    if (text.find('"') != std::string_view::npos) {
        return Support::Unexpected("a piece of text needs quotes around all of it: " +
                                   std::string(text));
    }
    int32_t value = 0;
    const auto* end = text.data() + text.size();
    const auto read = std::from_chars(text.data(), end, value);
    if (read.ec != std::errc{} || read.ptr != end) {
        return Support::Unexpected("an argument is a whole number or quoted text, not " +
                                   std::string(text));
    }
    return Argument{.kind = ArgumentKind::Number, .text = {}, .number = value};
}

Support::Expected<std::vector<Argument>, std::string> ReadArguments(std::string_view inside) {
    std::vector<Argument> arguments;
    if (Trimmed(inside).empty()) return arguments;
    std::size_t start = 0;
    bool quoted = false;
    for (std::size_t i = 0; i <= inside.size(); i++) {
        if (i < inside.size() && inside[i] == '"') quoted = !quoted;
        if (i < inside.size() && (inside[i] != ',' || quoted)) continue;
        auto argument = ReadArgument(Trimmed(inside.substr(start, i - start)));
        if (!argument) return Support::Unexpected(argument.error());
        arguments.push_back(std::move(*argument));
        start = i + 1;
    }
    if (quoted)
        return Support::Unexpected(std::string("a piece of text is missing its closing quote"));
    return arguments;
}

Support::Expected<Parsed, std::string> ReadLine(std::string_view line) {
    const std::size_t open = line.find('(');
    if (open == std::string_view::npos || line.back() != ')')
        return Support::Unexpected("a line is one call, like stop(), not " + std::string(line));
    const std::string_view name = Trimmed(line.substr(0, open));
    const auto known = std::ranges::find(kCalls, name, &Call::name);
    if (known == kCalls.end()) {
        return Support::Unexpected("this editor only knows the aeplib calls the game's own data "
                                   "uses, and " +
                                   std::string(name) + " is not one of them");
    }
    auto arguments = ReadArguments(line.substr(open + 1, line.size() - open - 2));
    if (!arguments) return Support::Unexpected(arguments.error());
    return Parsed{.id = known->id, .arguments = std::move(*arguments)};
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

constexpr std::size_t kCallLength = 5;

std::optional<std::string> ArgumentText(const AfpAnimation::Animation& animation,
                                        const AfpAnimation::Bytecode& bytecode,
                                        const AfpScript::Item& item) {
    if (AfpScript::ItemIsString(item)) {
        if (!bytecode.strings) return std::nullopt;
        const uint16_t index = AfpScript::StringIndex(item);
        if (index >= bytecode.strings->size()) return std::nullopt;
        return "\"" + StringText(animation, (*bytecode.strings)[index]) + "\"";
    }
    if (item.type == AfpScript::PushType::kStoredObject) return std::string(kThis);
    const auto number = AfpScript::ItemNumber(item);
    if (!number) return std::nullopt;
    return std::to_string(*number);
}

AfpScript::Instruction Bare(uint8_t opcode) {
    return AfpScript::Instruction{.opcode = opcode,
                                  .items = {},
                                  .registers = {},
                                  .flags = 0,
                                  .frame_bias = 0,
                                  .has_frame_bias = false};
}

Support::Expected<AfpScript::Item, std::string> ArgumentItem(AfpAnimation::Animation& animation,
                                                             AfpAnimation::Bytecode& bytecode,
                                                             const Argument& argument) {
    if (argument.kind == ArgumentKind::This)
        return AfpScript::Item{.type = AfpScript::PushType::kStoredObject, .operand = {}};
    if (argument.kind == ArgumentKind::Number) return AfpScript::NumberItem(argument.number);
    const std::optional<uint16_t> slot = StringSlot(animation, bytecode, argument.text);
    if (!slot) return Support::Unexpected(std::string("the script holds too many strings"));
    return StringItem(*slot);
}

std::string CallName(uint16_t id) {
    const auto found = std::ranges::find(kCalls, id, &Call::id);
    return found == kCalls.end() ? std::string() : std::string(found->name);
}

std::optional<std::string> CallText(const AfpAnimation::Animation& animation,
                                    const AfpAnimation::Bytecode& bytecode,
                                    const std::vector<AfpScript::Instruction>& code,
                                    std::size_t at) {
    if (code[at].opcode != AfpScript::Op::kPush ||
        code[at + 1].opcode != AfpScript::Op::kGetVariable ||
        code[at + 2].opcode != AfpScript::Op::kPush ||
        code[at + 3].opcode != AfpScript::Op::kCallMethod ||
        code[at + 4].opcode != AfpScript::Op::kPop) {
        return std::nullopt;
    }
    const std::vector<AfpScript::Item>& pushed = code[at].items;
    if (pushed.size() < 2 || code[at + 2].items.size() != 1) return std::nullopt;
    const std::optional<uint16_t> object = AfpScript::BuiltinId(pushed.back());
    const std::optional<uint16_t> method = AfpScript::BuiltinId(code[at + 2].items.front());
    if (!object || *object != kLibraryId || !method) return std::nullopt;
    const std::string name = CallName(*method);
    if (name.empty()) return std::nullopt;
    const auto count = AfpScript::ItemNumber(pushed[pushed.size() - 2]);
    if (!count || *count < 0 || static_cast<std::size_t>(*count) != pushed.size() - 2)
        return std::nullopt;

    std::string out = name + "(";
    const auto arguments = static_cast<std::size_t>(*count);
    for (std::size_t i = arguments; i > 0; i--) {
        if (i != arguments) out += ", ";
        const std::optional<std::string> text = ArgumentText(animation, bytecode, pushed[i - 1]);
        if (!text) return std::nullopt;
        out += *text;
    }
    return out + ")\n";
}

}

std::span<const std::string_view> ScriptCalls() {
    return kCallNames;
}

Support::Expected<AfpAnimation::Bytecode, std::string>
CompileScript(AfpAnimation::Animation& animation, std::string_view source) {
    std::vector<Parsed> calls;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= source.size(); i++) {
        if (i < source.size() && source[i] != '\n') continue;
        const std::string_view line = Trimmed(source.substr(start, i - start));
        start = i + 1;
        if (line.empty()) continue;
        auto parsed = ReadLine(line);
        if (!parsed) return Support::Unexpected(parsed.error());
        calls.push_back(std::move(*parsed));
    }
    if (calls.empty()) return Support::Unexpected(std::string("a script runs at least one call"));

    const std::optional<AfpScript::Item> library = AfpScript::BuiltinItem(kLibraryId);
    if (!library) return Support::Unexpected(std::string(kLibraryName) + " has no push item");

    AfpAnimation::Bytecode out;
    AfpScript::Script script;
    for (const Parsed& call : calls) {
        const std::optional<AfpScript::Item> method = AfpScript::BuiltinItem(call.id);
        if (!method) return Support::Unexpected(CallName(call.id) + " has no push item");

        AfpScript::Instruction push = Bare(AfpScript::Op::kPush);
        for (std::size_t i = call.arguments.size(); i > 0; i--) {
            auto item = ArgumentItem(animation, out, call.arguments[i - 1]);
            if (!item) return Support::Unexpected(item.error());
            push.items.push_back(std::move(*item));
        }
        push.items.push_back(AfpScript::NumberItem(static_cast<int32_t>(call.arguments.size())));
        push.items.push_back(*library);
        script.instructions.push_back(std::move(push));
        script.instructions.push_back(Bare(AfpScript::Op::kGetVariable));
        AfpScript::Instruction named = Bare(AfpScript::Op::kPush);
        named.items.push_back(*method);
        script.instructions.push_back(std::move(named));
        script.instructions.push_back(Bare(AfpScript::Op::kCallMethod));
        script.instructions.push_back(Bare(AfpScript::Op::kPop));
    }
    script.instructions.push_back(Bare(AfpScript::Op::kEnd));

    auto code = AfpScript::Write(script);
    if (!code) return Support::Unexpected(code.error());
    out.flags = out.strings ? uint8_t{1} : uint8_t{0};
    out.code = std::move(*code);
    return out;
}

std::optional<std::string> ScriptSourceText(const AfpAnimation::Animation& animation,
                                            const AfpAnimation::Bytecode& bytecode) {
    const auto script = AfpScript::Read(bytecode.code);
    if (!script) return std::nullopt;
    const std::vector<AfpScript::Instruction>& code = script->instructions;
    if (code.empty() || code.back().opcode != AfpScript::Op::kEnd) return std::nullopt;
    if ((code.size() - 1) % kCallLength != 0) return std::nullopt;

    std::string out;
    for (std::size_t at = 0; at + 1 < code.size(); at += kCallLength) {
        const std::optional<std::string> line = CallText(animation, bytecode, code, at);
        if (!line) return std::nullopt;
        out += *line;
    }
    return out;
}

}
