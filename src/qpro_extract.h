#pragma once

#include "qpro_scan.h"

#include <set>
#include <string>
#include <vector>

namespace QproExtract {

struct Issue {
    std::string text;
    bool failure;
};

using CategorySel = QproModel::CategorySel;

struct Options {
    std::string game_dir;
    std::string out_dir;
    int fps = 60;
    CategorySel parts;
    PartSelection part_sel;
};

struct Result {
    int parts = 0;
    int images = 0;
    int skipped = 0;
    int failed = 0;
    std::string json_path;
    std::string error;
};

struct Status {
    bool running = false;
    bool finished = false;
    int total = 0;
    int done = 0;
    int images = 0;
    int skipped = 0;
    int failed = 0;
    std::string output_dir;
    std::string error;
    std::vector<Issue> issues;
};

Status GetStatus();
bool IsRunning();

Result Run(const Options& opt);

void SetHueScopeEnabled(bool on);

void DumpIfs(const std::string& ifs_path);

void BodyOne(const std::string& game_dir, const std::string& body_ifs);

void BackOne(const std::string& game_dir, const std::string& back_ifs);

void HeadOne(const std::string& game_dir, const std::string& head_ifs);

void HandOne(const std::string& game_dir, const std::string& hand_ifs);

int RenderHandComposite(const std::string& game_dir, const std::string& hand_ifs, char side,
                        const std::string& out_path);

struct LayerJob {
    const char* layer;
    const char* item_clip;
    const char* atlas;
    const char* hue_eff;
    std::string out_path;
};

struct CompositeShare {
    uint32_t sid = 0;
    int comp_base = -1;
    int main_base = -1;
};

int RenderItemComposite(const std::string& game_dir, const std::string& item_ifs,
                        const std::vector<LayerJob>& jobs, const CompositeShare& share = {},
                        int fps = 60, bool detect_only = false,
                        std::set<std::string>* video_out = nullptr);

int RenderBackRealtime(const std::string& game_dir, const std::string& back_ifs, int fps,
                       const std::string& out_path);

int ProbeBackNativeFps(const std::string& game_dir, const std::string& back_ifs, int fps);

int RenderBackComposite(const std::string& game_dir, const std::string& back_ifs, int fps,
                        const std::string& out_path, int native_fps = 0,
                        const CompositeShare& share = {}, bool detect_only = false,
                        std::set<std::string>* video_out = nullptr);

int RenderPartComposite(const std::string& game_dir, const std::string& item_ifs, const char* layer,
                        const char* hue_eff, const std::string& out_path);

void HairOne(const std::string& game_dir, const std::string& hair_ifs);

void FaceOne(const std::string& game_dir, const std::string& face_ifs);

void ClipOne(const std::string& game_dir, const std::string& ifs, const std::string& clip);

void HandComposite(const std::string& game_dir, const std::string& hand_ifs);

void HeadComposite(const std::string& game_dir, const std::string& head_ifs);

void BackComposite(const std::string& game_dir, const std::string& back_ifs);

}
