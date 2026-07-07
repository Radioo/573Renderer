#include "support/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdarg>
#include <cstdio>

namespace Log {

namespace {

struct LogState {
    std::FILE* file = nullptr;
    Sink sink = nullptr;
    void* sink_user = nullptr;
};

LogState& S() {
    static LogState s;
    return s;
}

}

void Init() {
    AllocConsole();
    std::FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);

    fopen_s(&S().file, "renderer.log", "w");
    if (S().file != nullptr) {
        setvbuf(S().file, nullptr, _IONBF, 0);
    }
    setvbuf(stdout, nullptr, _IONBF, 0);
}

void Shutdown() {
    if (S().file != nullptr) {
        std::fclose(S().file);
        S().file = nullptr;
    }
}

void SetSink(Sink sink, void* user) {
    S().sink = sink;
    S().sink_user = user;
}

void Write(const char* tag, const char* fmt, ...) noexcept {
    std::array<char, 2048> message{};
    va_list args = nullptr;
    va_start(args, fmt);
    vsnprintf(message.data(), message.size(), fmt, args);
    va_end(args);

    if (S().sink != nullptr) {
        S().sink(tag, message.data(), S().sink_user);
        return;
    }

    std::printf("[%s] %s\n", tag, message.data());
    if (S().file != nullptr) {
        std::fprintf(S().file, "[%s] %s\n", tag, message.data());
        std::fflush(S().file);
    }
}

}
