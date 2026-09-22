#include "txp2_dump.h"

#include "afp_ddr_funcs.h"
#include "afp_ddr_txp2.h"
#include "app_globals.h"
#include "document/inputs.h"
#include "formats/afp_animation.h"
#include "formats/txp2.h"
#include "avs_boot.h"
#include "backend/afp_profiles.h"
#include "engine_dlls.h"
#include "game_profile.h"
#include "support/dll_loader.h"
#include "support/log.h"
#include "support/png_write.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace Txp2Dump {

namespace {

namespace fs = std::filesystem;

constexpr const char* kMount = "/txp2dump";

int BytesPerPixel(int format) {
    switch (format) {
    case 15:
        return 3;
    case 16:
    case 17:
    case 21:
    case 22:
        return 4;
    default:
        return 0;
    }
}

bool ToBgra(const DdrAfp::Txp2Texture& tex, std::vector<uint8_t>& out) {
    const int bpp = BytesPerPixel(tex.format);
    if (bpp == 0 || tex.width <= 0 || tex.height <= 0) return false;
    const size_t texels = (size_t)tex.width * (size_t)tex.height;
    if (tex.pixels.size() < texels * (size_t)bpp) return false;
    out.assign(texels * 4, 0);
    for (size_t i = 0; i < texels; i++) {
        const uint8_t* s = tex.pixels.data() + (i * (size_t)bpp);
        uint8_t* d = out.data() + (i * 4);
        if (bpp == 3) {
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d[3] = 0xFF;
            continue;
        }
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];
        d[3] = s[3];
    }
    return true;
}

bool WriteCrop(const std::string& path, const std::vector<uint8_t>& bgra, int src_w, int src_h,
               int x0, int y0, int x1, int y1) {
    if (x1 <= x0 || y1 <= y0 || x1 > src_w || y1 > src_h || x0 < 0 || y0 < 0) return false;
    const int w = x1 - x0;
    const int h = y1 - y0;
    std::vector<uint8_t> crop((size_t)w * (size_t)h * 4, 0);
    for (int y = 0; y < h; y++) {
        const uint8_t* s = bgra.data() + (((size_t)(y0 + y) * (size_t)src_w + (size_t)x0) * 4);
        uint8_t* d = crop.data() + ((size_t)y * (size_t)w * 4);
        std::copy(s, s + ((size_t)w * 4), d);
    }
    return Support::WritePngBGRA(path, crop.data(), w, h);
}

std::string SafeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        const bool keep = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                          (c >= 'a' && c <= 'z') || c == '_' || c == '-';
        out += keep ? c : '_';
    }
    return out;
}

std::string JsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char ch : text) {
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += ch;
        } else if (static_cast<unsigned char>(ch) < 0x20) {
            out += ' ';
        } else {
            out += ch;
        }
    }
    return out;
}

AfpDdrFuncs ResolveByteOrderFix(DllLoader& afp_dll) {
    AfpDdrFuncs funcs;
    funcs.Load(afp_dll);
    return funcs;
}

std::string InputsJson(const DdrAfp::Txp2Loaded& pkg) {
    const std::map<uint16_t, std::string> no_shape_images;
    std::string json = "{\n  \"animations\": [\n";
    bool first = true;
    for (const Txp2::AfpEntry& entry : pkg.package.afp_entries) {
        if (entry.data_offset == 0 || entry.data_offset + entry.size > pkg.core.size()) continue;
        const std::span<const uint8_t> bytes(pkg.core.data() + entry.data_offset, entry.size);
        auto animation = AfpAnimation::Read(bytes);
        if (!animation) {
            LOG("Txp2Dump", "  animation '%s' did not parse: %s", entry.name.c_str(),
                animation.error().c_str());
            continue;
        }
        const Document::InputSurface surface = Document::Inputs(*animation, no_shape_images);
        if (!first) json += ",\n";
        first = false;
        json += R"(    {"name": ")" + JsonEscape(entry.name) + R"(", "names": [)";
        for (std::size_t i = 0; i < surface.names.size(); i++) {
            const Document::InputSlot& slot = surface.names[i];
            if (i != 0) json += ", ";
            json += R"({"name": ")" + JsonEscape(slot.name) + R"(", "depth": )" +
                    std::to_string(slot.depth) + R"(, "frame": )" + std::to_string(slot.frame) +
                    R"(, "places": )" + std::to_string(slot.places) + R"(, "frames": )" +
                    std::to_string(slot.frames) + "}";
        }
        json += R"(], "labels": [)";
        for (std::size_t i = 0; i < surface.labels.size(); i++) {
            if (i != 0) json += ", ";
            json += R"({"name": ")" + JsonEscape(surface.labels[i].name) + R"(", "frame": )" +
                    std::to_string(surface.labels[i].frame) + "}";
        }
        json += "]}";
    }
    json += "\n  ]\n}\n";
    return json;
}

const AfpProfiles::AfpConfig* ResolveConfig(const std::string& game_dir) {
    const GameProfile::Profile* profile = GameProfile::AutoDetect(game_dir);
    if (profile == nullptr) return nullptr;
    return AfpProfiles::For(profile->slug);
}

int WriteCells(const DdrAfp::Txp2Loaded& pkg, const std::vector<std::vector<uint8_t>>& bgra,
               const fs::path& out) {
    int written = 0;
    for (const Txp2::CellName& named : pkg.package.cell_names) {
        if (named.cell_index >= pkg.package.cells.size()) continue;
        const Txp2::CellEntry& cell = pkg.package.cells[named.cell_index];
        if ((size_t)cell.texture_index >= bgra.size()) continue;
        const auto tex = static_cast<size_t>(cell.texture_index);
        if (bgra[tex].empty()) continue;
        const DdrAfp::Txp2Texture& t = pkg.textures[tex];
        const std::string path = (out / ("cell_" + SafeName(named.name) + ".png")).string();
        if (WriteCrop(path, bgra[tex], t.width, t.height, cell.x0 / 2, cell.y0 / 2, cell.x1 / 2,
                      cell.y1 / 2)) {
            written++;
        } else {
            LOG("Txp2Dump", "  cell '%s' rect %u,%u..%u,%u is not inside texture %d (%dx%d)",
                named.name.c_str(), cell.x0, cell.y0, cell.x1, cell.y1, (int)cell.texture_index,
                t.width, t.height);
        }
    }
    return written;
}

bool BootEngine(const std::string& game_dir) {
    const AfpProfiles::AfpConfig* cfg = ResolveConfig(game_dir);
    if (cfg == nullptr) {
        LOG("Txp2Dump", "no AFP profile for game dir '%s'", game_dir.c_str());
        return false;
    }
    const std::string dll_dir = EngineDlls::DiscoverDllDir(game_dir, *cfg);
    if (dll_dir.empty()) {
        LOG("Txp2Dump", "could not find %s under '%s'", cfg->avs_dll, game_dir.c_str());
        return false;
    }
    if (!EngineDlls::Load(g_engine, dll_dir, *cfg, true)) {
        LOG("Txp2Dump", "engine DLLs failed to load from '%s'", dll_dir.c_str());
        return false;
    }
    if (!AvsManager::Boot(g_engine.avs)) {
        LOG("Txp2Dump", "AVS boot failed");
        return false;
    }
    return true;
}

bool OpenPackage(const std::string& package_path, DdrAfp::Txp2Loaded& pkg) {
    const fs::path native(package_path);
    if (!AvsManager::MountFsRoot(g_engine.avs, kMount, native.parent_path().string())) {
        LOG("Txp2Dump", "could not mount '%s'", native.parent_path().string().c_str());
        return false;
    }
    std::string err;
    const std::string vfs = std::string(kMount) + "/" + native.filename().string();
    if (!DdrAfp::ReadTxp2Core(g_engine.avs, vfs, pkg, err)) {
        LOG("Txp2Dump", "load failed for %s: %s", package_path.c_str(), err.c_str());
        return false;
    }
    DdrAfp::DecodeTxp2Textures(g_engine.avs, pkg);
    return true;
}

int WriteTextures(const DdrAfp::Txp2Loaded& pkg, const fs::path& out,
                  std::vector<std::vector<uint8_t>>& bgra) {
    int written = 0;
    for (size_t i = 0; i < pkg.textures.size(); i++) {
        const DdrAfp::Txp2Texture& t = pkg.textures[i];
        if (!ToBgra(t, bgra[i])) {
            LOG("Txp2Dump", "  texture '%s' fmt=%d %dx%d is not a supported layout", t.name.c_str(),
                t.format, t.width, t.height);
            continue;
        }
        const std::string path = (out / (SafeName(t.name) + ".png")).string();
        if (Support::WritePngBGRA(path, bgra[i].data(), t.width, t.height)) {
            written++;
            LOG("Txp2Dump", "  %s (%dx%d fmt=%d)", path.c_str(), t.width, t.height, t.format);
        }
    }
    return written;
}

int WriteInputs(DdrAfp::Txp2Loaded& pkg, const fs::path& out) {
    if (pkg.package.afp_entries.empty()) return 0;
    const AfpDdrFuncs fix = ResolveByteOrderFix(g_engine.afp_dll);
    DdrAfp::ApplyTxp2ByteOrder(fix, pkg);
    const std::string json = InputsJson(pkg);
    std::ofstream f(out / "inputs.json", std::ios::binary | std::ios::trunc);
    if (!f) return 0;
    f.write(json.data(), static_cast<std::streamsize>(json.size()));
    return 1;
}

}

int Run(const std::string& game_dir, const std::string& package_path, const std::string& out_dir) {
    if (!BootEngine(game_dir)) return 2;

    DdrAfp::Txp2Loaded pkg;
    if (!OpenPackage(package_path, pkg)) return 2;

    std::error_code ec;
    const fs::path out(out_dir);
    fs::create_directories(out, ec);

    std::vector<std::vector<uint8_t>> bgra(pkg.textures.size());
    int written = WriteTextures(pkg, out, bgra);
    written += WriteCells(pkg, bgra, out);
    written += WriteInputs(pkg, out);

    LOG("Txp2Dump", "%s -> %s (%zu textures, %zu cells, %zu named, %d files)", package_path.c_str(),
        out_dir.c_str(), pkg.textures.size(), pkg.package.cells.size(),
        pkg.package.cell_names.size(), written);
    AvsManager::Shutdown(g_engine.avs);
    return written > 0 ? 0 : 2;
}

}
