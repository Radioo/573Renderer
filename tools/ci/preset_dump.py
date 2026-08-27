import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RENDERER = ROOT / "bin" / "573Renderer.exe"


class DumpError(Exception):
    pass


def read_dump(dump_dir):
    documents = []
    for path in sorted(Path(dump_dir).glob("*/*.json")):
        try:
            documents.append(json.loads(path.read_text(encoding="utf-8")))
        except json.JSONDecodeError as bad:
            raise DumpError(f"{path} is not valid JSON: {bad}") from bad
    return documents


def log_tail(directory, lines=20):
    log = Path(directory) / "renderer.log"
    if not log.is_file():
        return f"no renderer.log was left in {directory}"
    text = log.read_text(encoding="utf-8", errors="replace").strip()
    if not text:
        return f"{log} is empty"
    return "\n".join(f"  {line}" for line in text.splitlines()[-lines:])


def write_dump(dump_dir):
    if not RENDERER.exists():
        raise DumpError(
            f"{RENDERER} is missing. The preset gates read the documents the renderer "
            f"dumps, so build.bat has to run first (tools/checks.sh does that)."
        )
    Path(dump_dir).mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(RENDERER), "--preset-dump-defaults", str(dump_dir)],
        check=False,
        cwd=dump_dir,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise DumpError(
            f"--preset-dump-defaults exited {result.returncode}. The renderer writes its "
            f"own console to a fresh one and everything else to renderer.log in its working "
            f"directory, so the tail of that log is what happened:\n{log_tail(dump_dir)}"
        )


def documents(dump_dir=None):
    if dump_dir is not None:
        return read_dump(dump_dir)
    with tempfile.TemporaryDirectory(prefix="r573_preset_dump_") as tmp:
        write_dump(tmp)
        return read_dump(tmp)


def add_dump_argument(parser):
    parser.add_argument(
        "--dump",
        default=None,
        help="read documents from an existing --preset-dump-defaults directory "
        "instead of running the renderer",
    )


def clips_of(document, types):
    for track in document.get("tracks", []):
        for clip in track.get("clips", []):
            if clip.get("type") in types:
                yield clip


def asset_dirs(document):
    return {name: asset["dir"] for name, asset in document.get("assets", {}).items()}
