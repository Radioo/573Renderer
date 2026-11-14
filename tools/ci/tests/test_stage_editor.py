import stage_editor


def fake_builds(root):
    editor = root / "build-editor"
    host = root / "build"
    for path in [
        editor / "ifs_editor.exe",
        editor / "ifs_editor.pdb",
        editor / "Qt6Core.dll",
        editor / "platforms" / "qwindows.dll",
        host / "preview_host.exe",
        host / "preview_host.pdb",
    ]:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"MZ")
    return editor, host


def staged(into):
    return sorted(p.relative_to(into).as_posix() for p in into.rglob("*") if p.is_file())


def test_the_staged_editor_is_two_executables(tmp_path):
    editor, host = fake_builds(tmp_path)
    into = tmp_path / "dist"
    assert stage_editor.stage(editor, host, into, symbols=False) == []
    assert stage_editor.unmet(into) == []
    assert staged(into) == ["ifs_editor.exe", "preview_host.exe"]


def test_symbols_add_only_the_two_pdbs(tmp_path):
    editor, host = fake_builds(tmp_path)
    into = tmp_path / "dist"
    stage_editor.stage(editor, host, into, symbols=True)
    assert staged(into) == [
        "ifs_editor.exe",
        "ifs_editor.pdb",
        "preview_host.exe",
        "preview_host.pdb",
    ]


def test_a_missing_host_is_named(tmp_path):
    editor, host = fake_builds(tmp_path)
    (host / "preview_host.exe").unlink()
    into = tmp_path / "dist"
    stage_editor.stage(editor, host, into, symbols=False)
    assert stage_editor.unmet(into) == ["preview_host.exe"]
