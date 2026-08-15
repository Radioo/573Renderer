#pragma once

#include <map>
#include <string>
#include <vector>

namespace PresetGolden {

struct Recording {
    std::string preset;
    std::string build;
    int choice = -1;
    int frames = 0;
    std::vector<std::string> setup;
    std::vector<std::string> hashes;
    std::vector<std::string> hashes_no_transform;
    std::map<int, std::vector<std::string>> detail;
};

std::vector<std::string> WithoutModelTransforms(const std::vector<std::string>& calls);

std::string FormatFloat(float value);

std::string FormatCall(const std::string& name, const std::vector<std::string>& args);

std::string HashFrame(const std::vector<std::string>& calls);

std::string FixtureName(const std::string& preset, int choice);

bool Parse(const std::string& text, Recording& out, std::string& err);

}
