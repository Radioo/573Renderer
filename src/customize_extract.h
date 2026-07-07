#pragma once

#include <string>

namespace CustomizeExtract {

struct Status {
    bool running = false;
    bool finished = false;
    int total = 0;
    int done = 0;
    int written = 0;
    int optimized = 0;
    int failed = 0;
    long long bytes_in = 0;
    long long bytes_out = 0;
    std::string output_dir;
    std::string current;
    std::string error;
};

void Start(const std::string& folder);

bool IsRunning();
Status GetStatus();

}
