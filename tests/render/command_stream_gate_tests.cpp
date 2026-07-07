#include <catch2/catch_test_macros.hpp>

#include "gpu_context.h"
#include "render/command_diff.h"
#include "render/command_list.h"
#include "render_backend.h"
#include "scene_support.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

uint32_t F32Bits(float v) {
    return std::bit_cast<uint32_t>(v);
}

struct GeoBlob {
    std::array<uint32_t, 16> words{};
};

GeoBlob MakeGeo(uint32_t prim, uint32_t flags, uint32_t tex_ref, const std::vector<float>& tail) {
    GeoBlob g;
    g.words.at(0) = prim;
    g.words.at(1) = flags;
    g.words.at(2) = tex_ref;
    g.words.at(3) = 0;
    for (std::size_t i = 0; i < tail.size(); i++) {
        g.words.at(4 + i) = F32Bits(tail.at(i));
    }
    return g;
}

struct Recording {
    Render::RenderCommandList tap;
    Render::RenderCommandList frame;
};

Recording RecordFixedFrame() {
    g_gpu = GpuContext{};
    Recording rec;
    g_gpu.cmd_list = &rec.tap;
    g_gpu.deferred_replay = true;
    const std::array<float, 16> matrix = {0.0625F, 0.125F, 0.1875F, 0.25F,  0.3125F, 0.375F,
                                          0.4375F, 0.5F,   0.5625F, 0.625F, 0.6875F, 0.75F,
                                          0.8125F, 0.875F, 0.9375F, 1.0F};
    std::copy_n(matrix.begin(), matrix.size(), std::begin(g_gpu.current_matrix));
    g_gpu.current_matrix_ready = true;

    AfpD3D9::SetLayer(0, 0, nullptr);
    AfpD3D9::SetBlend(0, 0, nullptr);

    std::vector<float> strip_src = {10.25F, 20.75F,  110.25F, 20.75F,
                                    10.25F, 120.75F, 110.25F, 120.75F};
    GeoBlob strip_geo = MakeGeo(5, 0x01, 0, {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F});
    AfpD3D9::SubmitGeometry(strip_src.data(), 4, strip_geo.words.data(), nullptr);

    AfpD3D9::SetBlend(1, 0, nullptr);
    std::vector<float> list_src;
    const std::array<float, 4> xs = {200.0F, 328.0F, 200.0F, 328.0F};
    const std::array<float, 4> ys = {40.0F, 40.0F, 104.0F, 104.0F};
    const std::array<std::size_t, 6> order = {0, 1, 2, 2, 1, 3};
    for (const std::size_t v : order) {
        list_src.push_back(v == 1 || v == 3 ? 0.5F : 0.0F);
        list_src.push_back(v >= 2 ? 0.5F : 0.0F);
        list_src.push_back(std::bit_cast<float>(uint32_t{0xFFB06040}));
        list_src.push_back(xs.at(v));
        list_src.push_back(ys.at(v));
    }
    GeoBlob list_geo = MakeGeo(4, 0x08 | 0x04 | 0x01, 0x00000005, {1.0F, 0.5F, 0.25F, 0.875F});
    AfpD3D9::SubmitGeometry(list_src.data(), 6, list_geo.words.data(), nullptr);

    AfpD3D9::SetMaskRegion(0, 1, 0, 0, 0, 0);
    AfpD3D9::SubmitGeometry(strip_src.data(), 4, strip_geo.words.data(), nullptr);
    AfpD3D9::SetMaskRegion(1, 1, 8, 16, 32, 64);
    AfpD3D9::SubmitGeometry(strip_src.data(), 4, strip_geo.words.data(), nullptr);
    AfpD3D9::SetMaskRegion(2, 1, 0, 0, 0, 0);

    AfpD3D9::SubmitGeometry(strip_src.data(), 2, strip_geo.words.data(), nullptr);

    AfpD3D9::LayerCommand(0, 0, 0, nullptr);
    AfpD3D9::LayerCommand((2U << 1U) | 1U, 0, 0, nullptr);

    g_gpu.cmd_list = nullptr;
    rec.frame = std::move(g_gpu.frame_cmds);
    g_gpu.frame_cmds.clear();
    return rec;
}

Recording RecordWideFrame() {
    g_gpu = GpuContext{};
    Recording rec;
    g_gpu.cmd_list = &rec.tap;
    g_gpu.deferred_replay = true;
    const std::array<float, 16> matrix = {0.0625F, 0.125F, 0.1875F, 0.25F,  0.3125F, 0.375F,
                                          0.4375F, 0.5F,   0.5625F, 0.625F, 0.6875F, 0.75F,
                                          0.8125F, 0.875F, 0.9375F, 1.0F};
    std::copy_n(matrix.begin(), matrix.size(), std::begin(g_gpu.current_matrix));
    g_gpu.current_matrix_ready = true;

    SceneSupport::DriveWideScene(6);

    g_gpu.cmd_list = nullptr;
    rec.frame = std::move(g_gpu.frame_cmds);
    g_gpu.frame_cmds.clear();
    return rec;
}

template <typename T> std::size_t CountOf(const Render::RenderCommandList& list) {
    std::size_t n = 0;
    for (const Render::RenderCommand& rc : list) {
        if (std::holds_alternative<T>(rc)) n++;
    }
    return n;
}

}

TEST_CASE("Recorder golden: the fixed frame formats to the committed golden") {
    const Recording rec = RecordFixedFrame();
    const std::string text = Render::FormatCommandList(rec.tap);
    const std::string path = std::string(R573_GOLDEN_DIR) + "/command_stream_frame.golden";
    const std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::ofstream out(path, std::ios::binary);
        out << text;
        FAIL("golden was missing - wrote the current recording; review it and commit");
    }
    std::stringstream have;
    have << in.rdbuf();
    CHECK(have.str() == text);
}

TEST_CASE("Recorder drops matte draws and keeps zero-prim draws only for replay") {
    const Recording rec = RecordFixedFrame();
    CHECK(CountOf<Render::DrawCmd>(rec.tap) == 3);
    CHECK(CountOf<Render::DrawCmd>(rec.frame) == 4);
    CHECK(CountOf<Render::LayerCmdCmd>(rec.tap) == 1);
    CHECK(CountOf<Render::LayerCmdCmd>(rec.frame) == 0);
    CHECK(CountOf<Render::MaskCmd>(rec.tap) == 3);
    CHECK(CountOf<Render::MaskCmd>(rec.frame) == 3);
}

TEST_CASE("Recorder is deterministic across identical frames") {
    const Recording a = RecordFixedFrame();
    const Recording b = RecordFixedFrame();
    CHECK(!Render::DiffCommandLists(a.tap, b.tap, 0.0001F).has_value());
    CHECK(!Render::DiffCommandLists(a.frame, b.frame, 0.0001F).has_value());
}

TEST_CASE("DiffCommandLists flags a perturbed recording") {
    const Recording a = RecordFixedFrame();
    Recording b = RecordFixedFrame();
    for (Render::RenderCommand& rc : b.tap) {
        if (auto* draw = std::get_if<Render::DrawCmd>(&rc)) {
            if (!draw->verts.empty()) {
                draw->verts.front().x += 5.0F;
                break;
            }
        }
    }
    const std::optional<Render::CommandDivergence> div =
        Render::DiffCommandLists(a.tap, b.tap, 0.0001F);
    REQUIRE(div.has_value());
    if (div.has_value()) CHECK(div->reason.find("vert 0 x") != std::string::npos);
}
TEST_CASE("Recorder golden: the wide scene formats to the committed golden") {
    const Recording rec = RecordWideFrame();
    const std::string text = Render::FormatCommandList(rec.tap);
    const std::string path = std::string(R573_GOLDEN_DIR) + "/command_stream_wide.golden";
    const std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::ofstream out(path, std::ios::binary);
        out << text;
        FAIL("golden was missing - wrote the current recording; review it and commit");
    }
    std::stringstream have;
    have << in.rdbuf();
    CHECK(have.str() == text);
}

TEST_CASE("Wide scene records deterministically with matching tap and frame draws") {
    const Recording a = RecordWideFrame();
    const Recording b = RecordWideFrame();
    CHECK(!Render::DiffCommandLists(a.tap, b.tap, 0.0001F).has_value());
    CHECK(CountOf<Render::DrawCmd>(a.tap) == 8);
    CHECK(CountOf<Render::DrawCmd>(a.frame) == 8);
    CHECK(CountOf<Render::SetBlendCmd>(a.tap) == 4);
    CHECK(CountOf<Render::SetLayerCmd>(a.tap) == 3);
}
