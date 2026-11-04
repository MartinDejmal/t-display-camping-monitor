"""Host checks; no connected device or third-party Python package required."""
import argparse
import csv
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def partitions():
    rows = list(csv.reader(line for line in (ROOT / "partitions.csv").read_text().splitlines()
                           if line.strip() and not line.lstrip().startswith("#")))
    end = 0x9000
    slots = []
    history = 0
    for row in rows:
        name, kind, subtype, offset, size = (value.strip() for value in row[:5])
        offset, size = int(offset, 0), int(size, 0)
        assert offset >= end, f"Overlapping partition {name}"
        assert offset % 0x1000 == 0 and size % 0x1000 == 0
        assert offset + size <= 4 * 1024 * 1024
        if kind == "app":
            assert offset % 0x10000 == 0
            slots.append(size)
        if name == "spiffs":
            history = size
        end = offset + size
    assert len(slots) == 2 and slots[0] == slots[1]
    assert history * .9 > 30 * 1440 * 24, "Insufficient nominal 30-day payload space"
    for image in (ROOT / ".pio/build").glob("ttgo-t-display*/firmware.bin"):
        assert image.stat().st_size <= slots[0], f"Firmware exceeds OTA slot: {image}"
        usb = int(image.parent.name.endswith("usb-sense"))
        bench = int(image.parent.name.endswith("service"))
        marker = f"|TDLOGGER:ORIGINAL:USB={usb}:BENCH={bench}:RECORD=2:PART=1|".encode()
        assert marker in image.read_bytes(), f"Missing OTA compatibility marker: {image}"
        print(f"Image {image.parent.name}: {image.stat().st_size} / {slots[0]} bytes")
    print("Partitions: aligned, non-overlapping, dual OTA, 30-day payload fits")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default=shutil.which("g++"))
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    work = ROOT / "work/checks"
    work.mkdir(parents=True, exist_ok=True)
    if not args.cxx or not args.node:
        parser.error("g++ and Node.js required; pass --cxx and --node if not on PATH")
    exe = work / ("core-test.exe" if sys.platform == "win32" else "core-test")
    compiler_env = dict(os.environ)
    compiler_env["PATH"] = str(Path(args.cxx).resolve().parent) + os.pathsep + compiler_env.get("PATH", "")
    subprocess.run([args.cxx, "-std=c++11", "-Wall", "-Wextra", "-pedantic", "-static",
                    "-I", str(ROOT / "include"), str(ROOT / "tests/core_test.cpp"), "-o", str(exe)], check=True, env=compiler_env)
    subprocess.run([str(exe)], check=True, env=compiler_env)
    storage_exe = work / ("storage-test.exe" if sys.platform == "win32" else "storage-test")
    subprocess.run([args.cxx, "-std=c++11", "-Wall", "-Wextra", "-pedantic", "-static",
                    "-I", str(ROOT / "tests/fakes"), "-I", str(ROOT / "include"),
                    str(ROOT / "tests/storage_test.cpp"), str(ROOT / "src/storage.cpp"),
                    "-o", str(storage_exe)], check=True, env=compiler_env)
    fixture = tempfile.mkdtemp(prefix="storage-", dir=work)
    subprocess.run([str(storage_exe), fixture], check=True, env=compiler_env)
    ui = (ROOT / "include/web_ui.h").read_text(encoding="utf-8")
    source = re.search(r"<script>(.*?)</script>", ui, re.S).group(1)
    (work / "ui.js").write_text(source, encoding="utf-8")
    subprocess.run([args.node, "--check", str(work / "ui.js")], check=True)
    subprocess.run([args.node, str(ROOT / "tests/web_test.cjs"), str(work / "ui.js")], check=True)
    partitions()
    print("All host checks passed")

if __name__ == "__main__":
    main()
