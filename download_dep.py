#!/usr/bin/env python3
"""
Dependency downloader & builder for MuluUI.

Downloads and optionally builds the three MuluUI dependencies into a local
``dependencies/`` folder so that no system-wide package manager (vcpkg, apt,
brew) is required.

Dependencies
------------
* **SDL3**          – windowing + OpenGL context + input  (source build via CMake)
* **tinyxml2**      – XML parser for the .ui form loader   (source build via CMake)
* **stb_truetype.h** – single-header TrueType rasteriser   (download only)

Usage
-----
.. code-block:: bash

    python download_dep.py                    # interactive: prompts for static/shared
    python download_dep.py --shared           # build shared libraries  (.dll / .so)
    python download_dep.py --static           # build static libraries  (.lib / .a)
    python download_dep.py --skip-build       # download sources only, don't build
    python download_dep.py --clean            # wipe dependencies/ and start fresh

The linkage choice is persisted in ``dependencies/.mulu_dep_config.json`` so
that ``build_sdk.py`` and ``build_examples.py`` can read it and pass matching
flags to the main CMake configure step.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path
from typing import Optional
from urllib.request import urlretrieve

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
PROJECT_ROOT = Path(__file__).resolve().parent
DEPS_DIR = PROJECT_ROOT / "dependencies"
DEPS_SRC = DEPS_DIR / "src"
DEPS_BUILD = DEPS_DIR / "build"
DEPS_INSTALL = DEPS_DIR / "install"
DEPS_CONFIG = DEPS_DIR / ".mulu_dep_config.json"

# Where stb_truetype.h should land (in the deps include directory).
STB_DEST = DEPS_INSTALL / "include" / "stb_truetype.h"

# ---------------------------------------------------------------------------
# Dependency metadata
# ---------------------------------------------------------------------------
SDL3_VERSION = "3.2.10"
SDL3_URL = (
    f"https://github.com/libsdl-org/SDL/archive/refs/tags/"
    f"release-{SDL3_VERSION}.zip"
)
SDL3_SRC_DIR = DEPS_SRC / f"SDL-release-{SDL3_VERSION}"

TINYXML2_VERSION = "10.1.0"
TINYXML2_URL = (
    f"https://github.com/leethomason/tinyxml2/archive/refs/tags/"
    f"{TINYXML2_VERSION}.zip"
)
TINYXML2_SRC_DIR = DEPS_SRC / f"tinyxml2-{TINYXML2_VERSION}"

STB_TRUETYPE_URL = (
    "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h"
)

# Inter font (SIL Open Font License) – a clean, modern UI typeface.
# Served directly from the Inter GitHub releases.
INTER_FONT_URL = (
    "https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip"
)
FONT_DEST = DEPS_DIR / "font"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def log(msg: str) -> None:
    print(f"[download_dep] {msg}", flush=True)


def die(msg: str) -> None:
    print(f"[download_dep] ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd: list[str], cwd: Optional[Path] = None, shell: bool = False) -> None:
    log("$ " + " ".join(cmd))
    proc = subprocess.Popen(
        cmd,
        cwd=str(cwd) if cwd else None,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    assert proc.stdout is not None
    for line in proc.stdout:
        print(line, end="", flush=True)
    proc.wait()
    if proc.returncode != 0:
        die(f"command failed with exit code {proc.returncode}: {' '.join(cmd)}")


def find_cmake() -> str:
    """Return the path to cmake, or die."""
    cmake = shutil.which("cmake")
    if cmake is None:
        die("cmake not found on PATH (install CMake >= 3.22)")
    return cmake


def download(url: str, dest: Path) -> None:
    """Download *url* to *dest*, showing a progress bar."""
    if dest.exists():
        log(f"  already downloaded: {dest.name}")
        return
    log(f"  downloading {dest.name} ...")
    dest.parent.mkdir(parents=True, exist_ok=True)

    def _report(block_num: int, block_size: int, total_size: int) -> None:
        downloaded = block_num * block_size
        if total_size > 0:
            pct = min(100, downloaded * 100 // total_size)
            print(f"\r  {dest.name}: {pct}% ({downloaded}/{total_size})",
                  end="", flush=True)
        else:
            print(f"\r  {dest.name}: {downloaded} bytes", end="", flush=True)

    urlretrieve(url, str(dest), _report)
    print()  # newline after progress bar


def extract_zip(zip_path: Path, dest_dir: Path) -> Path:
    """Extract *zip_path* and return the actual source root (CMakeLists.txt parent).

    GitHub archive zips wrap the source in a directory named <repo>-<tag>,
    but the zip's internal entries already include that prefix.  We extract
    to a staging area first, then detect the real root and rename it to
    *dest_dir* so the caller always gets a flat, predictable path.
    """
    marker = dest_dir / ".mulu_extracted"
    if marker.exists():
        log(f"  already extracted: {dest_dir.name}")
        return dest_dir

    log(f"  extracting {zip_path.name} ...")
    staging = dest_dir.parent / f"_tmp_{dest_dir.name}"
    if staging.exists():
        shutil.rmtree(staging)

    staging.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "r") as zf:
        zf.extractall(staging)

    # Find the actual source root (the directory containing CMakeLists.txt).
    root = _find_cmake_root(staging)
    if root is None:
        # No wrapper directory – use staging as-is.
        root = staging

    # Move to the expected dest_dir.
    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    if root != dest_dir:
        shutil.move(str(root), str(dest_dir))
    else:
        # Already in the right place.
        pass

    # Clean up staging if it wasn't moved.
    if staging.exists():
        shutil.rmtree(staging)

    marker.touch()
    return dest_dir


def _find_cmake_root(base: Path) -> Optional[Path]:
    """Walk up to 3 levels deep looking for CMakeLists.txt; return the dir."""
    for root, dirs, _files in os.walk(str(base)):
        if "CMakeLists.txt" in _files:
            return Path(root)
        # Only go 3 levels deep.
        depth = len(Path(root).relative_to(base).parts)
        if depth >= 3:
            dirs.clear()  # stop descending
    return None


def save_config(shared: bool) -> None:
    """Persist the linkage choice so other scripts can read it."""
    DEPS_DIR.mkdir(parents=True, exist_ok=True)
    data = {"linkage": "shared" if shared else "static"}
    DEPS_CONFIG.write_text(json.dumps(data, indent=2), encoding="utf-8")
    log(f"Configuration saved to {DEPS_CONFIG}")


def load_config() -> Optional[dict]:
    """Return the saved config dict, or None if it doesn't exist."""
    if not DEPS_CONFIG.is_file():
        return None
    try:
        return json.loads(DEPS_CONFIG.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


# ---------------------------------------------------------------------------
# Dependency build steps
# ---------------------------------------------------------------------------

def build_sdl3(shared: bool) -> None:
    """Configure + build + install SDL3."""
    src = SDL3_SRC_DIR
    build = DEPS_BUILD / "sdl3"
    install = DEPS_INSTALL

    if (install / "include" / "SDL3" / "SDL.h").is_file():
        log("SDL3 already installed — skipping build.")
        return

    log("Building SDL3 ...")
    build.mkdir(parents=True, exist_ok=True)

    cmake = find_cmake()
    configure_cmd = [
        cmake, "-S", str(src), "-B", str(build),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install}",
        f"-DBUILD_SHARED_LIBS={'ON' if shared else 'OFF'}",
        "-DSDL_SHARED=ON" if shared else "-DSDL_STATIC=ON",
        "-DSDL_TEST=OFF",
        "-DSDL_TESTS=OFF",
    ]
    run(configure_cmd)
    run([cmake, "--build", str(build), "--config", "Release",
         "--parallel", str(os.cpu_count() or 4)])
    run([cmake, "--install", str(build), "--config", "Release"])

    log("SDL3 build complete.")


def build_tinyxml2(shared: bool) -> None:
    """Configure + build + install tinyxml2."""
    src = TINYXML2_SRC_DIR
    build = DEPS_BUILD / "tinyxml2"
    install = DEPS_INSTALL

    if (install / "include" / "tinyxml2.h").is_file():
        log("tinyxml2 already installed — skipping build.")
        return

    log("Building tinyxml2 ...")
    build.mkdir(parents=True, exist_ok=True)

    cmake = find_cmake()
    configure_cmd = [
        cmake, "-S", str(src), "-B", str(build),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install}",
        f"-DBUILD_SHARED_LIBS={'ON' if shared else 'OFF'}",
        "-Dtinyxml2_BUILD_TESTING=OFF",
    ]
    run(configure_cmd)
    run([cmake, "--build", str(build), "--config", "Release",
         "--parallel", str(os.cpu_count() or 4)])
    run([cmake, "--install", str(build), "--config", "Release"])

    log("tinyxml2 build complete.")


def download_stb_truetype() -> None:
    """Download stb_truetype.h into the dependencies include directory."""
    if STB_DEST.exists():
        log("stb_truetype.h already installed.")
        return

    log("Downloading stb_truetype.h ...")
    STB_DEST.parent.mkdir(parents=True, exist_ok=True)
    download(STB_TRUETYPE_URL, STB_DEST)
    log("stb_truetype.h installed.")


def download_font() -> None:
    """Download the Inter font (SIL OFL) and extract the TrueType (.ttf) version."""
    font_file = FONT_DEST / "Inter-Regular.ttf"
    if font_file.is_file():
        log("Font already installed (Inter-Regular.ttf).")
        return

    log("Downloading Inter font ...")
    FONT_DEST.mkdir(parents=True, exist_ok=True)

    zip_path = FONT_DEST / "Inter-4.0.zip"
    download(INTER_FONT_URL, zip_path)

    log("  extracting Inter-Regular.ttf ...")
    with zipfile.ZipFile(zip_path, "r") as zf:
        # Use the TrueType (.ttf) version — stb_truetype handles TTF more
        # reliably than CFF-based .otf files.
        for name in zf.namelist():
            if name.endswith("Inter-Regular.ttf"):
                zf.extract(name, FONT_DEST)
                extracted = FONT_DEST / name
                shutil.move(str(extracted), str(font_file))
                for parent in extracted.parents:
                    if parent != FONT_DEST and parent.exists():
                        try:
                            parent.rmdir()
                        except OSError:
                            pass
                break

    zip_path.unlink(missing_ok=True)

    if font_file.is_file():
        log("Font installed: Inter-Regular.ttf")
    else:
        log("WARNING: could not extract Inter-Regular.otf from the zip.")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Download and build MuluUI dependencies.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--shared", action="store_true", default=None,
        help="Build shared libraries (.dll / .so)",
    )
    group.add_argument(
        "--static", action="store_true", default=None,
        help="Build static libraries (.lib / .a)",
    )
    parser.add_argument(
        "--skip-build", action="store_true",
        help="Download sources only; do not compile anything",
    )
    parser.add_argument(
        "--clean", action="store_true",
        help=f"Remove {DEPS_DIR} and start fresh",
    )
    args = parser.parse_args()

    # ------------------------------------------------------------------
    # Clean
    # ------------------------------------------------------------------
    if args.clean and DEPS_DIR.exists():
        log(f"Removing {DEPS_DIR} ...")
        shutil.rmtree(DEPS_DIR)

    # ------------------------------------------------------------------
    # Determine linkage
    # ------------------------------------------------------------------
    if args.shared:
        shared = True
    elif args.static:
        shared = False
    else:
        # Try to read saved config.
        cfg = load_config()
        if cfg and "linkage" in cfg:
            shared = cfg["linkage"] == "shared"
            log(f"Using saved linkage config: {'shared' if shared else 'static'}")
        else:
            # Interactive prompt.
            print()
            print("How should dependencies be built?")
            print("  [1] Shared  (.dll / .so  – smaller binary, needs DLLs alongside)")
            print("  [2] Static  (.lib / .a   – larger binary, self-contained)")
            print()
            while True:
                try:
                    choice = input("Choose [1/2] (default: 1): ").strip()
                except (EOFError, KeyboardInterrupt):
                    print()
                    choice = ""
                if choice in ("", "1"):
                    shared = True
                    break
                if choice == "2":
                    shared = False
                    break
                print("Please enter 1 or 2.")

    save_config(shared)

    # ------------------------------------------------------------------
    # Download sources
    # ------------------------------------------------------------------
    log("=== Downloading sources ===")
    DEPS_SRC.mkdir(parents=True, exist_ok=True)

    # SDL3
    sdl_zip = DEPS_SRC / f"sdl3-{SDL3_VERSION}.zip"
    download(SDL3_URL, sdl_zip)
    extract_zip(sdl_zip, SDL3_SRC_DIR)

    # tinyxml2
    txml_zip = DEPS_SRC / f"tinyxml2-{TINYXML2_VERSION}.zip"
    download(TINYXML2_URL, txml_zip)
    extract_zip(txml_zip, TINYXML2_SRC_DIR)

    # stb_truetype.h
    download_stb_truetype()

    # Font for embedded text rendering
    download_font()

    # ------------------------------------------------------------------
    # Build (unless --skip-build)
    # ------------------------------------------------------------------
    if args.skip_build:
        log("--skip-build given; sources downloaded but not compiled.")
        log(f"Run this script again without --skip-build to compile.")
        return

    log("=== Building dependencies ===")
    build_sdl3(shared)
    build_tinyxml2(shared)

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------
    log("=== All dependencies ready ===")
    log(f"  Install prefix : {DEPS_INSTALL}")
    log(f"  Linkage        : {'shared' if shared else 'static'}")
    log(f"  stb_truetype.h : {STB_DEST}")
    log("")
    log("You can now build MuluUI with:")
    log(f"  python build_sdk.py")
    log("  (build_sdk.py reads the linkage config automatically)")


if __name__ == "__main__":
    main()
