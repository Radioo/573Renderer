#include "encode/host/encode_session.h"
#include "preview/preview_channel.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int Serve(const std::string& pipe_name) {
    auto server = PreviewChannel::Listen(pipe_name);
    if (!server) {
        std::fprintf(stderr, "573Encoder: %s\n", server.error().c_str());
        return 1;
    }
    auto accepted = PreviewChannel::Accept(*server, INFINITE);
    if (!accepted) {
        std::fprintf(stderr, "573Encoder: %s\n", accepted.error().c_str());
        return 1;
    }
    EncodeHost::Session session;
    while (true) {
        auto request = PreviewChannel::Receive(*server, INFINITE);
        if (!request) return 0;
        const std::vector<uint8_t> reply = session.Handle(*request);
        if (!PreviewChannel::Send(*server, reply)) return 0;
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("usage: 573Encoder <pipe name>\n", stderr);
        return 2;
    }
    try {
        return Serve(argv[1]);
    } catch (...) {
        std::fputs("573Encoder: unexpected exception\n", stderr);
        return 1;
    }
}
