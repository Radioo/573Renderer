#pragma once

#include "qpro/qpro_scan.h"

#include <set>
#include <string>
#include <vector>

struct EngineSession;
struct D3D9State;

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
void PublishStatus(Status s);

Result Run(EngineSession& es, D3D9State& d3d, const Options& opt);

void SetHueScopeEnabled(bool on);

void DumpIfs(EngineSession& es, const std::string& ifs_path);

void BodyOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& body_ifs);

void BackOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& back_ifs);

void HeadOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& head_ifs);

void HandOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& hand_ifs);

int RenderHandComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                        const std::string& hand_ifs, char side, const std::string& out_path);

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

int RenderItemComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                        const std::string& item_ifs, const std::vector<LayerJob>& jobs,
                        const CompositeShare& share = {}, int fps = 60, bool detect_only = false,
                        std::set<std::string>* video_out = nullptr);

int RenderBackRealtime(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                       const std::string& back_ifs, int fps, const std::string& out_path);

int ProbeBackNativeFps(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                       const std::string& back_ifs, int fps);

struct BackOptions {
    int native_fps = 0;
    CompositeShare share;
    bool detect_only = false;
    std::set<std::string>* video_out = nullptr;
};

int RenderBackComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                        const std::string& back_ifs, int fps, const std::string& out_path,
                        const BackOptions& opt = {});

int RenderPartComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                        const std::string& item_ifs, const char* layer, const char* hue_eff,
                        const std::string& out_path);

void HairOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& hair_ifs);

void FaceOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
             const std::string& face_ifs);

void ClipOne(EngineSession& es, D3D9State& d3d, const std::string& game_dir, const std::string& ifs,
             const std::string& clip);

void HandComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                   const std::string& hand_ifs);

void HeadComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                   const std::string& head_ifs);

void BackComposite(EngineSession& es, D3D9State& d3d, const std::string& game_dir,
                   const std::string& back_ifs);

}
