#pragma once

namespace Log {

using Sink = void (*)(const char* tag, const char* message, void* user);

void Init();
void Shutdown();
void SetSink(Sink sink, void* user);

void Write(const char* tag, const char* fmt, ...) noexcept;

}

#define LOG(tag, fmt, ...) Log::Write(tag, fmt, ##__VA_ARGS__)

#define LOG_ONCE(tag, fmt, ...)                                                                    \
    do {                                                                                           \
        static bool once_ = false;                                                                 \
        if (!once_) {                                                                              \
            once_ = true;                                                                          \
            LOG(tag, fmt, ##__VA_ARGS__);                                                          \
        }                                                                                          \
    } while (0)
