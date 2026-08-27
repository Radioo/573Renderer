import subprocess

import check_host_isolation
import pytest


def make_repo(root):
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    (root / "src" / "gui" / "timeline").mkdir(parents=True)
    (root / "src" / "gui" / "other").mkdir(parents=True)
    return root


def write(root, rel, text):
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


@pytest.fixture
def repo(tmp_path):
    return make_repo(tmp_path)


def test_gate_passes_on_a_command_only_editor(repo):
    write(repo, "src/gui/timeline/gui_tl_editor.cpp",
          "void Draw() { App::Global().PostCommand(PresetCmd::Wrap(PresetCmd::Seek{})); }\n")
    problems, checked = check_host_isolation.check(repo)
    assert problems == []
    assert checked == 1


def test_gate_fails_on_a_direct_preset_host_call(repo):
    write(repo, "src/gui/timeline/gui_tl_editor.cpp", "void Draw() { PresetHost::Seek(4); }\n")
    problems, _ = check_host_isolation.check(repo)
    assert len(problems) == 1
    assert "gui_tl_editor.cpp:1" in problems[0]
    assert "PresetHost" in problems[0]


def test_gate_fails_on_scene3d_and_gc2d_hosts(repo):
    write(repo, "src/gui/timeline/gui_tl_clips.h",
          "inline bool A() { return Scene3dHost::Active(); }\n"
          "inline bool B() { return Gc2dHost::Active(); }\n")
    problems, _ = check_host_isolation.check(repo)
    assert len(problems) == 2
    assert "Scene3dHost" in problems[0]
    assert "Gc2dHost" in problems[1]


def test_gate_covers_the_preset_library(repo):
    write(repo, "src/gui/gui_preset_library.cpp", "void Save() { PresetHost::Unload(); }\n")
    problems, checked = check_host_isolation.check(repo)
    assert checked == 1
    assert len(problems) == 1


def test_gate_covers_every_preset_library_file(repo):
    write(repo, "src/gui/gui_preset_library_actions.cpp",
          "void Save() { PresetHost::Unload(); }\n")
    write(repo, "src/gui/gui_preset_library_internal.h",
          "inline bool A() { return Gc2dHost::Active(); }\n")
    problems, checked = check_host_isolation.check(repo)
    assert checked == 2
    assert len(problems) == 2
    assert "gui_preset_library_actions.cpp:1" in problems[0]
    assert "gui_preset_library_internal.h:1" in problems[1]


def test_gate_ignores_panels_outside_the_editor(repo):
    write(repo, "src/gui/other/gui_scene3d_panel.cpp", "void Draw() { Scene3dHost::Unload(); }\n")
    problems, checked = check_host_isolation.check(repo)
    assert problems == []
    assert checked == 0
