#!/usr/bin/env python3
"""Cross-platform CMake build driver for the MuluUI SDK.

Detects the running platform and runs the appropriate CMake configure +
build steps.  If ``download_dep.py`` has been run, the locally-built
dependencies in ``dependencies/install/`` are used automatically; otherwise
the script falls back to system-installed packages (vcpkg, apt, brew).

Usage examples:
    python build_sdk.py                    # Release, auto-detected generator
    python build_sdk.py -c Debug -j 8
    python build_sdk.py -g Ninja --clean
    python build_sdk.py --target hello
    python build_sdk.py --preset default   # use a CMakePresets.json preset
    python build_sdk.py --shared           # force shared linkage (overrides config)
    python build_sdk.py --static           # force static linkage

Platform -> generator mapping:
    Windows : newest installed Visual Studio generator (falls back to Ninja)
    Linux   : Ninja (falls back to Unix Makefiles)
    macOS   : Ninja (falls back to Unix Makefiles)
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, NoReturn, Optional

PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_DIR_DEFAULT = PROJECT_ROOT / "build"
DEPS_DIR = PROJECT_ROOT / "dependencies"
DEPS_INSTALL = DEPS_DIR / "install"
DEPS_CONFIG = DEPS_DIR / ".mulu_dep_config.json"

# Generators that build every configuration at once (vs. single-config ones).
MULTI_CONFIG_GENERATORS = ("visual studio", "ninja multi-config")


def log(message: str) -> None:
    print(f"[build_sdk] {message}", flush=True)


def die(message: str) -> NoReturn:
    print(f"[build_sdk] ERROR: {message}", file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd: List[str], cwd: Optional[Path] = None, shell: bool = False) -> None:
    log("$ " + " ".join(cmd))
    try:
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
    except FileNotFoundError:
        die(f"command not found: {cmd[0]}. Is it installed and on PATH?")
    assert proc.stdout is not None
    for line in proc.stdout:
        # Encode safely for the terminal, replacing unencodable characters
        # (e.g. on GBK/CP936 Windows terminals when embedding binary paths).
        try:
            print(line, end="", flush=True)
        except UnicodeEncodeError:
            print(line.encode("ascii", errors="replace").decode("ascii"),
                  end="", flush=True)
    proc.wait()
    if proc.returncode != 0:
        die(f"command failed with exit code {proc.returncode}: {' '.join(cmd)}")


def detect_platform() -> str:
    system = platform.system().lower()
    if system == "windows":
        return "windows"
    if system == "linux":
        return "linux"
    if system == "darwin":
        return "macos"
    die(f"unsupported platform: {system}")


def _cmake_help() -> str:
    try:
        result = subprocess.run(
            ["cmake", "--help"], capture_output=True, text=True, check=False
        )
    except FileNotFoundError:
        die("cmake was not found on PATH (install CMake >= 3.22)")
    return result.stdout


def generator_available(name: str) -> bool:
    return name.lower() in _cmake_help().lower()


def _generator_year(name: str) -> int:
    for token in reversed(name.split()):
        if token.isdigit():
            return int(token)
    return 0


def list_visual_studio_generators() -> List[str]:
    names: List[str] = []
    for line in _cmake_help().splitlines():
        if line.startswith("  ") and " = " in line:
            name = line.split("=", 1)[0].strip()
            if name.startswith("Visual Studio") and name not in names:
                names.append(name)
    return sorted(names, key=_generator_year, reverse=True)


def pick_generator(platform_name: str) -> str:
    """Return the best CMake -G value for *platform_name*.

    On Windows we return an empty string so CMake auto-detects the installed
    Visual Studio version.  ``cmake --help`` lists every generator CMake knows
    about, not just the ones that are actually installed, so picking a specific
    Visual Studio version by name often fails when the user has a different
    edition (e.g. VS 2026 instead of VS 2022).
    """
    if platform_name == "windows":
        # Let CMake auto-detect — it uses vswhere internally and always finds
        # the right VS install.
        return ""
    if generator_available("Ninja"):
        return "Ninja"
    return "Unix Makefiles"


def is_multi_config(generator: str, platform_name: str = "") -> bool:
    # Empty generator means CMake auto-detects — on Windows that always
    # picks Visual Studio, which is a multi-config generator.
    if not generator:
        return platform_name == "windows"
    lowered = generator.lower()
    return any(tag in lowered for tag in MULTI_CONFIG_GENERATORS)


def default_triplet(platform_name: str) -> str:
    machine = platform.machine().lower()
    if platform_name == "windows":
        return "x64-windows"
    if platform_name == "linux":
        return "arm64-linux" if machine in ("aarch64", "arm64") else "x64-linux"
    return "arm64-osx" if machine in ("aarch64", "arm64") else "x64-osx"


def find_vcpkg_root() -> Optional[Path]:
    env = os.environ.get("VCPKG_ROOT")
    if env:
        root = Path(env)
        if root.is_dir():
            return root
    for candidate in (
        PROJECT_ROOT.parent / "vcpkg",
        PROJECT_ROOT / "vcpkg",
        Path.home() / "vcpkg",
    ):
        if (candidate / "scripts" / "buildsystems" / "vcpkg.cmake").is_file():
            return candidate
    return None


def vcpkg_toolchain_path(vcpkg_root: Path) -> Path:
    toolchain = vcpkg_root / "scripts" / "buildsystems" / "vcpkg.cmake"
    if not toolchain.is_file():
        die(f"vcpkg toolchain not found at {toolchain}")
    return toolchain


def is_interactive() -> bool:
    try:
        return bool(sys.stdin) and sys.stdin.isatty()
    except (AttributeError, ValueError):
        return False


def prompt_yes_no(question: str, default: bool = False) -> bool:
    suffix = " [Y/n]: " if default else " [y/N]: "
    while True:
        try:
            answer = input(question + suffix).strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            return default
        if answer in ("y", "yes"):
            return True
        if answer in ("n", "no"):
            return False
        if answer == "":
            return default


def install_vcpkg(destination: Path, platform_name: str) -> Path:
    toolchain = destination / "scripts" / "buildsystems" / "vcpkg.cmake"
    if toolchain.is_file():
        log(f"vcpkg already present at {destination}; reusing it.")
        return destination
    if shutil.which("git") is None:
        die("git is required to install vcpkg (install Git and retry).")
    log(f"Cloning vcpkg into {destination} ...")
    destination.parent.mkdir(parents=True, exist_ok=True)
    run([
        "git", "clone", "--depth", "1",
        "https://github.com/microsoft/vcpkg.git", str(destination),
    ])
    bootstrap = destination / (
        "bootstrap-vcpkg.bat" if platform_name == "windows" else "bootstrap-vcpkg.sh"
    )
    log(f"Bootstrapping vcpkg ({bootstrap.name}) ...")
    run([str(bootstrap), "-disableMetrics"], shell=platform_name == "windows")
    return destination


# ---------------------------------------------------------------------------
# Dependency linkage helpers
# ---------------------------------------------------------------------------

def load_dep_config() -> Optional[dict]:
    """Return the saved dependency config, or None."""
    if not DEPS_CONFIG.is_file():
        return None
    try:
        return json.loads(DEPS_CONFIG.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


def deps_available() -> bool:
    """True when dependencies/install/ exists and looks complete."""
    sdl_h = DEPS_INSTALL / "include" / "SDL3" / "SDL.h"
    txml_h = DEPS_INSTALL / "include" / "tinyxml2.h"
    return sdl_h.is_file() and txml_h.is_file()


# ---------------------------------------------------------------------------
# CMake configure / build wrappers
# ---------------------------------------------------------------------------

def configure(platform_name: str, args: argparse.Namespace, build_dir: Path) -> None:
    cmd = ["cmake", "-S", str(PROJECT_ROOT), "-B", str(build_dir)]

    if args.generator:
        cmd += ["-G", args.generator]

    # On Windows with no explicit generator, CMake auto-detects Visual Studio
    # which is multi-config — skip CMAKE_BUILD_TYPE in that case.
    multi = is_multi_config(args.generator, platform_name)
    if not multi:
        cmd += [f"-DCMAKE_BUILD_TYPE={args.config}"]

    # --- Dependency prefix --------------------------------------------------
    if args.toolchain:
        cmd += [f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain}"]
        if args.triplet:
            cmd += [f"-DVCPKG_TARGET_TRIPLET={args.triplet}"]
    elif deps_available():
        # Use locally-built dependencies from download_dep.py.
        log(f"Using local dependencies: {DEPS_INSTALL}")
        cmd += [f"-DCMAKE_PREFIX_PATH={DEPS_INSTALL}"]
        shared = args.shared if args.shared is not None else args.static is False
        cmd += [f"-DBUILD_SHARED_LIBS={'ON' if shared else 'OFF'}"]
    elif args.toolchain is None and not args.no_deps:
        # Try vcpkg as a fallback.
        vcpkg_root = find_vcpkg_root()
        if vcpkg_root:
            args.toolchain = str(vcpkg_toolchain_path(vcpkg_root))
            cmd += [f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain}"]
            log(f"vcpkg toolchain: {args.toolchain}")
            if args.triplet is None:
                args.triplet = default_triplet(platform_name)
            cmd += [f"-DVCPKG_TARGET_TRIPLET={args.triplet}"]
        else:
            die(
                "No dependencies found.\n\n"
                "Run one of the following first:\n"
                f"  python download_dep.py          (recommended – local build)\n"
                "  vcpkg install sdl3 tinyxml2     (system package manager)\n"
            )

    for definition in args.defines:
        cmd += [f"-D{definition}"]

    run(cmd)


def build(args: argparse.Namespace, build_dir: Path, platform_name: str = "") -> None:
    cmd = ["cmake", "--build", str(build_dir)]

    if args.target:
        cmd += ["--target", args.target]

    multi = is_multi_config(args.generator, platform_name)
    if args.config and multi:
        cmd += ["--config", args.config]

    if args.jobs:
        cmd += ["--parallel", str(args.jobs)]
    if args.verbose:
        cmd += ["--verbose"]

    run(cmd)


def build_with_preset(args: argparse.Namespace) -> None:
    run(["cmake", "--preset", args.preset])
    cmd = ["cmake", "--build", "--preset", args.preset]
    if args.config:
        cmd += ["--config", args.config]
    if args.jobs:
        cmd += ["--parallel", str(args.jobs)]
    if args.verbose:
        cmd += ["--verbose"]
    run(cmd)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Cross-platform CMake build driver for the MuluUI SDK.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-c", "--config",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
        help="CMake build configuration",
    )
    parser.add_argument(
        "-g", "--generator", default=None,
        help="CMake generator (default: auto-detected per platform)",
    )
    parser.add_argument(
        "-j", "--jobs", type=int, default=None,
        help="Number of parallel build jobs (default: CMake default)",
    )
    parser.add_argument(
        "-b", "--build-dir", default=str(BUILD_DIR_DEFAULT),
        help="CMake build directory",
    )
    parser.add_argument(
        "--clean", action="store_true",
        help="Wipe the build directory before configuring",
    )
    parser.add_argument(
        "--target", default=None,
        help="Build only this CMake target (e.g. hello, MuluUI)",
    )
    parser.add_argument(
        "--preset", default=None,
        help="Configure/build through a named CMakePresets.json preset",
    )
    parser.add_argument(
        "--toolchain", default=None,
        help="Path to a CMake toolchain file (overrides local deps + vcpkg)",
    )
    parser.add_argument(
        "--no-deps", action="store_true",
        help="Do not use local dependencies or vcpkg (core library only)",
    )
    parser.add_argument(
        "--install-deps", action="store_true",
        help="Install vcpkg automatically (no prompt) if no deps found",
    )
    parser.add_argument(
        "--triplet", default=None,
        help="vcpkg target triplet (only used with vcpkg toolchain)",
    )
    parser.add_argument(
        "-D", "--define", dest="defines", action="append", default=[],
        metavar="VAR=VAL",
        help="Extra CMake cache variable (repeatable)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true",
        help="Verbose build output",
    )

    # Linkage overrides (only meaningful with local deps).
    linkage = parser.add_mutually_exclusive_group()
    linkage.add_argument(
        "--shared", action="store_true", default=None,
        help="Force shared linkage (overrides download_dep.py config)",
    )
    linkage.add_argument(
        "--static", action="store_true", default=None,
        help="Force static linkage (overrides download_dep.py config)",
    )

    args = parser.parse_args()

    platform_name = detect_platform()
    log(f"Platform: {platform_name} ({platform.system()} {platform.machine()})")

    # ------------------------------------------------------------------
    # Build directory setup
    # ------------------------------------------------------------------
    build_dir = Path(args.build_dir).resolve()

    if args.clean and build_dir.exists():
        log(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)

    # ------------------------------------------------------------------
    # Preset mode
    # ------------------------------------------------------------------
    if args.preset:
        build_with_preset(args)
        log(f"Build complete (preset '{args.preset}').")
        return

    # ------------------------------------------------------------------
    # Generator selection
    # ------------------------------------------------------------------
    if args.generator is None:
        args.generator = pick_generator(platform_name)
    log(f"Generator: {args.generator}")

    # ------------------------------------------------------------------
    # Resolve shared/static from saved config if not overridden
    # ------------------------------------------------------------------
    if args.shared is None and args.static is None:
        cfg = load_dep_config()
        if cfg and "linkage" in cfg:
            if cfg["linkage"] == "shared":
                args.shared = True
            else:
                args.static = True
            log(f"Linkage from saved config: {cfg['linkage']}")

    if args.shared:
        log("Linkage: shared (.dll / .so)")
    elif args.static:
        log("Linkage: static (.lib / .a)")
    else:
        log("Linkage: not specified (shared by default)")

    # ------------------------------------------------------------------
    # Dependency detection
    # ------------------------------------------------------------------
    if args.no_deps:
        log("--no-deps: building core library only (no platform backend)")
    elif deps_available():
        log(f"Local dependencies found: {DEPS_INSTALL}")
    elif args.toolchain:
        log(f"Using explicit toolchain: {args.toolchain}")
    else:
        vcpkg_root = find_vcpkg_root()
        if vcpkg_root:
            log(f"vcpkg found at {vcpkg_root}")
        else:
            log("No dependency source found.")
            if args.install_deps:
                vcpkg_root = install_vcpkg(
                    PROJECT_ROOT / "vcpkg", platform_name
                )
                args.toolchain = str(vcpkg_toolchain_path(vcpkg_root))
                log(f"vcpkg installed; toolchain: {args.toolchain}")
            elif is_interactive():
                print()
                print("No dependencies found. You have two options:")
                print(f"  1. Run download_dep.py to build them locally")
                print(f"     (recommended – self-contained, portable).")
                print(f"  2. Install vcpkg and use it as the package manager.")
                print()
                choice = input("Choose [1/2] (default: 1): ").strip()
                if choice == "2":
                    if prompt_yes_no("Install vcpkg now?"):
                        vcpkg_root = install_vcpkg(
                            PROJECT_ROOT / "vcpkg", platform_name
                        )
                        args.toolchain = str(vcpkg_toolchain_path(vcpkg_root))
                        log(f"vcpkg installed; toolchain: {args.toolchain}")
                    else:
                        die(
                            "Cannot build without dependencies. "
                            "Run download_dep.py first, or install vcpkg."
                        )
                else:
                    die(
                        "Run download_dep.py first to build dependencies locally:\n"
                        "  python download_dep.py\n"
                        "Then re-run build_sdk.py."
                    )
            else:
                die(
                    "No dependencies found in non-interactive mode.\n"
                    "Run download_dep.py first, or pass --install-deps."
                )

    # ------------------------------------------------------------------
    # Triplet
    # ------------------------------------------------------------------
    if args.triplet is None and args.toolchain:
        args.triplet = default_triplet(platform_name)

    configure(platform_name, args, build_dir)
    build(args, build_dir, platform_name)
    log(f"Build complete. Outputs in: {build_dir}")


if __name__ == "__main__":
    main()
