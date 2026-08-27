#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "backend/afp_profiles.h"
#include "game_fingerprint.h"
#include "game_profile.h"
#include "game_revision.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace {

struct TempDir {
    std::filesystem::path root;

    explicit TempDir(const char* tag) {
        root = std::filesystem::temp_directory_path() / (std::string("r573_game_") + tag);
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void Dir(const char* rel) const {
        std::error_code ec;
        std::filesystem::create_directories(root / rel, ec);
    }

    void File(const char* rel) const {
        std::error_code ec;
        std::filesystem::create_directories((root / rel).parent_path(), ec);
        std::ofstream f(root / rel, std::ios::binary);
        f << "x";
    }

    void Sized(const char* rel, std::size_t size, char fill) const {
        std::error_code ec;
        std::filesystem::create_directories((root / rel).parent_path(), ec);
        std::ofstream f(root / rel, std::ios::binary);
        const std::vector<char> bytes(size, fill);
        f.write(bytes.data(), (std::streamsize)bytes.size());
    }

    void SizedWithTail(const char* rel, std::size_t size, char fill,
                       const std::vector<unsigned char>& tail) const {
        std::error_code ec;
        std::filesystem::create_directories((root / rel).parent_path(), ec);
        std::ofstream f(root / rel, std::ios::binary);
        const std::vector<char> bytes(size - tail.size(), fill);
        f.write(bytes.data(), (std::streamsize)bytes.size());
        for (const unsigned char byte : tail)
            f.put((char)byte);
    }
};

constexpr std::size_t kHappySkySize = 1105920;
const std::vector<unsigned char> kHappySkyCrcTail = {0xAD, 0xE9, 0x63, 0x32};

}

TEST_CASE("Every profile carries a slug, backend and sane default render size") {
    const std::vector<GameProfile::Profile>& all = GameProfile::All();
    REQUIRE_FALSE(all.empty());
    for (const GameProfile::Profile& p : all) {
        INFO("profile " << p.slug);
        CHECK(p.name != nullptr);
        CHECK(p.slug != nullptr);
        CHECK(p.backend_id != nullptr);
        CHECK(p.default_render_w > 0);
        CHECK(p.default_render_h > 0);
        CHECK(GameProfile::BySlug(p.slug) == &p);
    }
}

TEST_CASE("fingerprint accepts a size match as a patched copy and rejects a size mismatch") {
    const TempDir patched("fp_patched");
    patched.Sized("JAE/bm2dx.exe", 860160, 'A');
    const GameFingerprint::Match hit = GameFingerprint::Identify(patched.root.string());
    REQUIRE(hit.build != nullptr);
    CHECK(std::string(hit.build->id) == "iidx10");
    CHECK(hit.file.find("bm2dx.exe") != std::string::npos);

    const TempDir wrong("fp_wrongsize");
    wrong.Sized("JAE/bm2dx.exe", 4096, 'A');
    CHECK(GameFingerprint::Identify(wrong.root.string()).build == nullptr);
}

TEST_CASE("fingerprint identifies a HAPPY SKY bm2dx.exe by size and CRC") {
    const TempDir exact("fp_iidx12");
    exact.SizedWithTail("JAD/bm2dx.exe", kHappySkySize, 'A', kHappySkyCrcTail);
    const GameFingerprint::Match hit = GameFingerprint::Identify(exact.root.string());
    REQUIRE(hit.build != nullptr);
    CHECK(std::string(hit.build->id) == "iidx12");
    CHECK(std::string(hit.build->profile_slug) == "iidx12");
    CHECK(hit.build->size == kHappySkySize);
    CHECK(hit.build->crc == 0x122D2CC3U);
    CHECK(hit.file.find("bm2dx.exe") != std::string::npos);

    const TempDir patched("fp_iidx12_patched");
    patched.Sized("JAD/bm2dx.exe", kHappySkySize, 'A');
    const GameFingerprint::Match by_size = GameFingerprint::Identify(patched.root.string());
    REQUIRE(by_size.build != nullptr);
    CHECK(std::string(by_size.build->id) == "iidx12");

    CHECK(std::string(GameFingerprint::ProfileSlugFor("iidx12")) == "iidx12");
}

TEST_CASE("A HAPPY SKY install auto-detects as IIDX 12, not as the generic IIDX profile") {
    const GameProfile::Profile* happy = GameProfile::AutoDetect("F:/IIDX/IIDX 12 - HAPPY SKY");
    REQUIRE(happy != nullptr);
    CHECK(std::string(happy->slug) == "iidx12");
    CHECK(std::string(happy->backend_id) == "scene3d");
    CHECK(std::string(happy->game_dll) == "bm2dx.exe");
    CHECK(happy->default_render_w == 640);
    CHECK(happy->default_render_h == 480);

    const GameProfile::Profile* distorted = GameProfile::AutoDetect("F:/IIDX/IIDX 13 - DistorteD");
    REQUIRE(distorted != nullptr);
    CHECK(std::string(distorted->slug) == "iidx13");
}

TEST_CASE("fingerprint finds nothing in an empty directory") {
    const TempDir empty("fp_empty");
    CHECK(GameFingerprint::Identify(empty.root.string()).build == nullptr);
    CHECK(GameFingerprint::Identify("").build == nullptr);
}

TEST_CASE("BySlug rejects an unknown or empty slug") {
    CHECK(GameProfile::BySlug("") == nullptr);
    CHECK(GameProfile::BySlug("not_a_game") == nullptr);
}

TEST_CASE("AutoDetect matches the folder-name substring case-insensitively") {
    const GameProfile::Profile* sdvx = GameProfile::AutoDetect("D:/games/SDVX 7 - NABLA");
    REQUIRE(sdvx != nullptr);
    CHECK(std::string(sdvx->slug) == "sdvx7");

    const GameProfile::Profile* ddr = GameProfile::AutoDetect("D:/games/MDX-001");
    REQUIRE(ddr != nullptr);
    CHECK(std::string(ddr->slug) == "ddrworld");

    CHECK(GameProfile::AutoDetect("") == nullptr);
}

TEST_CASE("No profile's dir_substring shadows a later one") {
    const std::vector<GameProfile::Profile>& all = GameProfile::All();
    for (size_t i = 0; i < all.size(); i++) {
        const std::string a = all[i].dir_substring != nullptr ? all[i].dir_substring : "";
        if (a.empty()) continue;
        for (size_t j = i + 1; j < all.size(); j++) {
            const std::string b = all[j].dir_substring != nullptr ? all[j].dir_substring : "";
            if (b.empty()) continue;
            INFO("'" << a << "' (" << all[i].slug << ", position " << i << ") contains '" << b
                     << "' (" << all[j].slug << ", position " << j
                     << ") - the more specific substring must come FIRST or AutoDetect can never "
                        "reach it");
            CHECK(b.find(a) == std::string::npos);
        }
    }
}

TEST_CASE("Profiles are listed oldest-first within the IIDX family") {
    const std::vector<GameProfile::Profile>& all = GameProfile::All();
    std::vector<std::string> iidx;
    for (const GameProfile::Profile& p : all) {
        const std::string slug = p.slug;
        if (slug.starts_with("iidx")) iidx.push_back(slug);
    }
    CHECK(iidx == std::vector<std::string>{"iidx09", "iidx11", "iidx12", "iidx13", "iidx17",
                                           "iidx18", "iidx19", "iidx20", "iidx24", "iidx26",
                                           "iidx33"});
}

TEST_CASE("Profile order keeps the specific IIDX substrings ahead of the generic one") {
    const GameProfile::Profile* red = GameProfile::AutoDetect("F:/IIDX/IIDX 11 - IIDXRED");
    REQUIRE(red != nullptr);
    CHECK(std::string(red->slug) == "iidx11");

    const GameProfile::Profile* sinobuz = GameProfile::AutoDetect("F:/IIDX/IIDX 24 - SINOBUZ");
    REQUIRE(sinobuz != nullptr);
    CHECK(std::string(sinobuz->slug) == "iidx24");

    const GameProfile::Profile* generic = GameProfile::AutoDetect("F:/IIDX/IIDX 33");
    REQUIRE(generic != nullptr);
    CHECK(std::string(generic->slug) == "iidx33");
}

TEST_CASE("Every AFP-family profile has an engine config row") {
    for (const GameProfile::Profile& p : GameProfile::All()) {
        const std::string backend = p.backend_id != nullptr ? p.backend_id : "";
        if (backend != "afp_modern" && backend != "afp_ddr") continue;
        INFO("profile '" << p.slug << "' uses backend '" << backend
                         << "' but AfpProfiles::For(slug) has no row, so its boot fails");
        CHECK(AfpProfiles::For(p.slug) != nullptr);
    }
}

TEST_CASE("A pop'n install auto-detects as pop'n music 29") {
    const GameProfile::Profile* popn = GameProfile::AutoDetect("F:/POPN/29");
    REQUIRE(popn != nullptr);
    CHECK(std::string(popn->slug) == "popn29");
    CHECK(popn->default_render_w == 1920);
    CHECK(popn->default_render_h == 1080);
}

TEST_CASE("AutoDetect falls back to the profile's own game DLL when the name says nothing") {
    const TempDir t("dllprobe");
    t.File("modules/soundvoltex.dll");

    const GameProfile::Profile* p = GameProfile::AutoDetect(t.root.string());
    REQUIRE(p != nullptr);
    CHECK(std::string(p->slug) == "sdvx7");
}

TEST_CASE("The game-DLL fallback also probes contents/modules and the root") {
    const TempDir nested("dllnested");
    nested.File("contents/modules/soundvoltex.dll");
    const GameProfile::Profile* a = GameProfile::AutoDetect(nested.root.string());
    REQUIRE(a != nullptr);
    CHECK(std::string(a->slug) == "sdvx7");

    const TempDir flat("dllflat");
    flat.File("soundvoltex.dll");
    const GameProfile::Profile* b = GameProfile::AutoDetect(flat.root.string());
    REQUIRE(b != nullptr);
    CHECK(std::string(b->slug) == "sdvx7");
}

TEST_CASE("AutoDetect gives up on a directory with no name hint and no game DLL") {
    const TempDir t("empty");
    t.Dir("data");
    CHECK(GameProfile::AutoDetect(t.root.string()) == nullptr);
}

TEST_CASE("LatestRevisionDir picks the newest 10-digit datecode folder") {
    const TempDir t("rev");
    t.Dir("2024010100");
    t.Dir("2025063000");
    t.Dir("2023120100");

    const std::string best = GameRevision::LatestRevisionDir(t.root.string());
    CHECK(std::filesystem::path(best).filename().string() == "2025063000");
}

TEST_CASE("LatestRevisionDir ignores names that are not exactly ten digits") {
    const TempDir t("revfilter");
    t.Dir("202401010");
    t.Dir("20240101000");
    t.Dir("2024010100.orig");
    t.Dir("20240a0100");
    t.Dir("2024010100");

    const std::string best = GameRevision::LatestRevisionDir(t.root.string());
    CHECK(std::filesystem::path(best).filename().string() == "2024010100");
}

TEST_CASE("LatestRevisionDir ignores files and returns empty when nothing matches") {
    const TempDir t("revnone");
    t.File("2025010100");
    t.Dir("data");
    CHECK(GameRevision::LatestRevisionDir(t.root.string()).empty());
    CHECK(GameRevision::LatestRevisionDir((t.root / "missing").string()).empty());
    CHECK(GameRevision::LatestRevisionDir("").empty());
}
