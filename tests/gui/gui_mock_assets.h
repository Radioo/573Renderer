#pragma once

#include <filesystem>
#include <string>

namespace GuiTest {

class TempAssetDir {
public:
    explicit TempAssetDir(const char* name);
    ~TempAssetDir();
    TempAssetDir(const TempAssetDir&) = delete;
    TempAssetDir& operator=(const TempAssetDir&) = delete;
    TempAssetDir(TempAssetDir&&) = delete;
    TempAssetDir& operator=(TempAssetDir&&) = delete;

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    std::filesystem::path dir_;
    std::string path_;
};

void WriteMock2dPackage(const std::string& dir);

void WriteMock3dScene(const std::string& dir, bool with_camera);

}
