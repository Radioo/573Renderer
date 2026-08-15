#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/frame_state.h"

#include <vector>

namespace Preset::Eval {

void ApplySpriteDraw(const Doc::Clip& clip, const Doc::SpriteDraw& command, int frame,
                     SpriteSlot& slot, bool with_keys);

void ApplySpriteAnimate(const Doc::Clip& clip, const Doc::SpriteAnimate& command, int frame,
                        SpriteSlot& slot, bool abuts_previous, bool with_keys);

void ApplySpriteScroll(const Doc::Clip& clip, const Doc::SpriteScroll& command, int frame,
                       SpriteSlot& slot, bool with_keys);

std::vector<int> SpriteDrawOrder(const std::vector<SpriteSlot>& sprites);

}
