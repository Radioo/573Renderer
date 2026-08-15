import json
import os

import check_preset_layers
import check_preset_states
import preset_dump
import pytest


def write_document(root, document):
    directory = root / document["build"]
    directory.mkdir(parents=True, exist_ok=True)
    (directory / f"{document['id']}.json").write_text(json.dumps(document), encoding="utf-8")


def fake_renderer(directory, code, log_line=None):
    directory.mkdir(parents=True, exist_ok=True)
    write_log = "" if log_line is None else f"echo {log_line}> renderer.log\r\n"
    if os.name == "nt":
        exe = directory / "fake_renderer.bat"
        exe.write_text(f"@echo off\r\n{write_log}exit /b {code}\r\n", encoding="utf-8")
        return exe
    write_log = "" if log_line is None else f"echo '{log_line}' > renderer.log\n"
    exe = directory / "fake_renderer.sh"
    exe.write_text(f"#!/bin/sh\n{write_log}exit {code}\n", encoding="utf-8")
    exe.chmod(0o755)
    return exe


def documented_screen():
    return {
        "id": "iidx11-card-in",
        "build": "iidx11",
        "assets": {"card": {"kind": "package2d", "dir": "data/graph/sys/card"}},
        "markers": [],
        "options": [],
        "tracks": [
            {
                "id": "CARD_BG",
                "clips": [
                    {
                        "id": "CARD_BG",
                        "type": "sprite.animate",
                        "params": {"asset": "card", "animation": "CARD_BG"},
                    }
                ],
            }
        ],
    }


def test_layers_gate_fails_on_an_empty_dump(tmp_path):
    problems = check_preset_layers.check(preset_dump.read_dump(tmp_path))
    assert len(problems) == 1
    assert "no preset documents" in problems[0]


def test_layers_gate_fails_when_no_document_draws_a_sprite(tmp_path):
    document = documented_screen()
    document["tracks"] = []
    write_document(tmp_path, document)
    problems = check_preset_layers.check(preset_dump.read_dump(tmp_path))
    assert len(problems) == 1
    assert "not one sprite.draw or sprite.animate clip" in problems[0]


def test_layers_gate_fails_on_an_unclassified_layer(tmp_path):
    document = documented_screen()
    document["tracks"][0]["clips"][0]["params"]["animation"] = "NOT_LOOKED_AT"
    write_document(tmp_path, document)
    problems = check_preset_layers.check(preset_dump.read_dump(tmp_path))
    assert any("NOT_LOOKED_AT" in problem for problem in problems)


def test_layers_gate_accepts_a_classified_layer(tmp_path):
    write_document(tmp_path, documented_screen())
    assert check_preset_layers.check(preset_dump.read_dump(tmp_path)) == []


def test_states_gate_fails_on_an_empty_dump(tmp_path):
    problems, _, _ = check_preset_states.check(preset_dump.read_dump(tmp_path))
    assert len(problems) == 1
    assert "no preset documents" in problems[0]


def test_states_gate_fails_when_no_document_has_a_state(tmp_path):
    write_document(tmp_path, documented_screen())
    problems, _, _ = check_preset_states.check(preset_dump.read_dump(tmp_path))
    assert len(problems) == 1
    assert "not one marker or option choice" in problems[0]


def test_states_gate_fails_on_a_marker_with_no_row(tmp_path):
    document = documented_screen()
    document["markers"] = [{"frame": 0, "label": "Undocumented phase"}]
    write_document(tmp_path, document)
    problems, _, _ = check_preset_states.check(preset_dump.read_dump(tmp_path))
    assert any("Undocumented phase" in problem for problem in problems)


def test_states_gate_fails_on_a_documented_state_the_preset_lost(tmp_path):
    document = documented_screen()
    document["id"] = "iidx11-attract"
    document["markers"] = [{"frame": 0, "label": "Boot animation, models hidden"}]
    write_document(tmp_path, document)
    problems, _, _ = check_preset_states.check(preset_dump.read_dump(tmp_path))
    assert any("Warp in, rotating and zooming" in problem for problem in problems)


def test_states_gate_fails_on_a_row_naming_no_document(tmp_path):
    document = documented_screen()
    document["markers"] = [{"frame": 0, "label": "Boot animation, models hidden"}]
    write_document(tmp_path, document)
    problems, _, _ = check_preset_states.check(preset_dump.read_dump(tmp_path))
    assert any("names a preset no built-in document provides" in problem for problem in problems)


def test_states_gate_fails_on_a_row_whose_id_carries_no_build_prefix(tmp_path, monkeypatch):
    document = documented_screen()
    document["markers"] = [{"frame": 0, "label": "Only state"}]
    write_document(tmp_path, document)
    doc = tmp_path / "preset_states.md"
    doc.write_text(
        "| preset | state | source |\n"
        "|---|---|---|\n"
        "| iidx11-card-in | Only state | the row the document backs |\n"
        "| attract-screen | Only state | a typo no document backs |\n",
        encoding="utf-8",
    )
    monkeypatch.setattr(check_preset_states, "DOC", doc)
    problems, _, _ = check_preset_states.check(preset_dump.read_dump(tmp_path))
    assert len(problems) == 1
    assert problems[0].startswith("attract-screen: docs/preset_states.md names a preset")


def test_write_dump_reports_the_exit_code_and_the_renderer_log(tmp_path, monkeypatch):
    exe = fake_renderer(tmp_path / "exe", 3, "the dump could not be written")
    monkeypatch.setattr(preset_dump, "RENDERER", exe)
    with pytest.raises(preset_dump.DumpError) as bad:
        preset_dump.write_dump(tmp_path / "dump")
    assert "exited 3" in str(bad.value)
    assert "the dump could not be written" in str(bad.value)


def test_write_dump_says_when_the_renderer_left_no_log(tmp_path, monkeypatch):
    exe = fake_renderer(tmp_path / "exe", 4)
    monkeypatch.setattr(preset_dump, "RENDERER", exe)
    with pytest.raises(preset_dump.DumpError) as bad:
        preset_dump.write_dump(tmp_path / "dump")
    assert "exited 4" in str(bad.value)
    assert "no renderer.log" in str(bad.value)
