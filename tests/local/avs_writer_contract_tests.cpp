#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../formats/binary_test_support.h"
#include "avs_boot.h"
#include "avs_funcs.h"
#include "formats/avs_lz77.h"
#include "formats/binary_xml.h"
#include "support/dll_loader.h"
#include "support/env.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kCstreamCreate217 = 0x130;
constexpr int kCstreamOperate217 = 0x132;
constexpr int kCstreamFinish217 = 0x133;
constexpr int kCstreamDestroy217 = 0x134;
constexpr int kCompressOperator = 1;
constexpr int kMaxFinishSteps = 64;
constexpr int kPropertyPartWrite217 = 0x097;
constexpr int kPropertyBinaryReadWrite = 0x1F | 0x08;
constexpr int kPropertyLongNames = 0x1000;
constexpr std::size_t kPropertyWorkBytes = std::size_t{1} << 25U;

struct CstreamIo {
    uint8_t* out = nullptr;
    const uint8_t* in = nullptr;
    int32_t out_avail = 0;
    int32_t in_avail = 0;
};

struct Cstream {
    void* (*create)(int op) = nullptr;
    int (*operate)(void* ctx) = nullptr;
    int (*finish)(void* ctx) = nullptr;
    void (*destroy)(void* ctx) = nullptr;
};

std::vector<uint8_t> DllCompress(const Cstream& cs, std::span<const uint8_t> src) {
    std::vector<uint8_t> out(src.size() + (src.size() / 8) + 64);
    void* ctx = cs.create(kCompressOperator);
    REQUIRE(ctx != nullptr);
    auto* io = static_cast<CstreamIo*>(ctx);
    io->out = out.data();
    io->in = src.data();
    io->out_avail = static_cast<int32_t>(out.size());
    io->in_avail = static_cast<int32_t>(src.size());
    int done = cs.operate(ctx);
    io->in_avail = -1;
    for (int step = 0; done == 0 && step < kMaxFinishSteps; step++)
        done = cs.operate(ctx);
    const auto produced = out.size() - static_cast<std::size_t>(io->out_avail);
    cs.finish(ctx);
    cs.destroy(ctx);
    REQUIRE(done == 1);
    out.resize(produced);
    return out;
}

using PropertyPartWrite = int (*)(T_PROPERTY* property, T_PROPERTY_NODE* node, avs_reader_fn write,
                                  int ctx);

struct CallbackChannel {
    std::span<const uint8_t> source;
    std::size_t read_pos = 0;
    std::vector<uint8_t> sink;
};

CallbackChannel& Channel() {
    static CallbackChannel channel;
    return channel;
}

int ReadFromSource([[maybe_unused]] int context, void* buffer, int size) {
    CallbackChannel& channel = Channel();
    const std::size_t count =
        std::min(static_cast<std::size_t>(size), channel.source.size() - channel.read_pos);
    std::ranges::copy(channel.source.subspan(channel.read_pos, count),
                      std::span(static_cast<uint8_t*>(buffer), count).begin());
    channel.read_pos += count;
    return static_cast<int>(count);
}

int WriteToSink([[maybe_unused]] int context, void* buffer, int size) {
    const std::span<const uint8_t> bytes(static_cast<const uint8_t*>(buffer),
                                         static_cast<std::size_t>(size));
    Channel().sink.insert(Channel().sink.end(), bytes.begin(), bytes.end());
    return size;
}

std::vector<uint8_t> DllRewriteBinaryXml(const AvsFuncs& avs, PropertyPartWrite part_write,
                                         std::span<const uint8_t> bytes, bool long_names) {
    std::vector<uint8_t> work(kPropertyWorkBytes);
    const int flags = kPropertyBinaryReadWrite | (long_names ? kPropertyLongNames : 0);
    T_PROPERTY* property =
        avs.property_create(flags, work.data(), static_cast<unsigned int>(work.size()));
    REQUIRE(property != nullptr);
    Channel().source = bytes;
    Channel().read_pos = 0;
    REQUIRE(avs.property_insert_read(property, nullptr, &ReadFromSource, 0) >= 0);
    Channel().sink.clear();
    REQUIRE(part_write(property, nullptr, &WriteToSink, 0) >= 0);
    return std::move(Channel().sink);
}

std::vector<uint8_t> Pattern(std::size_t size, uint8_t seed) {
    std::vector<uint8_t> out(size);
    for (std::size_t i = 0; i < size; i++)
        out[i] = static_cast<uint8_t>(seed + (i * 37U));
    return out;
}

BinaryXml::Document EveryTypeDocument(uint8_t signature, uint8_t encoding) {
    constexpr std::array<uint8_t, 57> kSizes = {
        0, 0, 1,  1,  2,  2,  4,  4,  8, 8, 0,  0,  4,  4,  4,  8,  2,  2, 4,
        4, 8, 8,  16, 16, 8,  16, 3,  3, 6, 6,  12, 12, 24, 24, 12, 24, 4, 4,
        8, 8, 16, 16, 32, 32, 16, 32, 0, 0, 16, 16, 16, 16, 1,  2,  3,  4, 16,
    };
    constexpr std::array<uint8_t, 11> kArrayable = {2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 52};
    BinaryXml::Document doc;
    doc.signature = signature;
    doc.encoding = encoding;
    doc.root = TestSupport::MakeNode(1, "root", {});
    doc.root.attributes.push_back(
        TestSupport::MakeNode(BinaryXml::Type::kAttribute, "zeta", {'z', 0}));
    doc.root.attributes.push_back(
        TestSupport::MakeNode(BinaryXml::Type::kAttribute, "alpha", {'a', 'b', 0}));
    for (uint8_t type = 2; type <= 56; type++) {
        if (type == 10 || type == 11 || type == BinaryXml::Type::kAttribute || type == 47) continue;
        doc.root.children.push_back(TestSupport::MakeNode(type, "t" + std::to_string(type),
                                                          Pattern(kSizes.at(type), type)));
    }
    doc.root.children.push_back(TestSupport::MakeNode(10, "blob", Pattern(5, 9)));
    doc.root.children.push_back(TestSupport::MakeNode(10, "empty_blob", {}));
    doc.root.children.push_back(TestSupport::MakeNode(11, "text", {'h', 'i', 0}));
    doc.root.children.push_back(TestSupport::MakeNode(11, "empty_text", {0}));
    for (const uint8_t type : kArrayable) {
        doc.root.children.push_back(TestSupport::MakeNode(
            static_cast<uint8_t>(type | BinaryXml::kArrayFlag), "a" + std::to_string(type),
            Pattern(std::size_t{kSizes.at(type)} * 3U, type)));
    }
    doc.root.children.push_back(TestSupport::MakeNode(11, "invalid_sjis", {0x83, 0xFF, 0x81, 0}));
    if (signature == BinaryXml::kByteNames) {
        doc.root.children.push_back(TestSupport::MakeNode(1, std::string(100, 'w'), {}));
    }
    BinaryXml::Node nested = TestSupport::MakeNode(1, "nested", {});
    nested.attributes.push_back(
        TestSupport::MakeNode(BinaryXml::Type::kAttribute, "name", {'n', 0}));
    nested.children.push_back(TestSupport::MakeNode(3, "tail", {0x7F}));
    doc.root.children.push_back(std::move(nested));
    return doc;
}

std::vector<uint8_t> RandomBytes(std::size_t size, uint32_t seed, int alphabet) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> byte(0, alphabet - 1);
    std::vector<uint8_t> out(size);
    for (auto& b : out)
        b = static_cast<uint8_t>(byte(rng));
    return out;
}

}

TEST_CASE("AvsLz77::Compress matches avs2-core's compressor byte for byte") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    DllLoader avs_dll;
    REQUIRE(avs_dll.Load((dir + "/modules/avs2-core.dll").c_str()));
    AvsFuncs avs;
    REQUIRE(avs.Load(avs_dll));
    REQUIRE(AvsManager::Boot(avs));

    Cstream cs;
    cs.create = avs_dll.GetFunc<void* (*)(int)>(kCstreamCreate217, "cstream_create");
    cs.operate = avs_dll.GetFunc<int (*)(void*)>(kCstreamOperate217, "cstream_operate");
    cs.finish = avs_dll.GetFunc<int (*)(void*)>(kCstreamFinish217, "cstream_finish");
    cs.destroy = avs_dll.GetFunc<void (*)(void*)>(kCstreamDestroy217, "cstream_destroy");
    REQUIRE(cs.create != nullptr);
    REQUIRE(cs.operate != nullptr);
    REQUIRE(cs.finish != nullptr);
    REQUIRE(cs.destroy != nullptr);

    const std::vector<uint8_t> letters = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    const std::vector<uint8_t> letters_packed = {0xFF, 'a', 'b', 'c', 'd', 'e',
                                                 'f',  'g', 'h', 0,   0,   0};
    CHECK(DllCompress(cs, letters) == letters_packed);

    const std::vector<std::vector<uint8_t>> inputs = {
        RandomBytes(1, 1, 256),   RandomBytes(17, 2, 3),         RandomBytes(4095, 3, 256),
        RandomBytes(4096, 4, 4),  RandomBytes(4097, 5, 256),     RandomBytes(70000, 6, 256),
        RandomBytes(50000, 7, 6), std::vector<uint8_t>(9000, 0), RandomBytes(300000, 8, 2),
    };
    for (const auto& input : inputs) {
        INFO("input size " << input.size());
        const std::vector<uint8_t> ours = AvsLz77::Compress(input);
        const std::vector<uint8_t> dll = DllCompress(cs, input);
        CHECK(dll.size() == ours.size());
        CHECK(dll == ours);
    }
}

TEST_CASE("BinaryXml::Write matches avs2-core's binary property writer") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    DllLoader avs_dll;
    REQUIRE(avs_dll.Load((dir + "/modules/avs2-core.dll").c_str()));
    AvsFuncs avs;
    REQUIRE(avs.Load(avs_dll));
    REQUIRE(avs.property_create != nullptr);
    REQUIRE(avs.property_insert_read != nullptr);
    const auto part_write =
        avs_dll.GetFunc<PropertyPartWrite>(kPropertyPartWrite217, "property_part_write");
    REQUIRE(part_write != nullptr);

    struct Variant {
        uint8_t signature;
        uint8_t encoding;
    };
    constexpr std::array<Variant, 3> kVariants = {
        Variant{.signature = BinaryXml::kSixBitNames, .encoding = 0x00},
        Variant{.signature = BinaryXml::kSixBitNames, .encoding = 0x80},
        Variant{.signature = BinaryXml::kByteNames, .encoding = 0xA0},
    };
    for (const auto& [signature, encoding] : kVariants) {
        INFO("signature " << int{signature} << " encoding " << int{encoding});
        const auto ours = BinaryXml::Write(EveryTypeDocument(signature, encoding));
        REQUIRE(ours.has_value());
        const std::vector<uint8_t> dll =
            DllRewriteBinaryXml(avs, part_write, *ours, signature == BinaryXml::kByteNames);
        CHECK(dll.size() == ours->size());
        CHECK(dll == *ours);
    }
}
