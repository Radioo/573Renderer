#include "formats/ifs_digest.h"

#include <md5.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Ifs::Detail {

namespace {

constexpr std::size_t kZeroChunk = 256;

Digest Finish(MD5& md5) {
    Digest digest{};
    md5.getHash(digest.data());
    return digest;
}

}

Digest Md5(std::span<const uint8_t> bytes) {
    MD5 md5;
    md5.add(bytes.data(), bytes.size());
    return Finish(md5);
}

Digest ManifestMd5(std::span<const uint8_t> file, std::size_t header_end, std::size_t data_offset) {
    MD5 md5;
    const std::size_t present_end = std::min(data_offset, file.size());
    if (present_end > header_end) {
        md5.add(file.subspan(header_end, present_end - header_end).data(),
                present_end - header_end);
    }
    const std::array<uint8_t, kZeroChunk> zeros{};
    for (std::size_t left = data_offset - std::max(present_end, header_end); left > 0;) {
        const std::size_t chunk = std::min(left, kZeroChunk);
        md5.add(zeros.data(), chunk);
        left -= chunk;
    }
    return Finish(md5);
}

}
