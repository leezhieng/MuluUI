#!/usr/bin/env python3
"""Build all MuluUI examples.

Reuses the platform-aware CMake logic from build_sdk.py (generator
selection, dependency detection, linkage config). It:

  1. discovers every example under examples/,
  2. configures the project once,
  3. builds each discovered example target and reports a per-target summary.

Usage examples:
    python build_examples.py                 # build every example (Release)
    python build_examples.py -c Debug -j 8
    python build_examples.py --only hello    # build only the 'hello' example
    python build_examples.py --shared        # force shared linkage
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path
from typing import List, Tuple

import build_sdk as sdk  # reuse the platform-aware CMake driver

PROJECT_ROOT = sdk.PROJECT_ROOT
EXAMPLES_DIR = PROJECT_ROOT / "examples"

# Matches `add_executable(<name> ...)` inside an example CMakeLists.txt.
ADD_EXECUTABLE_RE = re.compile(r"add_executable\s*\(\s*([^\s()]+)")


def log(message: str) -> None:
    print(f"[build_examples] {message}", flush=True)


def discover_examples() -> List[Tuple[str, Path]]:
    """Return [(target_name, example_dir)] for every executable example target."""
    examples: List[Tuple[str, Path]] = []
    if not EXAMPLES_DIR.is_dir():
        return examples

    for entry in sorted(EXAMPLES_DIR.iterdir()):
        if not entry.is_dir():
            continue
        cmake_file = entry / "CMakeLists.txt"
        if not cmake_file.is_file():
            continue

        targets: List[str] = []
        try:
            text = cmake_file.read_text(encoding="utf-8")
            targets = ADD_EXECUTABLE_RE.findall(text)
        except OSError:
            pass

        if not targets:
            log(f"WARNING: no add_executable() found in {cmake_file}; "
                f"falling back to directory name")
            targets = [entry.name]

        for target in targets:
            examples.append((target, entry))

    return examples


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build all MuluUI examples (reuses build_sdk.py logic).",
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
        "-b", "--build-dir", default=str(sdk.BUILD_DIR_DEFAULT),
        help="CMake build directory",
    )
    parser.add_argument(
        "--clean", action="store_true",
        help="Wipe the build directory before configuring",
    )
    parser.add_argument(
        "--only", action="append", default=[],
        help="Build only this example (repeatable; default: all examples)",
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

    platform_name = sdk.detect_platform()
    log(f"Platform: {platform_name} ({sys.platform})")

    # ----------------------------------------------------------------------
    # Discover examples
    # ----------------------------------------------------------------------
    examples = discover_examples()
    if not examples:
        sdk.die(f"no examples found under {EXAMPLES_DIR}")

    if args.only:
        wanted = set(args.only)
        examples = [(name, d) for name, d in examples if name in wanted]
        if not examples:
            sdk.die(
                f"none of the requested examples "
                f"({', '.join(sorted(wanted))}) were found"
            )

    names = ", ".join("'" + name + "'" for name, _ in examples)
    log(f"Examples to build: {names}")

    # ----------------------------------------------------------------------
    # Generator selection
    # ----------------------------------------------------------------------
    if args.generator is None:
        args.generator = sdk.pick_generator(platform_name)
    log(f"Generator: {args.generator}")

    # ----------------------------------------------------------------------
    # Resolve shared/static from saved config if not overridden
    # ----------------------------------------------------------------------
    if args.shared is None and args.static is None:
        cfg = sdk.load_dep_config()
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

    # ----------------------------------------------------------------------
    # Dependency detection
    # ----------------------------------------------------------------------
    if args.toolchain:
        log(f"Using explicit toolchain: {args.toolchain}")
    elif args.no_deps:
        log("--no-deps: building core library only")
    elif sdk.deps_available():
        log(f"Local dependencies found: {sdk.DEPS_INSTALL}")
    else:
        vcpkg_root = sdk.find_vcpkg_root()
        if vcpkg_root:
            log(f"vcpkg found at {vcpkg_root}")
        else:
            sdk.die(
                "No dependencies found.\n\n"
                "Run one of the following first:\n"
                f"  python download_dep.py          (recommended – local build)\n"
                "  vcpkg install sdl3 tinyxml2     (system package manager)\n"
            )

    if args.triplet is None and args.toolchain:
        args.triplet = sdk.default_triplet(platform_name)

    build_dir = Path(args.build_dir).resolve()

    if args.clean and build_dir.exists():
        log(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)

    # ----------------------------------------------------------------------
    # Configure once
    # ----------------------------------------------------------------------
    sdk.configure(platform_name, args, build_dir)

    # ----------------------------------------------------------------------
    # Build every example target
    # ----------------------------------------------------------------------
    failed: List[str] = []
    for target, _ in examples:
        log(f"Building example: {target}")
        build_cmd = ["cmake", "--build", str(build_dir), "--target", target]
        if args.config and sdk.is_multi_config(args.generator, platform_name):
            build_cmd += ["--config", args.config]
        if args.jobs:
            build_cmd += ["--parallel", str(args.jobs)]
        if args.verbose:
            build_cmd += ["--verbose"]

        try:
            sdk.run(build_cmd)
            log(f"OK: {target}")
        except SystemExit:
            log(f"FAILED: {target}")
            failed.append(target)

    total = len(examples)
    ok = total - len(failed)
    log(f"Summary: {ok}/{total} examples built successfully.")
    if failed:
        sdk.die(f"failed examples: {', '.join(failed)}")
    log(f"Example outputs in: {build_dir}")


if __name__ == "__main__":
    main()
