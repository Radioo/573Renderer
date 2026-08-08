#include "afp_ddr_txp2.h"

#include "afp_ddr_funcs.h"
#include "avs_funcs.h"
#include "formats/txp2.h"

#include "support/log.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace DdrAfp {

namespace {

constexpr int kSeekSet = 0;
constexpr int kSeekEnd = 2;
constexpr int kOpenReadOnly = 0;
constexpr int kOpenMode = 420;
constexpr int kMaxCreateLevel = 127;
constexpr int kCreateLevelBias = 10;
constexpr size_t kTextureHeaderBytes = 64;

uint16_t ReadU16(const std::vector<uint8_t>& b, size_t off, bool be) {
    if (off + 2 > b.size()) return 0;
    const uint8_t* p = b.data() + off;
    if (be) return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
    return (uint16_t)(((uint16_t)p[1] << 8) | p[0]);
}

uint32_t ReadU32(const std::vector<uint8_t>& b, size_t off, bool be) {
    if (off + 4 > b.size()) return 0;
    const uint8_t* p = b.data() + off;
    if (be) return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}

void DecodeTextures(AvsFuncs& avs, Txp2Loaded& pkg) {
    const bool be = pkg.package.big_endian;
    pkg.textures.clear();
    pkg.textures.reserve(pkg.package.textures.size());
    for (const auto& t : pkg.package.textures) {
        Txp2Texture tex;
        tex.name = t.name;
        if (t.file_offset == 0 || (size_t)t.file_offset + t.size > pkg.core.size()) {
            LOG("DDR", "  texture '%s' lies outside the file", t.name.c_str());
            pkg.textures.push_back(std::move(tex));
            continue;
        }

        const uint8_t* blob = pkg.core.data() + t.file_offset;
        std::vector<uint8_t> image;
        std::string err;
        if (pkg.package.textures_lz_compressed) {
            if (!InflateAvsLz(avs, blob, t.size, image, err)) {
                LOG("DDR", "  texture '%s' inflate failed: %s", t.name.c_str(), err.c_str());
                pkg.textures.push_back(std::move(tex));
                continue;
            }
        } else {
            image.assign(blob, blob + t.size);
        }

        if (image.size() < kTextureHeaderBytes) {
            LOG("DDR", "  texture '%s' image is smaller than its header", t.name.c_str());
            pkg.textures.push_back(std::move(tex));
            continue;
        }
        tex.width = ReadU16(image, 16, be);
        tex.height = ReadU16(image, 18, be);
        tex.format = image[20];
        tex.pixels.assign(image.begin() + kTextureHeaderBytes, image.end());
        LOG("DDR", "  texture[%zu] '%s' %dx%d fmt=%d (%zu bytes of pixels)", pkg.textures.size(),
            tex.name.c_str(), tex.width, tex.height, tex.format, tex.pixels.size());
        pkg.textures.push_back(std::move(tex));
    }
}

void ApplyByteOrderFixups(const AfpDdrFuncs& afp, Txp2Loaded& pkg) {
    if (afp.afp_check_src == nullptr) return;
    const uint32_t table = pkg.package.appended_block_offset;
    if (table == 0) {
        LOG("DDR", "package has no appended byte-order block; afp data is assumed native");
        return;
    }

    const size_t count = pkg.package.afp_entries.size();
    const bool be = pkg.package.big_endian;
    size_t applied = 0;
    for (size_t i = 0; i < count; i++) {
        const size_t rec = (size_t)table + (i * 12);
        if (rec + 12 > pkg.core.size()) break;
        const uint32_t size = ReadU32(pkg.core, rec + 4, be);
        const uint32_t off = ReadU32(pkg.core, rec + 8, be);
        const size_t padded = ((size_t)size + 3) & ~(size_t)3;
        if (off == 0 || padded == 0 || (size_t)off + padded > pkg.core.size()) continue;

        const auto& entry = pkg.package.afp_entries[i];
        if (entry.data_offset == 0 || entry.data_offset >= pkg.core.size()) continue;

        afp.afp_check_src(pkg.core.data() + entry.data_offset, pkg.core.data() + off);
        applied++;
    }
    LOG("DDR", "afp_check_src applied to %zu of %zu streams (byte-order block at %#x)", applied,
        count, table);
}

}

bool InflateAvsLz(AvsFuncs& avs, const uint8_t* blob, size_t blob_size, std::vector<uint8_t>& out,
                  std::string& err) {
    if (blob_size < 8) {
        err = "avslz blob shorter than its 8-byte header";
        return false;
    }
    const uint32_t usize =
        ((uint32_t)blob[0] << 24) | ((uint32_t)blob[1] << 16) | ((uint32_t)blob[2] << 8) | blob[3];
    const uint32_t csize =
        ((uint32_t)blob[4] << 24) | ((uint32_t)blob[5] << 16) | ((uint32_t)blob[6] << 8) | blob[7];
    if (usize == 0) {
        err = "avslz blob declares a zero uncompressed size";
        return false;
    }
    out.assign(usize, 0);

    if (csize == 0) {
        if (blob_size < 8 + (size_t)usize) {
            err = "uncompressed avslz blob is truncated";
            return false;
        }
        memcpy(out.data(), blob + 8, usize);
        return true;
    }

    if ((avs.avs_cstream_create == nullptr) || (avs.avs_cstream_execute == nullptr)) {
        err = "avs cstream inflate is not available on this avs generation";
        return false;
    }
    if (blob_size < 8 + (size_t)csize) {
        err = "compressed avslz blob is truncated";
        return false;
    }

    void* ctx = avs.avs_cstream_create(0);
    if (ctx == nullptr) {
        err = "avs_cstream_create failed";
        return false;
    }
    auto* slots = static_cast<void**>(ctx);
    slots[0] = out.data();
    slots[1] = const_cast<uint8_t*>(blob + 8);
    slots[2] = reinterpret_cast<void*>(static_cast<uintptr_t>(usize));
    slots[3] = reinterpret_cast<void*>(static_cast<uintptr_t>(csize));

    const bool ok = avs.avs_cstream_execute(ctx) != 0;
    if (avs.avs_cstream_finish != nullptr) avs.avs_cstream_finish(ctx);
    if (avs.avs_cstream_destroy != nullptr) avs.avs_cstream_destroy(ctx);
    if (!ok) {
        err = "avs cstream INFLATE failed";
        return false;
    }
    return true;
}

bool ReadAvsFile(AvsFuncs& avs, const std::string& vfs_path, std::vector<uint8_t>& out,
                 std::string& err) {
    if ((avs.avs_fs_open == nullptr) || (avs.avs_fs_read == nullptr) ||
        (avs.avs_fs_lseek == nullptr)) {
        err = "avs filesystem entry points are not resolved";
        return false;
    }

    int const desc = avs.avs_fs_open(vfs_path.c_str(), kOpenReadOnly, kOpenMode);
    if (desc <= 0) {
        err = "avs_fs_open failed for " + vfs_path;
        return false;
    }

    int const size = avs.avs_fs_lseek(desc, 0, kSeekEnd);
    avs.avs_fs_lseek(desc, 0, kSeekSet);
    if (size <= 0) {
        if (avs.avs_fs_close != nullptr) avs.avs_fs_close(desc);
        err = "avs_fs_lseek reported no data for " + vfs_path;
        return false;
    }

    out.assign(static_cast<size_t>(size), 0);
    int read_total = 0;
    while (read_total < size) {
        int const got = avs.avs_fs_read(desc, out.data() + read_total, size - read_total);
        if (got <= 0) break;
        read_total += got;
    }
    if (avs.avs_fs_close != nullptr) avs.avs_fs_close(desc);

    if (read_total != size) {
        err = "short read on " + vfs_path + " (" + std::to_string(read_total) + " of " +
              std::to_string(size) + ")";
        return false;
    }
    return true;
}

bool LoadTxp2Package(AvsFuncs& avs, const AfpDdrFuncs& afp, const std::string& vfs_path,
                     int package_slot, Txp2Loaded& out, std::string& err) {
    if (!afp.HasTxp2PackageApi()) {
        err = "this afp build has no afp_stream_create_call / afp_layer_create";
        return false;
    }

    if (!ReadAvsFile(avs, vfs_path, out.core, err)) return false;
    LOG("DDR", "read package %s (%zu bytes)", vfs_path.c_str(), out.core.size());

    if (!Txp2::Parse(out.core, out.package, err)) return false;
    LOG("DDR",
        "TXP2 flags=%#x header=%u core=%u endian=%s bo_block=%#x: %zu afp, %zu textures, %zu cells",
        out.package.flags, out.package.header_size, out.package.core_size,
        out.package.big_endian ? "big" : "little", out.package.appended_block_offset,
        out.package.afp_entries.size(), out.package.textures.size(), out.package.cells.size());

    ApplyByteOrderFixups(afp, out);

    if (afp.afp_set_create_level != nullptr) {
        afp.afp_set_create_level(std::min(package_slot + kCreateLevelBias, kMaxCreateLevel));
    }

    DecodeTextures(avs, out);

    out.clips.clear();
    out.clips.reserve(out.package.afp_entries.size());
    for (const auto& entry : out.package.afp_entries) {
        if (entry.data_offset == 0 || entry.data_offset >= out.core.size()) {
            LOG("DDR", "  skipping afp '%s': data offset %#x is outside the core",
                entry.name.c_str(), entry.data_offset);
            continue;
        }
        void* blob = out.core.data() + entry.data_offset;
        uint32_t const stream_id = afp.afp_stream_create_call(blob);
        if (stream_id == 0) {
            LOG("DDR", "  afp_stream_create_call failed for '%s'", entry.name.c_str());
            continue;
        }
        if (afp.afp_stream_set_name_call != nullptr) {
            afp.afp_stream_set_name_call(stream_id, entry.name.c_str());
        }
        LOG("DDR", "  stream[%zu] '%s' -> %#x (size %u)", out.clips.size(), entry.name.c_str(),
            stream_id, entry.size);
        out.clips.push_back({.name = entry.name, .stream_id = stream_id});
    }

    if (out.clips.empty()) {
        err = "package " + vfs_path + " produced no afp streams";
        return false;
    }
    return true;
}

}
