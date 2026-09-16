#include "document/library_call.h"

#include "document/animation_strings.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <format>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::size_t kObjectPush = 0;
constexpr std::size_t kMethodPush = 2;
constexpr std::size_t kCallMethodAt = 3;

struct Builtin {
    uint16_t id;
    std::string_view name;
};

constexpr std::array<Builtin, 9> kBuiltins{{
    {.id = 0x390, .name = "aeplib"},
    {.id = 0x440, .name = "stop"},
    {.id = 0x442, .name = "gotoAndPlay"},
    {.id = 0x443, .name = "gotoAndStop"},
    {.id = 0x814, .name = "deepStop"},
    {.id = 0x815, .name = "deepGotoAndPlay"},
    {.id = 0x832, .name = "aep_set_frame_control"},
    {.id = 0x833, .name = "aep_set_rect_mask"},
    {.id = 0x836, .name = "aep_set_set_frame"},
}};

std::string BuiltinName(uint16_t id) {
    const auto found = std::ranges::find(kBuiltins, id, &Builtin::id);
    if (found != kBuiltins.end()) return std::string(found->name);
    return std::format("builtin {:#x}", id);
}

std::string TextOf(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& bytecode,
                   const AfpScript::Item& item) {
    if (!bytecode.strings) return {};
    const uint16_t index = AfpScript::StringIndex(item);
    if (index >= bytecode.strings->size()) return {};
    return StringText(animation, (*bytecode.strings)[index]);
}

std::optional<std::string> NameOf(const AfpAnimation::Animation& animation,
                                  const AfpAnimation::Bytecode& bytecode,
                                  const AfpScript::Item& item) {
    if (AfpScript::ItemIsString(item)) return TextOf(animation, bytecode, item);
    const std::optional<uint16_t> id = AfpScript::BuiltinId(item);
    if (!id) return std::nullopt;
    return BuiltinName(*id);
}

bool ShapeIsLibraryCall(const AfpScript::Script& script) {
    const std::vector<AfpScript::Instruction>& code = script.instructions;
    if (code.size() < 5 || code.size() > 6) return false;
    if (code.back().opcode != AfpScript::Op::kEnd) return false;
    if (code.size() == 6 && code[4].opcode != AfpScript::Op::kPop) return false;
    return code[0].opcode == AfpScript::Op::kPush &&
           code[1].opcode == AfpScript::Op::kGetVariable &&
           code[2].opcode == AfpScript::Op::kPush &&
           code[kCallMethodAt].opcode == AfpScript::Op::kCallMethod;
}

CallArgument ArgumentOf(const AfpAnimation::Animation& animation,
                        const AfpAnimation::Bytecode& bytecode, const AfpScript::Item& item) {
    if (AfpScript::ItemIsString(item))
        return CallArgument{.is_string = true, .text = TextOf(animation, bytecode, item)};
    const auto number = AfpScript::ItemNumber(item);
    if (number) return CallArgument{.is_string = false, .text = std::to_string(*number)};
    if (item.type == AfpScript::PushType::kStoredObject)
        return CallArgument{.is_string = false, .text = "this"};
    const std::optional<uint16_t> id = AfpScript::BuiltinId(item);
    if (id) return CallArgument{.is_string = false, .text = BuiltinName(*id)};
    return CallArgument{.is_string = false, .text = "type " + std::to_string(item.type)};
}

std::string ItemListing(const AfpAnimation::Animation& animation,
                        const AfpAnimation::Bytecode& bytecode, const AfpScript::Item& item) {
    if (AfpScript::ItemIsString(item)) return "\"" + TextOf(animation, bytecode, item) + "\"";
    return ArgumentOf(animation, bytecode, item).text;
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
        const auto low = static_cast<uint8_t>(slot);
        return AfpScript::Item{.type = AfpScript::PushType::kShortString, .operand = {low}};
    }
    return AfpScript::Item{
        .type = AfpScript::PushType::kLongString,
        .operand = {static_cast<uint8_t>(slot >> 8U), static_cast<uint8_t>(slot & 0xFFU)}};
}

Support::Expected<int32_t, std::string> Number(std::string_view text) {
    int32_t value = 0;
    const auto* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
        return Support::Unexpected("not a number: " + std::string(text));
    return value;
}

}

std::optional<LibraryCall> ReadLibraryCall(const AfpAnimation::Animation& animation,
                                           const AfpAnimation::Bytecode& bytecode) {
    const auto script = AfpScript::Read(bytecode.code);
    if (!script || !ShapeIsLibraryCall(*script)) return std::nullopt;
    const std::vector<AfpScript::Item>& pushed = script->instructions[kObjectPush].items;
    const std::vector<AfpScript::Item>& named = script->instructions[kMethodPush].items;
    if (pushed.size() < 2 || named.size() != 1) return std::nullopt;
    const std::optional<std::string> object = NameOf(animation, bytecode, pushed.back());
    const std::optional<std::string> method = NameOf(animation, bytecode, named.front());
    if (!object || !method) return std::nullopt;
    const auto count = AfpScript::ItemNumber(pushed[pushed.size() - 2]);
    if (!count || *count < 0 || static_cast<std::size_t>(*count) != pushed.size() - 2)
        return std::nullopt;

    LibraryCall call{.object = *object, .method = *method, .arguments = {}};
    const auto arguments = static_cast<std::size_t>(*count);
    call.arguments.reserve(arguments);
    for (std::size_t i = arguments; i > 0; i--)
        call.arguments.push_back(ArgumentOf(animation, bytecode, pushed[i - 1]));
    return call;
}

Support::Expected<AfpAnimation::Bytecode, std::string>
WriteLibraryCall(AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& original,
                 const LibraryCall& call) {
    auto script = AfpScript::Read(original.code);
    if (!script) return Support::Unexpected(script.error());
    if (!ShapeIsLibraryCall(*script))
        return Support::Unexpected(std::string("this script is not a library call"));
    std::vector<AfpScript::Item>& pushed = script->instructions[kObjectPush].items;
    if (pushed.size() != call.arguments.size() + 2)
        return Support::Unexpected(std::string("the call takes a different number of arguments"));

    AfpAnimation::Bytecode out = original;
    for (std::size_t i = 0; i < call.arguments.size(); i++) {
        const CallArgument& argument = call.arguments[i];
        AfpScript::Item& item = pushed[call.arguments.size() - 1 - i];
        if (argument.is_string) {
            const std::optional<uint16_t> slot = StringSlot(animation, out, argument.text);
            if (!slot) return Support::Unexpected(std::string("the script holds too many strings"));
            item = StringItem(*slot);
            continue;
        }
        if (!AfpScript::ItemIsString(item) && !AfpScript::ItemNumber(item)) continue;
        auto number = Number(argument.text);
        if (!number) return Support::Unexpected(number.error());
        item = AfpScript::NumberItem(*number);
    }

    auto code = AfpScript::Write(*script);
    if (!code) return Support::Unexpected(code.error());
    out.code = std::move(*code);
    return out;
}

std::vector<std::string> ScriptListing(const AfpAnimation::Animation& animation,
                                       const AfpAnimation::Bytecode& bytecode) {
    const auto script = AfpScript::Read(bytecode.code);
    if (!script) return {script.error()};
    std::vector<std::string> lines;
    for (const AfpScript::Instruction& instruction : script->instructions) {
        std::string line = AfpScript::OpcodeName(instruction.opcode);
        for (const AfpScript::Item& item : instruction.items)
            line += " " + ItemListing(animation, bytecode, item);
        for (const uint8_t reg : instruction.registers)
            line += " r" + std::to_string(reg);
        if (instruction.opcode == AfpScript::Op::kGotoFrame2) {
            line += " " + std::to_string(instruction.flags);
            if (instruction.has_frame_bias) line += " +" + std::to_string(instruction.frame_bias);
        }
        lines.push_back(line);
    }
    return lines;
}

std::string CallArgumentField(std::size_t index) {
    return "Call argument " + std::to_string(index + 1);
}

std::optional<std::size_t> CallArgumentIndex(std::string_view field) {
    constexpr std::string_view kPrefix = "Call argument ";
    if (!field.starts_with(kPrefix)) return std::nullopt;
    const std::string_view digits = field.substr(kPrefix.size());
    std::size_t number = 0;
    const auto* end = digits.data() + digits.size();
    const auto parsed = std::from_chars(digits.data(), end, number);
    if (parsed.ec != std::errc{} || parsed.ptr != end || number == 0) return std::nullopt;
    return number - 1;
}

std::vector<Field> ScriptFields(const AfpAnimation::Animation& animation,
                                const AfpAnimation::Bytecode& bytecode) {
    std::vector<Field> fields;
    const std::optional<LibraryCall> call = ReadLibraryCall(animation, bytecode);
    if (call) {
        fields.push_back(Field{.name = "Call", .value = call->object + "." + call->method});
        for (std::size_t i = 0; i < call->arguments.size(); i++)
            fields.push_back(Field{.name = CallArgumentField(i), .value = call->arguments[i].text});
        return fields;
    }
    const std::vector<std::string> lines = ScriptListing(animation, bytecode);
    for (std::size_t i = 0; i < lines.size(); i++)
        fields.push_back(Field{.name = "Script " + std::to_string(i + 1), .value = lines[i]});
    return fields;
}

}
