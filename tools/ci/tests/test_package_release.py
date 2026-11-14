import zipfile

import package_release
import stage_editor


def touch(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"MZ")


def fake_artefacts(root):
    win64 = root / "win64"
    win32 = root / "win32"
    editor = root / "editor"
    for name in ["573Renderer.exe", "573Renderer.pdb", "573Encoder.exe", "573Encoder.pdb"]:
        touch(win64 / name)
    for name in ["573Renderer32.exe", "573Renderer32.pdb", "573Encoder.exe"]:
        touch(win32 / name)
    for name in stage_editor.NEEDED:
        touch(editor / name)
    return win64, win32, editor


def names(zip_path):
    with zipfile.ZipFile(zip_path) as archive:
        return sorted(archive.namelist())


def test_the_renderer_zip_holds_both_renderers_and_the_encoder(tmp_path):
    win64, win32, editor = fake_artefacts(tmp_path)
    into = tmp_path / "release"
    assert package_release.package(win64, win32, editor, into) == []
    assert names(into / "573Renderer.zip") == [
        "573Encoder.exe",
        "573Renderer.exe",
        "573Renderer32.exe",
    ]


def test_the_editor_zip_is_the_editor_and_its_host(tmp_path):
    win64, win32, editor = fake_artefacts(tmp_path)
    into = tmp_path / "release"
    package_release.package(win64, win32, editor, into)
    assert names(into / "573Editor.zip") == ["ifs_editor.exe", "preview_host.exe"]


def test_a_missing_encoder_is_named_and_nothing_is_packed(tmp_path):
    win64, win32, editor = fake_artefacts(tmp_path)
    (win64 / "573Encoder.exe").unlink()
    into = tmp_path / "release"
    short = package_release.package(win64, win32, editor, into)
    assert short == [str(win64 / "573Encoder.exe")]
    assert not into.exists()


def test_an_editor_without_its_host_is_refused(tmp_path):
    win64, win32, editor = fake_artefacts(tmp_path)
    (editor / "preview_host.exe").unlink()
    into = tmp_path / "release"
    short = package_release.package(win64, win32, editor, into)
    assert short == [str(editor / "preview_host.exe")]
    assert not into.exists()


def test_symbols_staged_beside_the_editor_stay_out(tmp_path):
    win64, win32, editor = fake_artefacts(tmp_path)
    touch(editor / "ifs_editor.pdb")
    into = tmp_path / "release"
    package_release.package(win64, win32, editor, into)
    assert names(into / "573Editor.zip") == ["ifs_editor.exe", "preview_host.exe"]
