#pragma once

#include <string>

namespace ArcExtract {

struct Status {
    bool running = false;
    bool finished = false;
    int total_arcs = 0;
    int done_arcs = 0;
    int entries_written = 0;
    int failed_arcs = 0;
    std::string output_dir;
    std::string current;
    std::string error;
};

void Start(const std::string& folder);

bool IsRunning();

Status GetStatus();

}
