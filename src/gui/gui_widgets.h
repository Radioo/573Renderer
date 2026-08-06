#pragma once

namespace Gui {

bool Segmented(const char* id, const char* const* items, int count, int* current);

void SectionHeader(const char* icon, const char* label, const char* suffix);

}
