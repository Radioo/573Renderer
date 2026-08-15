#include "gc2d/gc_sprite.h"

#include "formats/gcanim.h"
#include "formats/sysidx.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace Gc2d {

namespace {

void AppendCell(const SysIdx::Package& index, const SpriteDraw& sprite, float x,
                std::vector<GcAnim::DrawNode>& out) {
    const auto it = index.cell_names.find(sprite.name);
    if (it == index.cell_names.end()) return;
    if ((std::size_t)it->second >= index.cells.size()) return;
    const SysIdx::Cell& cell = index.cells[(std::size_t)it->second];
    GcAnim::DrawNode node;
    node.cell = it->second;
    node.x = x;
    node.y = sprite.y;
    node.w = (float)cell.w;
    node.h = (float)cell.h;
    node.pivot_x = node.x;
    node.pivot_y = node.y;
    node.blend = sprite.blend;
    out.push_back(node);
}

void AppendAnimation(const SysIdx::Package& index, const SpriteDraw& sprite, float x,
                     std::vector<GcAnim::DrawNode>& out) {
    const auto it = index.animation_names.find(sprite.name);
    if (it == index.animation_names.end()) return;
    const int length = SysIdx::AnimationLength(index, it->second);
    const int frame = GcAnim::ResolveFrame((int)sprite.time, length, sprite.timing);
    if (frame < 0) return;
    std::vector<std::size_t> skip_children;
    std::vector<int> skip_cells;
    for (const std::string& part : sprite.skip_parts) {
        const auto child = index.animation_names.find(part);
        if (child != index.animation_names.end()) skip_children.push_back(child->second);
        const auto cell = index.cell_names.find(part);
        if (cell != index.cell_names.end()) skip_cells.push_back(cell->second);
    }
    std::vector<GcAnim::DrawNode> scratch;
    GcAnim::Evaluate(index, it->second, frame, x, sprite.y, scratch,
                     GcAnim::SkipSet{.children = skip_children, .cells = skip_cells});
    out.insert(out.end(), scratch.begin(), scratch.end());
}

void FadeNodes(std::vector<GcAnim::DrawNode>& nodes, std::size_t from, float alpha) {
    for (std::size_t i = from; i < nodes.size(); i++)
        nodes[i].alpha *= alpha;
}

void ScaleNodes(std::vector<GcAnim::DrawNode>& nodes, std::size_t from, float scale,
                const std::array<float, 2>& pivot) {
    if (scale == 1.0F) return;
    for (std::size_t i = from; i < nodes.size(); i++) {
        GcAnim::DrawNode& node = nodes[i];
        node.x = pivot[0] + ((node.x - pivot[0]) * scale);
        node.y = pivot[1] + ((node.y - pivot[1]) * scale);
        node.pivot_x = pivot[0] + ((node.pivot_x - pivot[0]) * scale);
        node.pivot_y = pivot[1] + ((node.pivot_y - pivot[1]) * scale);
        node.w *= scale;
        node.h *= scale;
    }
}

}

std::array<float, 2> ScaleFactors(const Canvas& canvas, int target_width, int target_height) {
    const float width = (canvas.width > 0) ? (float)canvas.width : 1.0F;
    const float height = (canvas.height > 0) ? (float)canvas.height : 1.0F;
    return {(float)target_width / width, (float)target_height / height};
}

std::array<float, 2> PivotFor(const Canvas& canvas, float x, float y) {
    return {x + ((float)canvas.width * 0.5F), y + ((float)canvas.height * 0.5F)};
}

float ScrollOffset(const SpriteDraw& sprite) {
    if (sprite.scroll_wrap <= 0.0F) return 0.0F;
    return std::fmod(sprite.scroll_offset + (sprite.time * sprite.scroll_x), sprite.scroll_wrap);
}

int SpriteLength(const SysIdx::Package& index, const SpriteDraw& sprite) {
    const auto it = index.animation_names.find(sprite.name);
    if (it == index.animation_names.end()) return 0;
    return SysIdx::AnimationLength(index, it->second);
}

void AppendNodes(const SysIdx::Package& index, const SpriteDraw& sprite, const Canvas& canvas,
                 std::vector<GcAnim::DrawNode>& out) {
    const std::size_t from = out.size();
    const float x = sprite.x - ScrollOffset(sprite);
    if (sprite.animated) {
        AppendAnimation(index, sprite, x, out);
    } else {
        AppendCell(index, sprite, x, out);
    }
    FadeNodes(out, from, sprite.alpha);
    ScaleNodes(out, from, sprite.scale, PivotFor(canvas, sprite.x, sprite.y));
}

}
