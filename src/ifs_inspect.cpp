#include "ifs_inspect.h"
#include "avs_funcs.h"
#include "afp_funcs.h"
#include "avs_xml.h"
#include "state/ifs_catalog.h"
#include "support/log.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <algorithm>

namespace IfsInspect {

std::vector<App::CompanionIfs> FindCompanions(const std::string& base_ifs_path) {
    struct Suffix {
        const char* tag;
        const char* human;
    };
    static const Suffix kLocaleSuffixes[] = {
        {.tag = "_j", .human = "Japanese"},
        {.tag = "_a", .human = "Asian"},
        {.tag = "_k", .human = "Korean"},
    };

    std::vector<App::CompanionIfs> out;
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path const base(base_ifs_path);
    if (base.empty() || !fs::exists(base, ec)) return out;

    fs::path const parent = base.parent_path();
    std::string const stem = base.stem().string();
    std::string ext = base.extension().string();
    if (ext.empty()) ext = ".ifs";

    for (const auto& s : kLocaleSuffixes) {
        std::string cand_name = stem;
        cand_name += s.tag;
        cand_name += ext;
        fs::path const candidate = parent / cand_name;
        if (!fs::exists(candidate, ec)) continue;
        App::CompanionIfs c;
        c.path = candidate.string();
        c.suffix = s.tag;
        c.display_name = candidate.filename().string();
        c.loaded = false;
        out.push_back(std::move(c));
    }
    return out;
}

int CountExpectedTextures(const AvsFuncs& avs) {
    auto tree = AvsXml::LoadFromFile(avs, "/afp/packages/tex/texturelist.xml");
    if (!tree) return 0;
    return AvsXml::CountMatches(avs, tree, "texturelist/texture");
}

std::vector<AtlasFilter> ReadAtlasFilters(const AvsFuncs& avs, const char* mount_root) {
    std::vector<AtlasFilter> out;
    char path[256];
    std::snprintf(path, sizeof(path), "%s/tex/texturelist.xml",
                  (mount_root != nullptr) ? mount_root : "/afp/packages");
    auto tree = AvsXml::LoadFromFile(avs, path);
    if (!tree) return out;

    T_PROPERTY_NODE* tex = AvsXml::FindFirst(avs, tree, "texturelist/texture");
    while (tex != nullptr) {
        AtlasFilter af{};
        char mag[32] = {};
        char min[32] = {};
        AvsXml::ReadStrAttr(avs, tex, "mag_filter", mag, sizeof(mag));
        AvsXml::ReadStrAttr(avs, tex, "min_filter", min, sizeof(min));

        auto to_d3d = [](const char* s) -> unsigned int {
            if (!s || !*s) return 0;
            if (std::strcmp(s, "nearest") == 0) return 1;
            if (std::strcmp(s, "linear") == 0) return 2;
            return 0;
        };
        af.mag_filter_d3d = to_d3d(mag);
        af.min_filter_d3d = to_d3d(min);
        out.push_back(af);

        tex = AvsXml::NextMatch(avs, tex);
    }
    return out;
}

void LoadDictionary(const AvsFuncs& avs, App::IfsConfig& cfg) {
    cfg.bitmap_names.clear();
    cfg.anim_names.clear();

    {
        auto tree = AvsXml::LoadFromFile(avs, "/afp/packages/tex/texturelist.xml");
        if (tree) {
            int atlases = 0;
            T_PROPERTY_NODE* tex = AvsXml::FindFirst(avs, tree, "texturelist/texture");
            while (tex != nullptr) {
                atlases++;
                T_PROPERTY_NODE* img = avs.property_search(nullptr, tex, "image");
                while (img != nullptr) {
                    char name[128];
                    if (AvsXml::ReadStrAttr(avs, img, "name", name, sizeof(name)))
                        cfg.bitmap_names.emplace_back(name);
                    img = AvsXml::NextMatch(avs, img);
                }
                tex = AvsXml::NextMatch(avs, tex);
            }
            LOG("Inspect",
                "IFS '%s': %zu bitmaps across %d atlas(es) "
                "in texturelist.xml",
                cfg.filename.c_str(), cfg.bitmap_names.size(), atlases);
        } else {
            LOG("Inspect",
                "IFS '%s': texturelist.xml parse failed - "
                "treating as 0 bitmaps",
                cfg.filename.c_str());
        }
    }

    {
        auto tree = AvsXml::LoadFromFile(avs, "/afp/packages/afp/afplist.xml");
        if (tree) {
            int const matches =
                AvsXml::GatherMatchAttr(avs, tree, "afplist/afp", "name", cfg.anim_names);
            LOG("Inspect",
                "IFS '%s': %zu animations in afplist.xml "
                "(%d nodes walked)",
                cfg.filename.c_str(), cfg.anim_names.size(), matches);
            for (size_t i = 0; i < cfg.anim_names.size() && i < 10; i++) {
                LOG("Inspect", "  anim[%zu]: '%s'", i, cfg.anim_names[i].c_str());
            }
            if (cfg.anim_names.size() > 10) {
                LOG("Inspect", "  ... (+%zu more)", cfg.anim_names.size() - 10);
            }
        } else {
            LOG("Inspect",
                "IFS '%s': afplist.xml parse failed - "
                "treating as 0 animations",
                cfg.filename.c_str());
        }
    }
}

void ProbeSlots(const AfpFuncs& afp, uint32_t stream_id, App::IfsConfig& cfg) {
    if (afp.afp_mc_get_id_by_path == nullptr) return;

    auto already_have = [&](const std::string& p) {
        return std::ranges::any_of(cfg.slots, [&](const auto& s) { return s.path == p; });
    };

    for (auto& name : cfg.anim_names) {
        if (already_have(name)) continue;
        int const id = afp.afp_mc_get_id_by_path(stream_id, name.c_str());
        if (id >= 0) {
            App::VariantSlot s;
            s.path = name;
            s.default_bitmap = name;
            s.visible = true;
            s.is_valid = true;
            cfg.slots.push_back(std::move(s));
        }
    }

    for (auto& name : cfg.bitmap_names) {
        if (already_have(name)) continue;
        int const id = afp.afp_mc_get_id_by_path(stream_id, name.c_str());
        if (id >= 0) {
            App::VariantSlot s;
            s.path = name;
            s.default_bitmap = name;
            s.visible = true;
            s.is_valid = true;
            cfg.slots.push_back(std::move(s));
        }
    }

    static const char* kCommonSlots[] = {
        "coin",      "paseli",   "e_amu",  "copylight",  "m_paseli", "m_ketai", "cardless",
        "btn_start", "btn_coin", "btn_ok", "btn_cancel", "text_jp",  "text_en", "text_kr",
        "chara",     "chars",    "bg",     "fg",         "lang_jp",  "lang_en", nullptr};
    for (int i = 0; kCommonSlots[i] != nullptr; i++) {
        std::string const name = kCommonSlots[i];
        if (already_have(name)) continue;
        int const id = afp.afp_mc_get_id_by_path(stream_id, name.c_str());
        if (id >= 0) {
            App::VariantSlot s;
            s.path = name;
            s.default_bitmap = name;
            s.visible = true;
            s.is_valid = true;
            cfg.slots.push_back(std::move(s));
        }
    }

    LOG("Inspect", "IFS '%s': resolved %zu variant slots", cfg.filename.c_str(), cfg.slots.size());
}

}
