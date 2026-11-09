#include "preview/host/preview_host_session.h"
#include "preview/preview_channel.h"
#include "support/log.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int Serve(const std::string& pipe_name) {
    auto server = PreviewChannel::Listen(pipe_name);
    if (!server) {
        LOG("PreviewHost", "%s", server.error().c_str());
        return 1;
    }
    auto accepted = PreviewChannel::Accept(*server, INFINITE);
    if (!accepted) {
        LOG("PreviewHost", "%s", accepted.error().c_str());
        return 1;
    }
    PreviewHost::Session session;
    while (true) {
        auto request = PreviewChannel::Receive(*server, INFINITE);
        if (!request) {
            LOG("PreviewHost", "editor gone: %s", request.error().c_str());
            return 0;
        }
        const std::vector<uint8_t> reply = session.Handle(*request);
        auto sent = PreviewChannel::Send(*server, reply);
        if (!sent) {
            LOG("PreviewHost", "reply not sent: %s", sent.error().c_str());
            return 0;
        }
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("usage: preview_host <pipe name>\n", stderr);
        return 2;
    }
    try {
        return Serve(argv[1]);
    } catch (...) {
        std::fputs("preview_host: unexpected exception\n", stderr);
        return 1;
    }
}
