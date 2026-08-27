#include "gui_test_harness.h"

#include "editor/preset_editor_state.h"
#include "gui_preset_library.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_json.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/ifs_catalog.h"
#include "state/preset_commands.h"

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

std::filesystem::path TempRoot(const std::string& name) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    return root;
}

std::filesystem::path MakeInstall(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root / "JAE", ec);
    std::ofstream file(root / "JAE" / "bm2dx.exe", std::ios::binary);
    const std::vector<char> bytes((std::size_t)860160, 'A');
    file.write(bytes.data(), (std::streamsize)bytes.size());
    return root;
}

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    file << text;
}

std::string ReadText(const std::filesystem::path& path) {
    const std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

Doc::Document UserDocument(const std::string& id, const std::string& name) {
    Doc::Document document;
    document.id = id;
    document.name = name;
    document.build = "iidx10";
    document.length = 600;
    Doc::Track track;
    track.id = "core";
    track.name = "core";
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    Doc::Clip clip;
    clip.id = "core_draw";
    clip.start = 0;
    clip.end = 300;
    clip.command = Doc::ModelDraw{.asset = "scene", .model = "core"};
    track.clips.push_back(std::move(clip));
    document.assets.push_back(
        Doc::Asset{.id = "scene", .kind = Doc::AssetKind::Scene3d, .dir = "data/model"});
    document.tracks.push_back(std::move(track));
    return document;
}

struct Fixture {
    std::filesystem::path install;
    std::filesystem::path presets;
    std::filesystem::path scratch;
};

Fixture MakeFixture(const std::string& name) {
    Fixture fixture;
    const std::filesystem::path root = TempRoot(name);
    fixture.install = MakeInstall(root / "game");
    fixture.presets = root / "presets";
    fixture.scratch = root / "scratch";
    std::error_code ec;
    std::filesystem::create_directories(fixture.presets, ec);
    std::filesystem::create_directories(fixture.scratch, ec);
    Editor::Global().Close();
    GuiTest::SetDisplaySize(1600.0F, 1400.0F);
    App::Global().SetGameDir(fixture.install.string());
    Panels::PresetLibrary::SetUserRoot(fixture.presets.string());
    App::Global().ClearCloseRequest();
    GuiTest::EnterReadyView("scene3d", "iidx11");
    return fixture;
}

std::vector<PresetCmd::Any> DrainPresetCommands() {
    std::vector<PresetCmd::Any> out;
    while (const std::optional<App::Command> command = App::Global().TakeCommand()) {
        const auto* backend = std::get_if<App::Cmd::BackendCommand>(&*command);
        if (backend == nullptr) continue;
        const auto* preset = std::any_cast<PresetCmd::Any>(&backend->payload);
        if (preset != nullptr) out.push_back(*preset);
    }
    return out;
}

}

TEST_CASE("the preset library is the center pane on the scene3d backend", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_center_pane");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));
    App::Global().SetAvailableIfs({{.name = "bg/bg_0001.ifs",
                                    .full_path = (fixture.install / "bg_0001.ifs").string(),
                                    .from_arc = false}});

    ImGuiTest* test = harness.NewTest("library_center_pane");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->Yield(2);

        GuiTest::FocusChild(ctx, "main_view/pane_center");
        IM_CHECK(ctx->ItemExists("###lib_new"));
        IM_CHECK(ctx->ItemExists("###lib_import"));
        IM_CHECK(ctx->ItemExists("###lib_filter"));
        IM_CHECK(ctx->ItemExists("##scene_filter") == false);

        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center/lib_scroll");
        IM_CHECK(ctx->ItemExists("###lib_group_builtin"));
        IM_CHECK(ctx->ItemExists("###lib_group_user"));
        IM_CHECK(ctx->ItemExists("**/###lib_row_remix"));

        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left");
        IM_CHECK(ctx->ItemExists("##ifsfilter"));
        IM_CHECK(ctx->ItemExists("###lib_new") == false);
        IM_CHECK(ctx->ItemExists("###lib_filter") == false);

        ctx->SetRef("##main");
        IM_CHECK(ctx->WindowInfo("main_view/pane_center/lib_problems").Window != nullptr);
        IM_CHECK(
            ctx->WindowInfo("main_view/pane_left/library_area", ImGuiTestOpFlags_NoError).Window ==
            nullptr);
        IM_CHECK(ctx->WindowInfo("main_view/pane_center/scene_scroll", ImGuiTestOpFlags_NoError)
                     .Window == nullptr);
    };
    harness.Run(test);
}

TEST_CASE("an AFP backend keeps the scene layers view and shows no library", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_afp_center");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_0001.ifs", 0, 300);
    App::IfsConfig& cfg = App::Global().MutConfig("bg_0001.ifs");
    cfg.filename = "bg_0001.ifs";
    cfg.anim_names = {"bg_main"};

    ImGuiTest* test = harness.NewTest("library_afp_center");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->Yield(2);

        GuiTest::FocusChild(ctx, "main_view/pane_center");
        IM_CHECK(ctx->ItemExists("##scene_filter"));
        IM_CHECK(ctx->ItemExists("###lib_new") == false);

        ctx->SetRef("##main");
        IM_CHECK(ctx->WindowInfo("main_view/pane_center/scene_scroll").Window != nullptr);
        IM_CHECK_EQ(ctx->ItemInfo("**/###lib_new", ImGuiTestOpFlags_NoError).ID, 0U);
        IM_CHECK_EQ(ctx->ItemInfo("**/###lib_group_builtin", ImGuiTestOpFlags_NoError).ID, 0U);
        IM_CHECK_EQ(ctx->ItemInfo("**/###lib_row_remix", ImGuiTestOpFlags_NoError).ID, 0U);
    };
    harness.Run(test);
}

TEST_CASE("the preset library list gets the height of the center pane", "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_center_height");

    ImGuiTest* test = harness.NewTest("library_center_height");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->Yield(2);
        const ImGuiWindow* pane = ctx->WindowInfo("main_view/pane_center").Window;
        const ImGuiWindow* list =
            ctx->WindowInfo("main_view/pane_center/lib_scroll", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(pane != nullptr);
        IM_CHECK(list != nullptr);
        IM_CHECK_GT(pane->Size.y, 400.0F);
        IM_CHECK_GT(list->Size.y, pane->Size.y * 0.4F);
    };
    harness.Run(test);
}

TEST_CASE("the library lists the built-in and user documents of this build", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_groups");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));

    ImGuiTest* test = harness.NewTest("library_groups");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        IM_CHECK(ctx->ItemExists("**/###lib_group_builtin"));
        IM_CHECK(ctx->ItemExists("**/###lib_group_user"));
        IM_CHECK(ctx->ItemExists("**/###lib_group_other"));
        IM_CHECK(ctx->ItemExists("**/###lib_row_iidx10-music-select"));
        IM_CHECK(ctx->ItemExists("**/###lib_row_remix"));
    };
    harness.Run(test);
}

TEST_CASE("selecting a library row loads the document into the editor and the renderer",
          "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_select");

    ImGuiTest* test = harness.NewTest("library_select");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        while (App::Global().TakeCommand().has_value()) {
        }
        ctx->ItemClick("**/###lib_row_iidx10-music-select");
        ctx->Yield(2);

        IM_CHECK(Editor::Global().Loaded());
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "iidx10-music-select");
        IM_CHECK_EQ(Editor::Global().UndoDepth(), 0);
        IM_CHECK_EQ(Editor::Global().Dirty(), false);

        const std::vector<PresetCmd::Any> commands = DrainPresetCommands();
        bool loaded = false;
        for (const PresetCmd::Any& command : commands) {
            const auto* load = std::get_if<PresetCmd::LoadDocument>(&command);
            if (load == nullptr || load->document == nullptr) continue;
            loaded = load->document->id == "iidx10-music-select";
        }
        IM_CHECK(loaded);
    };
    harness.Run(test);
}

TEST_CASE("importing an unparseable file shows the line and column and loads nothing",
          "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_import_bad");
    const std::filesystem::path broken = fixture.scratch / "broken.json";
    WriteText(broken, "{\n  \"schema\": \"573renderer/scene-preset\",\n  \"id\": ,\n}\n");
    GuiTest::SetOpenFileResult(broken.string());

    ImGuiTest* test = harness.NewTest("library_import_bad");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_import");
        ctx->Yield(3);

        IM_CHECK(Editor::Global().Loaded() == false);
        ctx->SetRef("Import problems");
        IM_CHECK(ctx->ItemExists("###lib_import_close"));
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("line 3") != std::string::npos);
        IM_CHECK(text.find("column") != std::string::npos);
    };
    harness.Run(test);
}

TEST_CASE("importing a document with a validation error loads it and lists the problem",
          "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_import_problem");
    Doc::Document document = UserDocument("overlapped", "overlapped");
    Doc::Clip second;
    second.id = "core_draw_2";
    second.start = 100;
    second.end = 400;
    second.command = Doc::ModelDraw{.asset = "scene", .model = "core"};
    document.tracks.front().clips.push_back(std::move(second));
    const std::filesystem::path path = fixture.scratch / "overlapped.json";
    WriteText(path, Doc::Save(document));
    GuiTest::SetOpenFileResult(path.string());

    ImGuiTest* test = harness.NewTest("library_import_problem");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_import");
        ctx->Yield(3);

        IM_CHECK(Editor::Global().Loaded());
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "overlapped");
        IM_CHECK(ctx->ItemExists("**/###lib_problem_0"));
    };
    harness.Run(test);
}

TEST_CASE("exporting a document and importing it back yields the same file", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_roundtrip");
    const std::filesystem::path out = fixture.scratch / "exported.json";
    GuiTest::SetSaveFileResult(out.string());
    GuiTest::SetOpenFileResult(out.string());

    ImGuiTest* test = harness.NewTest("library_roundtrip");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_iidx10-music-select");
        ctx->Yield(2);
        ctx->ItemClick("**/###lib_export");
        ctx->Yield(2);
        ctx->ItemClick("**/###lib_import");
        ctx->Yield(3);
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "iidx10-music-select");
    };
    harness.Run(test);

    const std::string written = ReadText(out);
    CHECK_FALSE(written.empty());
    CHECK(Preset::Doc::Save(Editor::Global().Document()) == written);
}

TEST_CASE("saving a user document writes it under presets and clears the modified mark",
          "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_save");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));

    ImGuiTest* test = harness.NewTest("library_save");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_remix");
        ctx->Yield(2);
        Editor::Global().Apply([](Doc::Document& document) {
            document.notes = "edited";
            return true;
        });
        ctx->Yield(2);
        IM_CHECK(Editor::Global().Dirty());
        App::Global().SetLoadProgress({});
        ctx->ItemClick("**/###lib_save");
        ctx->Yield(3);
        IM_CHECK(Editor::Global().Dirty() == false);
        IM_CHECK(App::Global().GetLoadProgress().active == false);
    };
    harness.Run(test);

    const std::string saved = ReadText(fixture.presets / "iidx10" / "remix.json");
    CHECK(saved.find("edited") != std::string::npos);
}

TEST_CASE("Ctrl+S saves the loaded user document", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_ctrl_s");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));

    ImGuiTest* test = harness.NewTest("library_ctrl_s");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_remix");
        ctx->Yield(2);
        Editor::Global().Apply([](Doc::Document& document) {
            document.notes = "by shortcut";
            return true;
        });
        ctx->Yield(2);
        ctx->KeyDown(ImGuiKey_LeftCtrl);
        ctx->KeyPress(ImGuiKey_S);
        ctx->KeyUp(ImGuiKey_LeftCtrl);
        ctx->Yield(3);
        IM_CHECK(Editor::Global().Dirty() == false);
    };
    harness.Run(test);

    const std::string saved = ReadText(fixture.presets / "iidx10" / "remix.json");
    CHECK(saved.find("by shortcut") != std::string::npos);
}

TEST_CASE("switching document with unsaved changes asks first", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_prompt");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));

    ImGuiTest* test = harness.NewTest("library_prompt");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_remix");
        ctx->Yield(2);
        Editor::Global().Apply([](Doc::Document& document) {
            document.notes = "unsaved";
            return true;
        });
        ctx->Yield(2);
        ctx->ItemClick("**/###lib_row_iidx10-music-select");
        ctx->Yield(3);
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "remix");

        ctx->SetRef("Unsaved changes");
        IM_CHECK(ctx->ItemExists("###lib_prompt_discard"));
        ctx->ItemClick("###lib_prompt_discard");
        ctx->Yield(3);
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "iidx10-music-select");
    };
    harness.Run(test);
}

TEST_CASE("New opens the document properties modal and makes the id from the name",
          "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_new");

    ImGuiTest* test = harness.NewTest("library_new");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_new");
        ctx->Yield(3);
        ctx->SetRef("Document properties");
        const char* name_path = "###tl_doc_tabs/###tl_doc_tab_general/###tl_doc_name";
        IM_CHECK(ctx->ItemExists(name_path));
        IM_CHECK_EQ(ImGui::GetCurrentContext()->ActiveId, ctx->GetID(name_path));
        ctx->ItemInputValue(name_path, "My New Screen");
        ctx->ItemClick("###tl_doc_done");
        ctx->Yield(3);
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "my-new-screen");
        IM_CHECK_STR_EQ(Editor::Global().Document().build.c_str(), "iidx10");
    };
    harness.Run(test);
}

TEST_CASE("Duplicate gives the copy an id that is free", "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_duplicate");

    ImGuiTest* test = harness.NewTest("library_duplicate");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_iidx10-music-select");
        ctx->Yield(2);
        ctx->ItemClick("**/###lib_duplicate");
        ctx->Yield(3);
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "iidx10-music-select-copy");
        IM_CHECK(Editor::Global().Dirty());
    };
    harness.Run(test);
}

TEST_CASE("a built-in cannot be saved in place and offers a user copy", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_builtin_save");

    ImGuiTest* test = harness.NewTest("library_builtin_save");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_iidx10-music-select");
        ctx->Yield(2);
        ctx->ItemClick("**/###lib_save");
        ctx->Yield(3);
        ctx->SetRef("Save as user copy");
        IM_CHECK(ctx->ItemExists("###lib_saveas_id"));
        ctx->ItemClick("###lib_saveas_ok");
        ctx->Yield(3);
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "iidx10-music-select-copy");
    };
    harness.Run(test);

    std::error_code ec;
    CHECK(
        std::filesystem::exists(fixture.presets / "iidx10" / "iidx10-music-select-copy.json", ec));
}

TEST_CASE("importing a document whose id is taken keeps the file that already owns it",
          "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_import_clash");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "the one already here")));
    const std::filesystem::path incoming = fixture.scratch / "remix.json";
    WriteText(incoming, Doc::Save(UserDocument("remix", "the incoming one")));
    GuiTest::SetOpenFileResult(incoming.string());

    ImGuiTest* test = harness.NewTest("library_import_clash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_import");
        ctx->Yield(3);
        IM_CHECK(Editor::Global().Loaded());
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "remix-2");
    };
    harness.Run(test);

    const std::string kept = ReadText(fixture.presets / "iidx10" / "remix.json");
    CHECK(kept.find("the one already here") != std::string::npos);
    std::error_code ec;
    CHECK(std::filesystem::exists(fixture.presets / "iidx10" / "remix-2.json", ec));
}

TEST_CASE("the library problem list stays inside the pane when the pane is short",
          "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_problem_room");
    Doc::Document document = UserDocument("overlapped", "overlapped");
    Doc::Clip second;
    second.id = "core_draw_2";
    second.start = 100;
    second.end = 400;
    second.command = Doc::ModelDraw{.asset = "scene", .model = "core"};
    document.tracks.front().clips.push_back(std::move(second));
    WriteText(fixture.presets / "iidx10" / "overlapped.json", Doc::Save(document));

    ImGuiTest* test = harness.NewTest("library_problem_room");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_overlapped");
        ctx->Yield(2);
        GuiTest::SetDisplaySize(1360.0F, 700.0F);
        ctx->Yield(4);
        const ImGuiTestItemInfo info =
            ctx->ItemInfo("**/###lib_problem_0", ImGuiTestOpFlags_NoError);
        IM_CHECK(info.ID != 0);
        IM_CHECK(info.RectClipped.GetHeight() > 0.0F);
    };
    harness.Run(test);
    GuiTest::SetDisplaySize(1600.0F, 1400.0F);
}

TEST_CASE("an other-build row opens read-only and never reaches the renderer", "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_other_build");

    ImGuiTest* test = harness.NewTest("library_other_build");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        while (App::Global().TakeCommand().has_value()) {
        }
        ctx->ItemClick("**/###lib_row_iidx11-attract");
        ctx->Yield(3);

        IM_CHECK(Editor::Global().Loaded());
        IM_CHECK(Editor::Global().ReadOnly());
        for (const PresetCmd::Any& command : DrainPresetCommands())
            IM_CHECK(std::get_if<PresetCmd::LoadDocument>(&command) == nullptr);

        const std::string before = Doc::Save(Editor::Global().Document());
        IM_CHECK(Editor::Global().Apply([](Doc::Document& document) {
            document.notes = "should not stick";
            return true;
        }) == false);
        IM_CHECK_STR_EQ(Doc::Save(Editor::Global().Document()).c_str(), before.c_str());
        IM_CHECK(Editor::Global().Dirty() == false);
    };
    harness.Run(test);
}

TEST_CASE("an other-build document is still shown, with every editing control disabled",
          "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_other_build_disabled");

    ImGuiTest* test = harness.NewTest("library_other_build_disabled");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_iidx11-attract");
        ctx->Yield(3);
        IM_CHECK((ctx->ItemInfo("**/###lib_save").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        IM_CHECK((ctx->ItemInfo("**/###lib_properties").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        IM_CHECK((ctx->ItemInfo("**/###lib_duplicate").ItemFlags & ImGuiItemFlags_Disabled) == 0);

        IM_CHECK(ctx->ItemExists("**/###tl_add_command"));
        IM_CHECK((ctx->ItemInfo("**/###tl_add_command").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        IM_CHECK((ctx->ItemInfo("**/###tl_add_track").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        IM_CHECK((ctx->ItemInfo("**/###tl_doc_badge").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        IM_CHECK((ctx->ItemInfo("**/###tl_play").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("read-only: built for iidx11") != std::string::npos);
    };
    harness.Run(test);
}

TEST_CASE("duplicating an other-build document makes an editable copy for this build",
          "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_other_build_copy");

    ImGuiTest* test = harness.NewTest("library_other_build_copy");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_iidx11-attract");
        ctx->Yield(2);
        ctx->ItemClick("**/###lib_duplicate");
        ctx->Yield(3);
        IM_CHECK(Editor::Global().ReadOnly() == false);
        IM_CHECK_STR_EQ(Editor::Global().Document().build.c_str(), "iidx10");
        IM_CHECK_STR_EQ(Editor::Global().Document().id.c_str(), "iidx11-attract-copy");
    };
    harness.Run(test);
}

TEST_CASE("a user file that cannot load is listed under files that did not load",
          "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_rejected");
    WriteText(fixture.presets / "iidx10" / "unparseable.json", "{ \"schema\": , }\n");
    WriteText(fixture.presets / "iidx10" / "first.json", Doc::Save(UserDocument("twin", "first")));
    WriteText(fixture.presets / "iidx10" / "second.json",
              Doc::Save(UserDocument("twin", "second")));
    Panels::PresetLibrary::SetUserRoot(fixture.presets.string());

    ImGuiTest* test = harness.NewTest("library_rejected");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->Yield(3);
        IM_CHECK(ctx->ItemExists("**/###lib_rejected_0"));
        IM_CHECK(ctx->ItemExists("**/###lib_rejected_1"));
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("Files that did not load") != std::string::npos);
        IM_CHECK(text.find("unparseable.json") != std::string::npos);
    };
    harness.Run(test);
}

TEST_CASE("closing the app with unsaved changes asks before it exits", "[gui][library]") {
    GuiTest::Harness harness;
    const Fixture fixture = MakeFixture("r573_lib_close");
    WriteText(fixture.presets / "iidx10" / "remix.json",
              Doc::Save(UserDocument("remix", "my remix")));

    ImGuiTest* test = harness.NewTest("library_close");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->ItemClick("**/###lib_row_remix");
        ctx->Yield(2);
        Editor::Global().Apply([](Doc::Document& document) {
            document.notes = "unsaved";
            return true;
        });
        ctx->Yield(2);

        App::Global().PostCloseRequest();
        ctx->Yield(3);
        IM_CHECK(App::Global().CloseConfirmed() == false);
        ctx->SetRef("Unsaved changes");
        IM_CHECK(ctx->ItemExists("###lib_prompt_cancel"));
        ctx->ItemClick("###lib_prompt_cancel");
        ctx->Yield(3);
        IM_CHECK(App::Global().CloseRequested() == false);
        IM_CHECK(App::Global().CloseConfirmed() == false);

        ctx->SetRef("##main");
        App::Global().PostCloseRequest();
        ctx->Yield(3);
        ctx->SetRef("Unsaved changes");
        ctx->ItemClick("###lib_prompt_discard");
        ctx->Yield(3);
        IM_CHECK(App::Global().CloseConfirmed());
    };
    harness.Run(test);
    App::Global().ClearCloseRequest();
}

TEST_CASE("closing the app with nothing unsaved exits without asking", "[gui][library]") {
    GuiTest::Harness harness;
    MakeFixture("r573_lib_close_clean");

    ImGuiTest* test = harness.NewTest("library_close_clean");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->Yield(2);
        App::Global().PostCloseRequest();
        ctx->Yield(3);
        IM_CHECK(App::Global().CloseConfirmed());
        IM_CHECK(App::Global().CloseRequested() == false);
    };
    harness.Run(test);
    App::Global().ClearCloseRequest();
}
