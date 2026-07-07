#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace AppWindow {
HWND Create(int w, int h);

void Resize(HWND hwnd, int w, int h);

bool PumpMessages();

bool IsRunning();
void RequestClose();

void SetRenderRtSize(int w, int h);
}
