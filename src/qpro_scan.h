#pragma once

#include "qpro_dll.h"

#include <cstdint>
#include <string>
#include <vector>

namespace QproExtract {

struct ScanPart {
    int cat = 0;
    int idx = 0;
    std::string label;
    std::string ifs;
    std::string date;
    bool exists = false;
};

struct ScanResult {
    bool running = false;
    bool done = false;
    std::string error;
    std::vector<ScanPart> parts;
    int generation = 0;
};

void RunScan(const std::string& game_dir);
void MarkScanRunning();
ScanResult GetScanResult();

using PartSelection = QproModel::PartSelection;

}
