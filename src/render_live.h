#pragma once

#include <cstdint>

struct AfpFuncs;

namespace RenderLive {

bool HandleSeekRequest(int seek_to_frame, AfpFuncs& afp);
bool HandlePauseRequest(bool paused_value, AfpFuncs& afp);

void NotifySeek();

void ResetPauseDefend();

void PublishLiveState(AfpFuncs& afp, uint32_t stream_id, int frames_since_switch, bool exporting);

}
