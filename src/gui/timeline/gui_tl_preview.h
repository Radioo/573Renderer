#pragma once

#include <string>
#include <vector>

namespace Panels::Timeline {

void RequestPreview(const std::string& asset, const std::string& animation,
                    const std::vector<std::string>& hidden_parts);

void DrawPreviewStrip(const std::string& key);

void ForgetPreview();

}
