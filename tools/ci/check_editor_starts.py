import argparse
import os
import pathlib
import subprocess
import sys
import tempfile
import time

ALIVE_SECONDS = 6.0
POLL_SECONDS = 0.25


def started(exe, seconds):
    room = dict(os.environ)
    room["QT_QPA_PLATFORM"] = "minimal"
    room["QT_PLUGIN_PATH"] = str(exe.parent)
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as elsewhere:
        running = subprocess.Popen([str(exe)], cwd=elsewhere, env=room,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        waited = 0.0
        while waited < seconds:
            if running.poll() is not None:
                said = running.communicate()[0].decode("utf-8", "replace").strip()
                return False, running.returncode, said
            time.sleep(POLL_SECONDS)
            waited += POLL_SECONDS
        running.kill()
        running.communicate()
    return True, 0, ""


def main():
    parser = argparse.ArgumentParser(description="Check a staged editor folder actually starts")
    parser.add_argument("--dist", default="dist", type=pathlib.Path)
    parser.add_argument("--seconds", default=ALIVE_SECONDS, type=float)
    args = parser.parse_args()

    exe = args.dist / "ifs_editor.exe"
    if not exe.is_file():
        print(f"{exe} is not there", file=sys.stderr)
        return 1
    alive, code, said = started(exe, args.seconds)
    if not alive:
        print(f"{exe.name} exited with {code} instead of running", file=sys.stderr)
        if said:
            print(said, file=sys.stderr)
        return 1
    print(f"{exe.name} ran for {args.seconds:g}s from {args.dist} and was still going")
    return 0


if __name__ == "__main__":
    sys.exit(main())
