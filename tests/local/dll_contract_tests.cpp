#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "avs_boot.h"
#include "avs_funcs.h"
#include "avs_xml.h"
#include "game_profile.h"
#include "ifs_inspect.h"
#include "qpro_dll.h"
#include "support/dll_loader.h"
#include "support/env.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string EnvDir(const char* name) {
    return Support::EnvVar(name).value_or("");
}

}

TEST_CASE("bm2dx qpro part tables match the pattern-scan contract") {
    const std::string dir = EnvDir("R573_IIDX_DIR");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const QproDll::Parts parts = QproDll::Read(dir);
    REQUIRE(parts.ok());

    const std::vector<QproDll::Part>& heads = parts.of(QproDll::Category::Head);
    REQUIRE(!heads.empty());
    CHECK(heads.size() >= 447);
    CHECK(heads[0].ifs.find("qp_kihon") != std::string::npos);

    for (const auto c : {QproDll::Category::Body, QproDll::Category::Hand, QproDll::Category::Face,
                         QproDll::Category::Hair, QproDll::Category::Back}) {
        INFO("category " << QproDll::Prefix(c));
        CHECK(!parts.of(c).empty());
    }
    for (const QproDll::Part& p : heads) {
        REQUIRE(p.ifs.size() >= 8);
        REQUIRE(p.ifs.starts_with("qp_"));
        REQUIRE(p.ifs.ends_with(".ifs"));
    }
}

TEST_CASE("game profile auto-detection identifies the real installs") {
    int checked = 0;
    if (const std::string d = EnvDir("R573_IIDX_DIR"); !d.empty()) {
        const GameProfile::Profile* p = GameProfile::AutoDetect(d);
        REQUIRE(p != nullptr);
        CHECK(std::string(p->slug) == "iidx33");
        CHECK(!p->legacy_afp);
        checked++;
    }
    if (const std::string d = EnvDir("R573_SDVX_DIR"); !d.empty()) {
        const GameProfile::Profile* p = GameProfile::AutoDetect(d);
        REQUIRE(p != nullptr);
        CHECK(std::string(p->slug) == "sdvx7");
        CHECK(!p->legacy_afp);
        checked++;
    }
    if (const std::string d = EnvDir("R573_DDR_DIR"); !d.empty()) {
        const GameProfile::Profile* p = GameProfile::AutoDetect(d);
        REQUIRE(p != nullptr);
        CHECK(std::string(p->slug) == "ddrworld");
        CHECK(p->legacy_afp);
        checked++;
    }
    if (checked == 0) SKIP("no R573_*_DIR set");
}

TEST_CASE("avs2-core boots and our property-tree wrappers parse a real IFS") {
    const std::string dir = EnvDir("R573_IIDX_DIR");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    DllLoader avs_dll;
    REQUIRE(avs_dll.Load((dir + "/modules/avs2-core.dll").c_str()));
    AvsFuncs avs;
    REQUIRE(avs.Load(avs_dll));
    REQUIRE(AvsManager::Boot(avs));

    REQUIRE(AvsManager::MountFsRoot(avs, "/cts_host", dir + "/data"));
    REQUIRE(AvsManager::MountIfsImage(avs, "/afp/packages", "/cts_host/graphic/02005.ifs"));

    const int expected = IfsInspect::CountExpectedTextures(avs);
    CHECK(expected > 0);

    const AvsXml::PropertyTree tree =
        AvsXml::LoadFromFile(avs, "/afp/packages/tex/texturelist.xml");
    REQUIRE(static_cast<bool>(tree));
    CHECK(AvsXml::CountMatches(avs, tree, "texturelist/texture") == expected);

    std::vector<std::string> names;
    const int gathered = AvsXml::GatherMatchAttr(avs, tree, "texturelist/texture", "name", names);
    CHECK(gathered == expected);
    CHECK(std::cmp_equal(names.size(), expected));
    for (const std::string& n : names)
        CHECK(!n.empty());

    const std::vector<IfsInspect::AtlasFilter> filters = IfsInspect::ReadAtlasFilters(avs);
    CHECK(std::cmp_less_equal(filters.size(), expected));
    for (const IfsInspect::AtlasFilter& f : filters) {
        CHECK(f.mag_filter_d3d <= 2);
        CHECK(f.min_filter_d3d <= 2);
    }

    if (avs.avs_fs_umount != nullptr) {
        avs.avs_fs_umount("/afp/packages");
        avs.avs_fs_umount("/cts_host");
    }
}
