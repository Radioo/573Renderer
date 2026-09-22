#include <catch2/catch_test_macros.hpp>

#include "formats/afp_animation.h"
#include "document/script_source.h"
#include "formats/afp_script.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "support/env.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <format>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

struct Counts {
    std::map<std::string, std::size_t> opcodes;
    std::map<std::string, std::size_t> shapes;
    std::map<std::string, std::size_t> calls;
    std::size_t scripts = 0;
    std::size_t containers = 0;
    std::size_t labelled = 0;
    std::size_t by_name = 0;
    std::size_t by_frame = 0;
    std::size_t script_labelled = 0;
    std::size_t script_by_name = 0;
    std::size_t script_by_frame = 0;
    std::size_t linear_lookup = 0;
    std::size_t animations = 0;
    std::size_t images = 0;
    std::size_t images_at_origin = 0;
    std::size_t lists = 0;
    std::size_t rewritten = 0;
    std::size_t sourced = 0;
    std::size_t recompiled = 0;
    std::size_t recompile_differs = 0;
    std::map<std::string, std::size_t> trailing;
    std::string first_difference;
    std::size_t unreadable = 0;
    std::string first_error;
    std::string first_shape;
    std::vector<uint8_t> first_bytes;
};

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string TextOf(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& code,
                   const AfpScript::Item& item) {
    if (!code.strings) return {};
    const uint16_t index = AfpScript::StringIndex(item);
    if (index >= code.strings->size()) return {};
    const AfpAnimation::StringId id = (*code.strings)[index];
    if (id >= animation.strings.size()) return {};
    return animation.strings[id];
}

std::string Named(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& code,
                  const AfpScript::Item& item) {
    if (AfpScript::ItemIsString(item)) return "\"" + TextOf(animation, code, item) + "\"";
    const std::optional<uint16_t> id = AfpScript::BuiltinId(item);
    if (id) return std::format("builtin {:#x}", *id);
    return "type " + std::to_string(item.type);
}

constexpr std::size_t kSdvxScripts = 211761;

const std::map<std::string, std::size_t> kExpectedCalls{
    {"builtin 0x390.builtin 0x440", 263},   {"builtin 0x390.builtin 0x442", 3323},
    {"builtin 0x390.builtin 0x443", 20},    {"builtin 0x390.builtin 0x814", 117},
    {"builtin 0x390.builtin 0x815", 9966},  {"builtin 0x390.builtin 0x832", 6236},
    {"builtin 0x390.builtin 0x833", 55808}, {"builtin 0x390.builtin 0x836", 413915},
};

void CountCall(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& code,
               const std::vector<AfpScript::Instruction>& script, Counts& counts) {
    for (std::size_t i = 0; i < script.size(); i++) {
        if (script[i].opcode != AfpScript::Op::kCallMethod || i < 3) continue;
        const AfpScript::Instruction& name_push = script[i - 1];
        const AfpScript::Instruction& object_push = script[i - 3];
        if (name_push.opcode != AfpScript::Op::kPush || name_push.items.empty()) continue;
        if (object_push.opcode != AfpScript::Op::kPush || object_push.items.empty()) continue;
        const AfpScript::Item& method = name_push.items.back();
        const AfpScript::Item& object = object_push.items.back();
        counts.calls[Named(animation, code, object) + "." + Named(animation, code, method)]++;
    }
}

void CountSource(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& code,
                 Counts& counts) {
    const std::optional<std::string> source = Document::ScriptSourceText(animation, code);
    if (!source) return;
    counts.sourced++;
    AfpAnimation::Animation copy = animation;
    const auto again = Document::CompileScript(copy, *source);
    if (!again) {
        counts.recompile_differs++;
        if (counts.first_difference.empty()) counts.first_difference = again.error();
        return;
    }
    const auto script = AfpScript::Read(code.code);
    const std::size_t slack = script ? script->trailing.size() : 0;
    const std::vector<uint8_t> instructions(code.code.begin(),
                                            code.code.end() - static_cast<std::ptrdiff_t>(slack));
    if (again->code == instructions) {
        counts.recompiled++;
        return;
    }
    counts.recompile_differs++;
    if (counts.first_difference.empty()) {
        std::string mine;
        for (const uint8_t byte : again->code)
            mine += std::format("{:02x} ", byte);
        std::string theirs;
        for (const uint8_t byte : instructions)
            theirs += std::format("{:02x} ", byte);
        counts.first_difference = "mine " + mine + "| theirs " + theirs;
    }
}

void CountScript(const AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& code,
                 Counts& counts) {
    counts.scripts++;
    const auto script = AfpScript::Read(code.code);
    if (!script) {
        counts.unreadable++;
        if (counts.first_error.empty()) counts.first_error = script.error();
        return;
    }
    const auto written = AfpScript::Write(*script);
    if (!written || *written != code.code) counts.rewritten++;
    counts.trailing[std::to_string(script->trailing.size()) + " after " +
                    std::to_string(code.code.size() - script->trailing.size())]++;
    std::string shape;
    for (const AfpScript::Instruction& instruction : script->instructions) {
        counts.opcodes[AfpScript::OpcodeName(instruction.opcode)]++;
        if (!shape.empty()) shape += " ";
        shape += AfpScript::OpcodeName(instruction.opcode);
        if (instruction.opcode == AfpScript::Op::kPush)
            shape += "(" + std::to_string(instruction.items.size()) + ")";
    }
    counts.shapes[shape]++;
    if (counts.first_shape.empty()) {
        counts.first_shape = shape;
        counts.first_bytes = code.code;
    }
    CountCall(animation, code, script->instructions, counts);
    CountSource(animation, code, counts);
}

void WalkContainer(const AfpAnimation::Animation& animation,
                   const AfpAnimation::Container& container, Counts& counts);

void WalkTag(const AfpAnimation::Animation& animation, const AfpAnimation::Tag& tag,
             Counts& counts) {
    if (const auto* action = std::get_if<AfpAnimation::Action>(&tag.body)) {
        CountScript(animation, action->bytecode, counts);
    } else if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) {
        WalkContainer(animation, sprite->container, counts);
    } else if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body)) {
        if (!placement->clip_actions) return;
        for (const AfpAnimation::ClipEvent& event : placement->clip_actions->events)
            CountScript(animation, event.bytecode, counts);
    }
}

bool SortedByName(const AfpAnimation::Animation& animation,
                  const std::vector<AfpAnimation::Label>& labels) {
    for (std::size_t i = 1; i < labels.size(); i++) {
        const AfpAnimation::StringId before = labels[i - 1].name;
        const AfpAnimation::StringId after = labels[i].name;
        if (before >= animation.strings.size() || after >= animation.strings.size()) return false;
        if (animation.strings[before] > animation.strings[after]) return false;
    }
    return true;
}

bool SortedByFrame(const std::vector<AfpAnimation::Label>& labels) {
    for (std::size_t i = 1; i < labels.size(); i++) {
        if (labels[i - 1].frame > labels[i].frame) return false;
    }
    return true;
}

void CountLabels(const AfpAnimation::Animation& animation, const AfpAnimation::Container& container,
                 Counts& counts) {
    counts.containers++;
    if (container.labels.size() >= 2) {
        counts.labelled++;
        if (SortedByName(animation, container.labels)) counts.by_name++;
        if (SortedByFrame(container.labels)) counts.by_frame++;
    }
    if (container.script_labels && container.script_labels->size() >= 2) {
        counts.script_labelled++;
        if (SortedByName(animation, *container.script_labels)) counts.script_by_name++;
        if (SortedByFrame(*container.script_labels)) counts.script_by_frame++;
    }
}

void WalkContainer(const AfpAnimation::Animation& animation,
                   const AfpAnimation::Container& container, Counts& counts) {
    CountLabels(animation, container, counts);
    for (const AfpAnimation::Tag& tag : container.tags)
        WalkTag(animation, tag, counts);
}

void CountImages(const Ifs::Archive& archive, Counts& counts) {
    const auto textures = std::ranges::find(archive.entries, std::string("tex"), &Ifs::Entry::name);
    if (textures == archive.entries.end()) return;
    const auto list =
        std::ranges::find(textures->children, std::string("texturelist_Exml"), &Ifs::Entry::name);
    if (list == textures->children.end()) return;
    const auto document = BinaryXml::Read(list->bytes);
    if (!document) return;
    counts.lists++;
    for (const BinaryXml::Node& texture : document->root.children) {
        if (texture.name != "texture") continue;
        for (const BinaryXml::Node& image : texture.children) {
            if (image.name != "imgrect" && image.name != "image") continue;
            for (const BinaryXml::Node& rect : image.children) {
                if (rect.name != "imgrect" || rect.value.size() != 8) continue;
                counts.images++;
                if (BigEndian::ReadU16(rect.value, 0) == 0 &&
                    BigEndian::ReadU16(rect.value, 4) == 0)
                    counts.images_at_origin++;
            }
        }
    }
}

void WalkArchive(const Ifs::Archive& archive, Counts& counts) {
    const auto animations =
        std::ranges::find(archive.entries, std::string("afp"), &Ifs::Entry::name);
    if (animations == archive.entries.end()) return;
    const auto scripts =
        std::ranges::find(animations->children, std::string("bsi"), &Ifs::Entry::name);
    if (scripts == animations->children.end()) return;
    for (const Ifs::Entry& entry : animations->children) {
        if (entry.kind != Ifs::EntryKind::File) continue;
        const auto script = std::ranges::find(scripts->children, entry.name, &Ifs::Entry::name);
        if (script == scripts->children.end()) continue;
        const auto animation = AfpAnimation::ReadStored(entry.bytes, script->bytes);
        if (!animation) continue;
        counts.animations++;
        if ((animation->flags & 0x8) != 0) counts.linear_lookup++;
        WalkContainer(*animation, animation->root, counts);
    }
}

std::size_t WalkInstall(const std::string& dir, Counts& counts) {
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir + "/data")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ifs") continue;
        const auto archive = Ifs::Read(ReadAll(entry.path()));
        if (!archive) continue;
        files++;
        WalkArchive(*archive, counts);
        CountImages(*archive, counts);
        if (files % 200 == 0) {
            std::cerr << std::format("[afp scripts] {} files\n", files);
        }
    }
    return files;
}

void CheckRoundTrip(const Counts& counts) {
    CHECK(counts.sourced == counts.scripts);
    CHECK(counts.recompiled == counts.scripts);
    CHECK(counts.recompile_differs == 0);
    CHECK(counts.unreadable == 0);
    CHECK(counts.rewritten == 0);
}

}

TEST_CASE("Every script in the SDVX install reads back as source and recompiles to the bytes") {
    const std::string dir = Support::EnvVar("R573_SDVX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_SDVX_DIR not set");

    Counts counts;
    const std::size_t files = WalkInstall(dir, counts);
    std::cerr << std::format(
        "[afp scripts] SDVX {} files, {} scripts, {} read back as source, {} differ\n", files,
        counts.scripts, counts.sourced, counts.recompile_differs);
    if (!counts.first_difference.empty())
        std::cerr << std::format("[afp scripts] first difference: {}\n", counts.first_difference);
    CheckRoundTrip(counts);
    CHECK(counts.scripts == kSdvxScripts);
}

TEST_CASE("Every script in the install reads and writes back, and the counts match the survey") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    Counts counts;
    const std::size_t files = WalkInstall(dir, counts);

    std::cerr << std::format("[afp scripts] {} files, {} scripts, {} unreadable, {} rewritten\n",
                             files, counts.scripts, counts.unreadable, counts.rewritten);
    std::cerr << std::format(
        "[afp scripts] {} texture lists, {} images, {} with imgrect at the origin\n", counts.lists,
        counts.images, counts.images_at_origin);
    std::cerr << std::format("[afp scripts] {} animations, {} with the linear label lookup\n",
                             counts.animations, counts.linear_lookup);
    std::cerr << std::format(
        "[afp scripts] {} containers, {} with two or more labels, {} sorted by name, {} sorted by "
        "frame\n",
        counts.containers, counts.labelled, counts.by_name, counts.by_frame);
    std::cerr << std::format(
        "[afp scripts] {} with two or more script labels, {} sorted by name, {} sorted by frame\n",
        counts.script_labelled, counts.script_by_name, counts.script_by_frame);
    std::cerr << std::format(
        "[afp scripts] {} read back as source, {} recompile to the same bytes, {} differ\n",
        counts.sourced, counts.recompiled, counts.recompile_differs);
    std::size_t trailing_shown = 0;
    for (const auto& [shape, count] : counts.trailing) {
        if (trailing_shown++ >= 12) break;
        std::cerr << std::format("[afp scripts] trailing {}: {}\n", shape, count);
    }
    if (!counts.first_difference.empty())
        std::cerr << std::format("[afp scripts] first difference: {}\n", counts.first_difference);
    if (!counts.first_error.empty()) {
        std::cerr << std::format("[afp scripts] first error: {}\n", counts.first_error);
    }
    std::string bytes;
    for (const uint8_t byte : counts.first_bytes)
        bytes += std::format("{:02x} ", byte);
    std::cerr << std::format("[afp scripts] first shape: {}\n", counts.first_shape);
    std::cerr << std::format("[afp scripts] first bytes: {}\n", bytes);
    for (const auto& [name, count] : counts.opcodes)
        std::cerr << std::format("[afp scripts] opcode {}: {}\n", name, count);
    for (const auto& [name, count] : counts.calls)
        std::cerr << std::format("[afp scripts] call {}: {}\n", name, count);
    std::size_t shown = 0;
    for (const auto& [shape, count] : counts.shapes) {
        if (shown++ >= 20) break;
        std::cerr << std::format("[afp scripts] shape {}: {}\n", shape, count);
    }

    CheckRoundTrip(counts);
    CHECK(counts.scripts == 463562);
    CHECK(counts.calls == kExpectedCalls);
}
