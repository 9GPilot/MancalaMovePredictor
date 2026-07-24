#!/usr/bin/env python3
"""
handbrake_watcher.py
Converts .MOV files in a folder to numbered Part files using HandBrakeCLI.

Usage:
    python3 handbrake_watcher.py [folder]

If no folder is given, uses the current directory.

Place a handbrake.properties file in the watched folder to configure it:
    source.extension=MOV
    destination.extension=mp4
    handbrake.options.preset=Fast 1080p30
    handbrakecli=/usr/local/bin/HandBrakeCLI
    delete_original=true
"""

import sys
import re
import subprocess
import configparser
from pathlib import Path


# ---------- Config ----------

def load_config(folder: Path) -> dict:
    defaults = {
        "source.extension": "MOV",
        "destination.extension": "mp4",
        "handbrakecli": "HandBrakeCLI",
        "handbrake.flags": "",
        "delete_original": "true",
    }
    props_file = folder / "handbrake.properties"
    if props_file.exists():
        content = "[config]\n" + props_file.read_text()
        parser = configparser.RawConfigParser()
        parser.read_string(content)
        for key in defaults:
            if parser.has_option("config", key):
                defaults[key] = parser.get("config", key)
        for key, value in parser.items("config"):
            if key.startswith("handbrake.options."):
                defaults[key] = value
    return defaults


# ---------- Next part number ----------

def next_part_number(folder: Path, dest_ext: str) -> int:
    """Scan folder for files named Part<N>.ext and return the next number."""
    pattern = re.compile(r'^Part(\d+)\.' + re.escape(dest_ext) + r'$', re.IGNORECASE)
    highest = 0
    for f in folder.iterdir():
        m = pattern.match(f.name)
        if m:
            highest = max(highest, int(m.group(1)))
    return highest + 1


# ---------- Conversion ----------

def convert(src_file: Path, dest_file: Path, config: dict) -> bool:
    handbrake = config.get("handbrakecli", "HandBrakeCLI")

    cmd = [handbrake]

    # Flags (e.g. --all-audio)
    flags = config.get("handbrake.flags", "").split()
    if "--all-audio" not in flags:
        flags.append("--all-audio")
    cmd.extend(flags)

    # --option value pairs
    options = {}
    for key, val in config.items():
        if key.startswith("handbrake.options."):
            options[key[len("handbrake.options."):]] = val
    if "preset" not in options:
        options["preset"] = "Fast 1080p30"
    for opt, val in options.items():
        cmd.extend([f"--{opt}", val])

    cmd.extend(["-i", str(src_file), "-o", str(dest_file)])

    print(f"  Converting: {src_file.name} → {dest_file.name}")
    print(f"  Command: {' '.join(cmd)}\n")

    try:
        result = subprocess.run(cmd)
    except FileNotFoundError:
        print(f"\n  ERROR: Could not find HandBrakeCLI at '{handbrake}'")
        print("  Set 'handbrakecli' in handbrake.properties to the full path.")
        return False

    if result.returncode == 0:
        print(f"\n  Done: {dest_file.name}")
        if config.get("delete_original", "true").lower() == "true":
            src_file.unlink()
            print(f"  Deleted original: {src_file.name}")
        return True
    else:
        print(f"\n  ERROR: HandBrakeCLI exited with code {result.returncode}")
        if dest_file.exists():
            dest_file.unlink()
        return False


# ---------- Main ----------

def run(folder: Path):
    print("Handbrake Watcher (Python)")
    print(f"Folder: {folder.resolve()}\n")

    config = load_config(folder)
    src_extensions = [e.strip() for e in config["source.extension"].split() if e.strip()]
    dest_ext = config["destination.extension"]

    # Find all source files (ignore already-named Part files)
    part_pattern = re.compile(r'^Part\d+\.', re.IGNORECASE)
    sources = [
        f for f in sorted(folder.iterdir())
        if f.is_file()
        and f.suffix.lstrip(".").upper() in [e.upper() for e in src_extensions]
        and not part_pattern.match(f.name)
    ]

    if not sources:
        print("  No source files found to convert.")
        return

    for src in sources:
        part_num = next_part_number(folder, dest_ext)
        dest_file = folder / f"Part{part_num}.{dest_ext}"
        convert(src, dest_file, config)

    print("\nAll done.")


if __name__ == "__main__":
    folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
    if not folder.is_dir():
        print(f"Error: '{folder}' is not a directory.")
        sys.exit(1)
    try:
        run(folder)
    except KeyboardInterrupt:
        print("\nStopped.")
