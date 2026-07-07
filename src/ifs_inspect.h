#pragma once

#include "afp_funcs.h"
#include "avs_funcs.h"
#include "state/app_state.h"
#include <string>
#include <vector>

namespace IfsInspect {

void LoadDictionary(const AvsFuncs& avs, App::IfsConfig& cfg);

int CountExpectedTextures(const AvsFuncs& avs);

struct AtlasFilter {
    unsigned int mag_filter_d3d;
    unsigned int min_filter_d3d;
};
std::vector<AtlasFilter> ReadAtlasFilters(const AvsFuncs& avs,
                                          const char* mount_root = "/afp/packages");

std::vector<App::CompanionIfs> FindCompanions(const std::string& base_ifs_path);

void ProbeSlots(const AfpFuncs& afp, uint32_t stream_id, App::IfsConfig& cfg);

}
