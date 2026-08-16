#pragma once

#include "editor/preset_editor_state.h"
#include "preset/doc/preset_validate.h"

#include <string>
#include <vector>

namespace Panels::Timeline {

void ResetClipModal();

void ResetPalette();

void ResetDocumentModal();

void ResetProblems();

void RequestClipModal(std::string clip_id);

void RenderClipModal();

void RequestPalette(std::string track_id, int frame);

void RenderPalette();

void RequestTrackModal(std::string selected_track_id);

void RenderTrackModal();

void RequestDocumentModal();

void RenderDocumentModal();

void RequestProblems();

void RenderProblems();

const std::vector<Preset::Doc::Problem>& CurrentProblems();

int ErrorCount();

}
