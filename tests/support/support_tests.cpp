#include <catch2/catch_test_macros.hpp>

#include "support/dll_loader.h"
#include "support/expected.h"
#include "support/folder_job.h"
#include "support/log.h"
#include "support/module_handle.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

TEST_CASE("ClassifyFirstExport detects the mangled Konami scheme") {
    const Support::ExportNaming n = Support::ClassifyFirstExport("XCnbrep7000129");
    CHECK_FALSE(n.by_name);
    CHECK(n.prefix == "XCnbrep7");

    const Support::ExportNaming g = Support::ClassifyFirstExport("XCgsqzn0000af");
    CHECK_FALSE(g.by_name);
    CHECK(g.prefix == "XCgsqzn");
}

TEST_CASE("ClassifyFirstExport detects readable exports") {
    CHECK(Support::ClassifyFirstExport("afp_boot").by_name);
    CHECK(Support::ClassifyFirstExport("afp_do_render").by_name);
    CHECK(Support::ClassifyFirstExport("GetProcAddress").by_name);
    CHECK(Support::ClassifyFirstExport("short").by_name);
    CHECK(Support::ClassifyFirstExport("").by_name);
}

TEST_CASE("ClassifyFirstExport needs at least one prefix character") {
    CHECK(Support::ClassifyFirstExport("123456").by_name);
    const Support::ExportNaming edge = Support::ClassifyFirstExport("X123456");
    CHECK_FALSE(edge.by_name);
    CHECK(edge.prefix == "X");
}

TEST_CASE("FormatMangledExport pads the hex ordinal to six digits") {
    CHECK(Support::FormatMangledExport("XCgsqzn", 0x129) == "XCgsqzn000129");
    CHECK(Support::FormatMangledExport("XCd229cc", 0x0A) == "XCd229cc00000a");
    CHECK(Support::FormatMangledExport("P", 0xABCDEF) == "Pabcdef");
}

TEST_CASE("ModuleHandle owns and releases a module") {
    Support::ModuleHandle m = Support::LoadModule("kernel32.dll");
    REQUIRE(m != nullptr);
    CHECK(GetProcAddress(m.get(), "GetProcAddress") != nullptr);
    m.reset();
    CHECK(m == nullptr);
}

TEST_CASE("DllLoader resolves readable exports by name") {
    DllLoader loader;
    REQUIRE(loader.Load("kernel32.dll"));
    CHECK(loader.IsLoaded());
    CHECK(loader.IsByName());
    CHECK(loader.NumExports() > 0);
    CHECK(loader.GetFunc(0, "GetProcAddress") != nullptr);
    CHECK(loader.GetFunc(0, "DefinitelyNotAnExport__") == nullptr);
}

namespace {

struct CapturedLog {
    std::vector<std::string> lines;
};

void CaptureSink(const char* tag, const char* message, void* user) {
    auto* cap = static_cast<CapturedLog*>(user);
    cap->lines.push_back(std::string(tag) + "|" + message);
}

}

TEST_CASE("Log sink injection captures formatted lines") {
    CapturedLog cap;
    Log::SetSink(CaptureSink, &cap);
    LOG("Test", "value=%d name=%s", 42, "abc");
    for (int i = 0; i < 2; i++) {
        LOG_ONCE("Test", "only once");
    }
    Log::SetSink(nullptr, nullptr);

    REQUIRE(cap.lines.size() == 2);
    CHECK(cap.lines[0] == "Test|value=42 name=abc");
    CHECK(cap.lines[1] == "Test|only once");
}

TEST_CASE("Expected carries values and errors") {
    const auto ok = []() -> Support::Expected<int, std::string> { return 7; }();
    REQUIRE(ok.has_value());
    CHECK(ok.value() == 7);

    const auto bad = []() -> Support::Expected<int, std::string> {
        return Support::Unexpected(std::string("nope"));
    }();
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error() == "nope");
}

TEST_CASE("StripTrailingSlashes drops any run of trailing separators") {
    CHECK(Support::StripTrailingSlashes("F:/games/") == "F:/games");
    CHECK(Support::StripTrailingSlashes("F:\\games\\\\") == "F:\\games");
    CHECK(Support::StripTrailingSlashes("F:/games") == "F:/games");
    CHECK(Support::StripTrailingSlashes("///").empty());
    CHECK(Support::StripTrailingSlashes("").empty());
}

namespace {

struct TempTree {
    std::filesystem::path root;
    TempTree() {
        root = std::filesystem::temp_directory_path() /
               ("r573_folder_job_" + std::to_string(GetCurrentProcessId()));
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "sub");
    }
    TempTree(const TempTree&) = delete;
    TempTree& operator=(const TempTree&) = delete;
    TempTree(TempTree&&) = delete;
    TempTree& operator=(TempTree&&) = delete;
    ~TempTree() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    void Put(const char* rel, const std::string& content) const {
        std::ofstream f(root / rel, std::ios::binary);
        f << content;
    }
};

}

TEST_CASE("ReadFileBytes reads whole files and returns empty on a missing path") {
    const TempTree tree;
    tree.Put("a.bin", "hello");
    const std::vector<uint8_t> bytes = Support::ReadFileBytes(tree.root / "a.bin");
    CHECK(bytes == std::vector<uint8_t>{'h', 'e', 'l', 'l', 'o'});
    CHECK(Support::ReadFileBytes(tree.root / "missing.bin").empty());
}

TEST_CASE("ScanFolder visits every entry recursively and reports the scan count") {
    const TempTree tree;
    tree.Put("a.arc", "x");
    tree.Put("b.txt", "x");
    tree.Put("sub/c.arc", "x");

    int arcs = 0;
    const long long scanned = Support::ScanFolder(
        tree.root, [](const std::filesystem::path&) {},
        [&](auto& it) {
            std::error_code ec;
            if (it->is_regular_file(ec) && it->path().extension() == ".arc") arcs++;
        });
    CHECK(arcs == 2);
    CHECK(scanned == 4);
}

TEST_CASE("FolderJob publishes snapshots and only starts one run at a time") {
    struct DemoStatus {
        bool finished = false;
        int done = 0;
    };
    Support::FolderJob<DemoStatus> job;
    CHECK_FALSE(job.IsRunning());

    job.Publish(DemoStatus{.finished = false, .done = 3});
    CHECK(job.Get().done == 3);

    std::atomic<int> runs{0};
    REQUIRE(job.Start("dir", [&](const std::string&) { runs++; }));
    CHECK_FALSE(job.Start("dir", [&](const std::string&) { runs++; }));
    while (runs.load() < 1)
        std::this_thread::yield();
    CHECK(job.IsRunning());
    job.Finish();
    CHECK_FALSE(job.IsRunning());
    REQUIRE(job.Start("dir", [&](const std::string&) { runs++; }));
    while (runs.load() < 2)
        std::this_thread::yield();
    job.Finish();
    CHECK(runs.load() == 2);
}
