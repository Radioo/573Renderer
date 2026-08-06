#pragma once

#include <string>

namespace Panels {

void RenderIfsPicker();

void RenderScenePane();
void RenderInspectorPane();
void RenderTimelineDock();

void RenderPropertiesTab();
void RenderRenderTabModern();
void RenderRenderTabDdr();
void RenderLiveTab();

void RenderRendererView();

void RenderQproTabBody();

namespace Scene {

struct Selection {
    enum class Kind : int { None, Layer, Child };
    Kind kind = Kind::None;
    std::string path;
    std::string name;
};

const Selection& Current();
void Select(Selection s);
void Reset();

}

}
