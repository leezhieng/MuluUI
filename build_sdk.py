#!/usr/bin/env python3
"""Cross-platform CMake build driver for the MuluUI SDK.

Detects the running platform and runs the appropriate CMake configure +
build steps, wiring in the vcpkg toolchain automatically when available
(required on Windows for the WinUI3 / Windows App SDK dependency).

Usage examples:
    python build_sdk.py                    # Release, auto-detected generator
    python build_sdk.py -c Debug -j 8
    python build_sdk.py -g Ninja --clean
    python build_sdk.py --target hello -DCMAKE_BUILD_TYPE=Debug
    python build_sdk.py --preset windows   # use a CMakePresets.json preset
    python build_sdk.py --no-vcpkg         # build the core without vcpkg deps

Platform -> generator mapping:
    Windows : Visual Studio 17 2022 (falls back to Ninja)
    Linux   : Ninja (falls back to Unix Makefiles)
    macOS   : Ninja (falls back to Unix Makefiles)
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, NoReturn, Optional

PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_DIR_DEFAULT = PROJECT_ROOT / "build"

# Generators that build every configuration at once (vs. single-config ones).
MSVC_GENERATORS = ["Visual Studio 17 2022", "Visual Studio 16 2019"]
MULTI_CONFIG_GENERATORS = ("visual studio", "ninja multi-config")


def log(message: str) -> None:
    print(f"[build_sdk] {message}", flush=True)


def die(message: str) -> NoReturn:
    print(f"[build_sdk] ERROR: {message}", file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd: List[str], cwd: Optional[Path] = None) -> None:
    log("$ " + " ".join(cmd))
    try:
        result = subprocess.run(cmd, cwd=str(cwd) if cwd else None)
    except FileNotFoundError:
        die(f"command not found: {cmd[0]}. Is it installed and on PATH?")
    if result.returncode != 0:
        die(f"command failed with exit code {result.returncode}: {' '.join(cmd)}")


def detect_platform() -> str:
    system = platform.system().lower()
    if system == "windows":
        return "windows"
    if system == "linux":
        return "linux"
    if system == "darwin":
        return "macos"
    die(f"unsupported platform: {system}")


def generator_available(name: str) -> bool:
    try:
        result = subprocess.run(
            ["cmake", "--help"], capture_output=True, text=True, check=False
        )
    except FileNotFoundError:
        die("cmake was not found on PATH (install CMake >= 3.22)")
    return name.lower() in result.stdout.lower()


def pick_generator(platform_name: str) -> str:
    if platform_name == "windows":
        for gen in MSVC_GENERATORS:
            if generator_available(gen):
                return gen
        if generator_available("Ninja"):
            log("No Visual Studio generator found; using Ninja instead.")
            return "Ninja"
        die("no supported CMake generator found (install Visual Studio 2022 or Ninja)")
    # Linux / macOS
    if generator_available("Ninja"):
        return "Ninja"
    return "Unix Makefiles"


def is_multi_config(generator: str) -> bool:
    lowered = generator.lower()
    return any(tag in lowered for tag in MULTI_CONFIG_GENERATORS)


def default_triplet(platform_name: str) -> str:
    machine = platform.machine().lower()
    if platform_name == "windows":
        return "x64-windows"
    if platform_name == "linux":
        return "arm64-linux" if machine in ("aarch64", "arm64") else "x64-linux"
    # macOS
    return "arm64-osx" if machine in ("aarch64", "arm64") else "x64-osx"


def find_vcpkg_root() -> Optional[Path]:
    env = os.environ.get("VCPKG_ROOT")
    if env:
        root = Path(env)
        if root.is_dir():
            return root
    # Fall back to common vcpkg checkout locations.
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


def configure(platform_name: str, args: argparse.Namespace, build_dir: Path) -> None:
    cmd = ["cmake", "-S", str(PROJECT_ROOT), "-B", str(build_dir)]

    if args.generator:
        cmd += ["-G", args.generator]

    # Single-config generators need CMAKE_BUILD_TYPE at configure time;
    # multi-config generators take --config at build time instead.
    if not is_multi_config(args.generator):
        cmd += [f"-DCMAKE_BUILD_TYPE={args.config}"]

    if args.toolchain:
        cmd += [f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain}"]
        if args.triplet:
            cmd += [f"-DVCPKG_TARGET_TRIPLET={args.triplet}"]

    for definition in args.defines:
        cmd += [f"-D{definition}"]

    run(cmd)


def build(args: argparse.Namespace, build_dir: Path) -> None:
    cmd = ["cmake", "--build", str(build_dir)]

    if args.target:
        cmd += ["--target", args.target]
    if args.config and is_multi_config(args.generator):
        cmd += ["--config", args.config]
    if args.jobs:
        cmd += ["--parallel", str(args.jobs)]
    if args.verbose:
        cmd += ["--verbose"]

    run(cmd)


def build_with_preset(args: argparse.Namespace) -> None:
    """Configure and build through a CMakePresets.json preset."""
    run(["cmake", "--preset", args.preset])

    cmd = ["cmake", "--build", "--preset", args.preset]
    if args.config:
        cmd += ["--config", args.config]
    if args.jobs:
        cmd += ["--parallel", str(args.jobs)]
    if args.verbose:
        cmd += ["--verbose"]
    run(cmd)


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
        help="Path to a CMake toolchain file (default: auto-detect vcpkg)",
    )
    parser.add_argument(
        "--no-vcpkg", action="store_true",
        help="Do not use the vcpkg toolchain (builds the core only)",
    )
    parser.add_argument(
        "--triplet", default=None,
        help="vcpkg target triplet (default: per-platform, e.g. x64-windows)",
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
    args = parser.parse_args()

    platform_name = detect_platform()
    log(f"Platform: {platform_name} ({platform.system()} {platform.machine()})")

    # ------------------------------------------------------------------
    # Preset mode: delegate everything to CMakePresets.json.
    # ------------------------------------------------------------------
    if args.preset:
        build_with_preset(args)
        log(f"Build complete (preset '{args.preset}').")
        return

    # ------------------------------------------------------------------
    # Generator selection.
    # ------------------------------------------------------------------
    if args.generator is None:
        args.generator = pick_generator(platform_name)
    log(f"Generator: {args.generator}")

    # ------------------------------------------------------------------
    # vcpkg toolchain detection (required for WinUI3 on Windows).
    # ------------------------------------------------------------------
    if args.toolchain is None and not args.no_vcpkg:
        vcpkg_root = find_vcpkg_root()
        if vcpkg_root:
            args.toolchain = str(vcpkg_toolchain_path(vcpkg_root))
            log(f"vcpkg toolchain: {args.toolchain}")
        else:
            if platform_name == "windows":
                die(
                    "vcpkg not found and it is required for the Windows App SDK "
                    "(WinUI3 backend). Install vcpkg and set VCPKG_ROOT, or pass "
                    "--no-vcpkg to build the core library only."
                )
            log("vcpkg not found; building without a toolchain file.")
    elif args.toolchain:
        log(f"Toolchain: {args.toolchain}")

    if args.triplet is None and args.toolchain:
        args.triplet = default_triplet(platform_name)

    build_dir = Path(args.build_dir).resolve()

    if args.clean and build_dir.exists():
        log(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)

    configure(platform_name, args, build_dir)
    build(args, build_dir)

    log(f"Build complete. Outputs in: {build_dir}")


if __name__ == "__main__":
    main()
