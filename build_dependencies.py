#!/usr/bin/env python3
"""Build the checked-out oneTBB, OpenUSD and PhysX submodules (Python 3.9+).

Windows x64: Python, CMake and Visual Studio 2026 with C++/Windows SDK.
  python build_dependencies.py
  python build_dependencies.py --config Debug --generator "Visual Studio 17 2022"
Windows Ninja builds require an x64 MSVC developer terminal.
Linux x86_64: Python, CMake, Clang and make (or Ninja with --generator Ninja).
  python3 build_dependencies.py

Initialize sources first: git submodule update --init --recursive
Default: Release, all available logical CPUs, all three submodules.
Builds OpenUSD C++ core (no imaging/Python/usdview), PhysX CPU libraries
(shared on Windows, static on Linux). --usd-python enables Python bindings
and requires Python development headers/libraries and Jinja2.

Build/install directories: build/dependencies/<platform>/<config>/{build,install}.
PhysX also writes binaries under thirdParty/PhysX/physx/bin, matching the
existing Windows output layout. VulkanLab consumes all three installed SDKs
via cmake/Dependencies.cmake. Select the same VULKANLAB_DEPENDENCY_CONFIG
when configuring the application; set VULKANLAB_DEPENDENCIES_DIR when using
a custom --output-dir. Windows application builds deploy dependency DLLs.

Use --dry-run to print commands. Re-running resumes incremental builds.
Use a different --output-dir when changing compiler or generator.
"""

import argparse
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
DEFAULT_CONFIG = "Release"
DEFAULT_JOBS = (getattr(os, "process_cpu_count", os.cpu_count)() or 1)
DEFAULT_TARGETS = ("onetbb", "openusd", "physx")


def run(command, dry_run):
    command = [str(value) for value in command]
    print("\n> " + (subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)), flush=True)
    if not dry_run:
        # Normalize case-insensitive Windows keys (some hosts supply PATH and Path).
        environment = {key.upper(): value for key, value in os.environ.items()} if os.name == "nt" else dict(os.environ)
        subprocess.run(command, cwd=ROOT, env=environment, check=True)


def positive_int(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return number


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", choices=("Debug", "Release"), default=DEFAULT_CONFIG)
    parser.add_argument("--jobs", type=positive_int, default=DEFAULT_JOBS, help="parallel jobs (default: all available logical CPUs)")
    parser.add_argument("--targets", nargs="+", choices=DEFAULT_TARGETS, default=list(DEFAULT_TARGETS))
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build" / "dependencies")
    parser.add_argument("--generator", help="default: Visual Studio 18 2026 on Windows, Unix Makefiles on Linux")
    parser.add_argument("--cmake", default="cmake", help="CMake executable")
    parser.add_argument("--cc", default="clang", help="Linux C compiler")
    parser.add_argument("--cxx", default="clang++", help="Linux C++ compiler")
    parser.add_argument("--usd-python", action="store_true", help="enable OpenUSD Python bindings")
    parser.add_argument("--dry-run", action="store_true", help="print commands without building or creating directories")
    return parser.parse_args(argv)


def build(args):
    system = platform.system()
    if system not in ("Windows", "Linux"):
        raise RuntimeError("Supported platforms: Windows and Linux")
    if platform.machine().lower() not in ("amd64", "x86_64"):
        raise RuntimeError("This script currently supports native x86_64 builds")
    windows = system == "Windows"
    generator = args.generator or ("Visual Studio 18 2026" if windows else "Unix Makefiles")
    targets = set(args.targets)
    if "openusd" in targets:
        targets.add("onetbb")
    sources = {"onetbb": ROOT / "thirdParty/oneTBB", "openusd": ROOT / "thirdParty/OpenUSD",
               "physx": ROOT / "thirdParty/PhysX/physx"}
    for name in targets:
        if not (sources[name] / "CMakeLists.txt").is_file():
            raise RuntimeError(f"Missing {name} source. Run: git submodule update --init --recursive")
    if not args.dry_run:
        for executable in [args.cmake] + ([] if windows else [args.cc, args.cxx]):
            if shutil.which(executable) is None:
                raise RuntimeError(f"Executable not found: {executable}")

    output = args.output_dir.resolve() / f"{system.lower()}-x86_64" / args.config.lower()
    install = output / "install"
    tbb_prefix = install / "oneTBB"
    tbb_config = tbb_prefix / "lib/cmake/TBB"
    common = ["-G", generator]
    if windows and generator.startswith("Visual Studio"):
        common += ["-A", "x64"]
    if not windows:
        common += [f"-DCMAKE_C_COMPILER={args.cc}", f"-DCMAKE_CXX_COMPILER={args.cxx}"]

    for name in ("onetbb", "openusd", "physx"):
        if name not in targets:
            continue
        config = args.config.lower() if name == "physx" else args.config
        prefix = install / {"onetbb": "oneTBB", "openusd": "OpenUSD", "physx": "PhysX"}[name]
        build_dir = output / "build" / ("physx-sdk" if name == "physx" else name)
        options = [f"-DCMAKE_BUILD_TYPE={config}", f"-DCMAKE_INSTALL_PREFIX={prefix.as_posix()}"]
        if name == "onetbb":
            options += ["-DTBB_TEST=OFF", "-DTBB_STRICT=OFF", "-DTCM_BUILD=OFF", "-DTBB_INSTALL=ON", "-DCMAKE_INSTALL_LIBDIR=lib"]
        elif name == "openusd":
            if not args.dry_run and not (tbb_config / "TBBConfig.cmake").is_file():
                raise RuntimeError(f"oneTBB package was not installed at {tbb_config}")
            options += [f"-DTBB_DIR={tbb_config.as_posix()}", "-DPXR_FIND_TBB_IN_CONFIG=ON",
                        "-DBUILD_SHARED_LIBS=ON", "-DPXR_BUILD_TESTS=OFF", "-DPXR_BUILD_EXAMPLES=OFF",
                        "-DPXR_BUILD_TUTORIALS=OFF", "-DPXR_BUILD_IMAGING=OFF", "-DPXR_BUILD_USD_IMAGING=OFF",
                        "-DPXR_BUILD_USDVIEW=OFF", "-DPXR_BUILD_DOCUMENTATION=OFF",
                        f"-DPXR_ENABLE_PYTHON_SUPPORT={'ON' if args.usd_python else 'OFF'}"]
            if args.usd_python:
                options.append(f"-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}")
            if not windows:
                options.append(f"-DCMAKE_INSTALL_RPATH={prefix.as_posix()}/lib;{tbb_prefix.as_posix()}/lib")
        else:
            physx_root = ROOT / "thirdParty/PhysX/physx"
            options += [f"-DPHYSX_ROOT_DIR={physx_root.as_posix()}", f"-DTARGET_BUILD_PLATFORM={system.lower()}",
                        f"-DPX_OUTPUT_LIB_DIR={physx_root.as_posix()}", f"-DPX_OUTPUT_BIN_DIR={physx_root.as_posix()}",
                        "-DCMAKE_INSTALL_LIBDIR=lib", "-DPX_OUTPUT_ARCH=x86", "-DPX_GENERATE_GPU_PROJECTS=OFF", "-DPX_GENERATE_GPU_PROJECTS_ONLY=OFF",
                        "-DPX_BUILDSNIPPETS=OFF", "-DPX_BUILDPVDRUNTIME=OFF", "-DPX_CMAKE_SUPPRESS_REGENERATION=ON",
                        f"-DPX_GENERATE_STATIC_LIBRARIES={'OFF' if windows else 'ON'}"]
            if windows:
                options += ["-DNV_USE_STATIC_WINCRT=OFF", "-DNV_USE_DEBUG_WINCRT=ON"]

        run([args.cmake, "-S", sources[name], "-B", build_dir] + common + options, args.dry_run)
        if name == "openusd" and not args.dry_run:
            # Verify the actual package selection even when reusing a CMake cache.
            cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
            selected = next((line.split("=", 1)[1] for line in cache.splitlines() if line.startswith("TBB_DIR:")), "")
            if not selected or Path(selected).resolve() != tbb_config.resolve():
                raise RuntimeError(f"OpenUSD selected unexpected TBB_DIR: {selected}")
        run([args.cmake, "--build", build_dir, "--config", config, "--parallel", args.jobs], args.dry_run)
        run([args.cmake, "--install", build_dir, "--config", config], args.dry_run)

    print(f"\n{'Planned' if args.dry_run else 'Completed'}. Install root: {install}")


def main(argv=None):
    args = parse_args(argv)
    try:
        build(args)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"\nBuild failed: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nBuild interrupted; rerun the same command to resume.", file=sys.stderr)
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
