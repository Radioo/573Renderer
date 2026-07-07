#include "qpro_scan.h"
#include <algorithm>
#include <cstdint>
#include <utility>
#include "qpro_dll.h"

#include "gui_panels_internal.h"
#include "gui_window.h"
#include "../state/app_state.h"
#include "../native_dialog.h"
#include "../qpro_extract.h"
#include "imgui.h"

#include <cfloat>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace Panels {

namespace {
void DrawQproIssues(const QproExtract::Status& st) {
    if (st.issues.empty()) return;
    const ImVec4 orange(1.0F, 0.80F, 0.35F, 1.0F);
    const ImVec4 red(1.0F, 0.55F, 0.45F, 1.0F);
    ImGui::Spacing();
    ImGui::TextColored(
        (st.failed != 0) ? red : orange,
        "%d skipped, %d failed - the following parts were not extracted:", st.skipped, st.failed);
    if (ImGui::SmallButton("Copy list")) {
        std::string all;
        for (const auto& is : st.issues) {
            all +=
                (is.failure ? std::string("FAILED: ") : std::string("skipped: ")) + is.text + "\n";
        }
        ImGui::SetClipboardText(all.c_str());
    }
    ImGui::BeginChild("qpro_issues", ImVec2(0, 170), 1, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& is : st.issues) {
        ImGui::TextColored(is.failure ? red : orange, "%s %s", is.failure ? "[FAIL]" : "[skip]",
                           is.text.c_str());
    }
    ImGui::EndChild();
}
}

namespace {

struct GroupView {
    std::string date;
    std::vector<int> parts;
};

int s_scan_gen = -1;
std::vector<QproExtract::ScanPart> s_parts;
std::vector<uint8_t> s_checked;
std::vector<GroupView> s_groups;

const char* kNoDateBucket = "(no source IFS / unknown date)";

void RebuildGroups() {
    std::map<std::string, std::vector<int>> by_date;
    for (int i = 0; std::cmp_less(i, s_parts.size()); ++i) {
        const std::string& d = s_parts[i].date;
        by_date[d.empty() ? kNoDateBucket : d].push_back(i);
    }
    s_groups.clear();
    for (auto& kv : by_date)
        s_groups.push_back({.date = kv.first, .parts = std::move(kv.second)});
    std::ranges::sort(s_groups, [](const GroupView& a, const GroupView& b) {
        bool const au = (a.date == kNoDateBucket);
        bool const bu = (b.date == kNoDateBucket);
        if (au != bu) return bu;
        return a.date > b.date;
    });
}

QproExtract::PartSelection BuildSelection() {
    QproExtract::PartSelection sel;
    int maxidx[(int)QproDll::Category::Count] = {0};
    for (const auto& p : s_parts)
        maxidx[p.cat] = std::max(p.idx + 1, maxidx[p.cat]);
    for (int c = 0; c < (int)QproDll::Category::Count; ++c)
        sel.sel[c].assign((size_t)maxidx[c], 0);
    for (size_t i = 0; i < s_parts.size(); ++i)
        sel.sel[s_parts[i].cat][s_parts[i].idx] = s_checked[i];
    return sel;
}

void DrawScanButton(bool busy, const QproExtract::ScanResult& scan) {
    ImGui::BeginDisabled(busy || scan.running);
    if (ImGui::Button("Scan parts from bm2dx.dll")) {
        QproExtract::MarkScanRunning();
        App::Request r;
        r.start_qpro_scan = true;
        App::Global().PostRequest(std::move(r));
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enumerate every qpro part in the loaded game (read directly from "
                          "bm2dx.dll) and group them\n"
                          "by their source IFS's modified date, so you can render ONLY the parts "
                          "from a recent update.\n"
                          "The manifests (2dx_qpro.json / qpro_videos.json) always cover the FULL "
                          "part set regardless.");
    }
    if (scan.running) {
        ImGui::SameLine();
        ImGui::TextDisabled("scanning bm2dx.dll ...");
    }
    if (!scan.error.empty())
        ImGui::TextColored(ImVec4(1.0F, 0.55F, 0.45F, 1.0F), "Scan failed: %s", scan.error.c_str());
}

void DrawPartGroup(size_t gi) {
    const GroupView& g = s_groups[gi];
    ImGui::PushID((int)gi);

    bool all = true;
    int gsel = 0;
    for (int const pi : g.parts) {
        gsel += s_checked[pi];
        if (s_checked[pi] == 0U) all = false;
    }
    bool v = all;
    if (ImGui::Checkbox("##grp", &v)) {
        for (int const pi : g.parts)
            s_checked[pi] = (uint8_t)v;
    }
    ImGui::SameLine();

    if (ImGui::TreeNode("node", "%s  -  %d parts (%d selected)", g.date.c_str(),
                        (int)g.parts.size(), gsel)) {
        ImGuiListClipper clip;
        clip.Begin((int)g.parts.size());
        while (clip.Step()) {
            for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                int const pi = g.parts[row];
                bool c = s_checked[pi] != 0;
                ImGui::PushID(pi);
                if (ImGui::Checkbox(s_parts[pi].label.c_str(), &c)) s_checked[pi] = (uint8_t)c;
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

int DrawPartScanList(bool busy) {
    QproExtract::ScanResult const scan = QproExtract::GetScanResult();
    if (scan.done && scan.generation != s_scan_gen) {
        s_scan_gen = scan.generation;
        s_parts = scan.parts;
        s_checked.assign(s_parts.size(), 1);
        RebuildGroups();
    }

    DrawScanButton(busy, scan);

    if (s_parts.empty()) return -1;

    int nsel = 0;
    for (uint8_t const c : s_checked)
        nsel += c;

    ImGui::Text("%d / %zu parts selected", nsel, s_parts.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("All")) std::ranges::fill(s_checked, (uint8_t)1);
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) std::ranges::fill(s_checked, (uint8_t)0);
    ImGui::TextDisabled("Uncheck older date groups to render only recent parts (the category boxes "
                        "above still apply).");

    ImGui::BeginChild("qpro_parts", ImVec2(0, 300), 1);
    for (size_t gi = 0; gi < s_groups.size(); ++gi)
        DrawPartGroup(gi);
    ImGui::EndChild();
    return nsel;
}

bool s_scope_hue = true;
int s_qpro_fps = 60;
bool s_ex_head = true;
bool s_ex_hand = true;
bool s_ex_hair = true;
bool s_ex_face = true;
bool s_ex_body = true;
bool s_ex_back = true;

void DrawQproIntro(App::State& state) {
    ImGui::Spacing();
    ImGui::TextWrapped("Extract the qpro (Q-pro DJ avatar) parts from the loaded game into a "
                       "web-ready asset layout. The part lists are read from bm2dx.dll by "
                       "pattern, so it survives game updates. Body parts are assembled via the "
                       "qp_motion template; the other parts are cropped from their texture "
                       "atlases.");
    ImGui::Spacing();
    ImGui::TextDisabled("Select an output folder - the assets and qpro JSON are written into a "
                        "\"qpro_assets\" subfolder inside it. Approximately 2300 parts; extraction "
                        "takes several minutes and the live preview pauses while it runs.");
    ImGui::Spacing();

    int rw = 0;
    int rh = 0;
    state.GetRenderSize(rw, rh);
    const bool body_ok = (rw == 520 && rh == 704);
    if (body_ok) {
        ImGui::TextColored(ImVec4(0.6F, 0.9F, 0.6F, 1.0F),
                           "Render size 520x704 - body parts will be assembled.");
    } else {
        ImGui::TextColored(
            ImVec4(1.0F, 0.85F, 0.5F, 1.0F),
            "Body parts need a 520x704 render size (current %dx%d). Select the "
            "\"520x704 (qpro avatar)\" preset on the Setup screen and re-Load to include "
            "bodies; the other parts (hair/hand/face/head/back) extract at any size.",
            rw, rh);
    }
    ImGui::Spacing();
}

void DrawQproOptions() {
    ImGui::Checkbox("Keep static base fills un-hue-shifted", &s_scope_hue);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Effect parts (e.g. the 21chronicle rainbow sword) are one sprite with a\n"
            "rainbow effect fill + a static gold base fill. afp hue-shifts both; the\n"
            "live game shifts only the rainbow. On (default) scopes the hue to the\n"
            "effect bitmap so the gold base stays gold. Off = afp-literal (whole sprite).");
    }
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Output fps", &s_qpro_fps, 1, 10);
    s_qpro_fps = std::max(s_qpro_fps, 1);
    s_qpro_fps = std::min(s_qpro_fps, 240);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Encode / sample rate for the exported clips (like the renderer's render_fps).\n"
            "Animations play at their NATIVE game speed; backgrounds are real-time-sampled to\n"
            "this rate (a 30fps bg's frame is held 2x at 60). 60 matches the avatar. Nothing\n"
            "is guessed from the clips.");
    }
    ImGui::Spacing();

    ImGui::TextDisabled("Animated parts -> .webm (VP9) + .mp4 (HEVC-alpha, Safari) + .avif poster");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Every ANIMATED part exports three files so a website can serve transparent video to\n"
            "any browser: a transparent WebM (VP9) for Chrome/Edge/Firefox, an HEVC-with-alpha "
            "MP4\n"
            "(hvc1) for Safari, and a static AVIF poster (first frame). STATIC parts stay a lone "
            "AVIF.");
    }
    ImGui::Spacing();
}

bool DrawCategoryChecks() {
    ImGui::TextUnformatted("Categories to extract:");
    ImGui::Checkbox("Head", &s_ex_head);
    ImGui::SameLine();
    ImGui::Checkbox("Hand", &s_ex_hand);
    ImGui::SameLine();
    ImGui::Checkbox("Hair", &s_ex_hair);
    ImGui::SameLine();
    ImGui::Checkbox("Face", &s_ex_face);
    ImGui::SameLine();
    ImGui::Checkbox("Body", &s_ex_body);
    ImGui::SameLine();
    ImGui::Checkbox("Back", &s_ex_back);
    const bool any_cat = s_ex_head || s_ex_hand || s_ex_hair || s_ex_face || s_ex_body || s_ex_back;
    if (!any_cat) {
        ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.5F, 1.0F),
                           "Select at least one category to extract.");
    }
    ImGui::Spacing();
    return any_cat;
}

void DrawExtractButton(App::State& state, const QproExtract::Status& st, bool any_cat, int nsel) {
    const bool any_part = (nsel != 0);
    ImGui::BeginDisabled(st.running || !any_cat || !any_part);
    if (ImGui::Button("Choose output folder + extract...", ImVec2(280, 32))) {
        std::string const picked = NativeDialog::BrowseForFolder(Gui::GetHwnd(), "");
        if (!picked.empty()) {
            App::Request r;
            r.start_qpro_extract = true;
            r.qpro_out_dir = picked;
            r.qpro_hue_scope = s_scope_hue;
            r.qpro_fps = s_qpro_fps;
            r.qpro_parts.head = s_ex_head;
            r.qpro_parts.hand = s_ex_hand;
            r.qpro_parts.hair = s_ex_hair;
            r.qpro_parts.face = s_ex_face;
            r.qpro_parts.body = s_ex_body;
            r.qpro_parts.back = s_ex_back;
            if (nsel >= 0) r.qpro_part_sel = BuildSelection();
            state.PostRequest(std::move(r));
        }
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
}

void DrawExtractStatus(const QproExtract::Status& st) {
    if (st.running) {
        float const frac = st.total > 0 ? (float)st.done / (float)st.total : -1.0F;
        char ov[64];
        snprintf(ov, sizeof(ov), "%d / %d parts", st.done, st.total);
        ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0), ov);
        DrawQproIssues(st);
    } else if (st.finished) {
        if (!st.error.empty()) {
            ImGui::TextColored(ImVec4(1.0F, 0.6F, 0.5F, 1.0F), "Failed: %s", st.error.c_str());
        } else {
            ImVec4 const col = ((st.skipped != 0) || (st.failed != 0))
                                   ? ImVec4(1.0F, 0.80F, 0.35F, 1.0F)
                                   : ImVec4(0.6F, 0.9F, 0.6F, 1.0F);
            ImGui::TextColored(col, "Done: %d images written%s.", st.images,
                               ((st.skipped != 0) || (st.failed != 0))
                                   ? " (some parts were not extracted - see below)"
                                   : "");
            ImGui::TextWrapped("Output: %s", st.output_dir.c_str());
            DrawQproIssues(st);
        }
    }
}

}

void RenderQproTabBody() {
    auto& state = App::Global();
    DrawQproIntro(state);

    QproExtract::Status const st = QproExtract::GetStatus();

    DrawQproOptions();
    const bool any_cat = DrawCategoryChecks();

    ImGui::Separator();
    ImGui::TextUnformatted("Per-part selection (by update date):");
    const int nsel = DrawPartScanList(st.running);
    ImGui::Spacing();

    DrawExtractButton(state, st, any_cat, nsel);
    DrawExtractStatus(st);
}

}
