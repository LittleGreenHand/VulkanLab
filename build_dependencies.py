#!/usr/bin/env python3
"""
构建并安装 oneTBB、OpenUSD 和 PhysX 子模块。默认情况下，每个子模块把 SDK 安装到自身的 `thirdParty/<库名>/install/<平台>/<配置>/` 目录，例如：
thirdParty/oneTBB/install/windows-x86_64/release/
thirdParty/OpenUSD/install/windows-x86_64/release/
thirdParty/PhysX/install/windows-x86_64/release/

已成功安装的包会复用；添加 --rebuild 参数可重新生成。
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
SDK_ROOTS = {"onetbb": ROOT / "thirdParty/oneTBB", "openusd": ROOT / "thirdParty/OpenUSD", "physx": ROOT / "thirdParty/PhysX"}
BUILD_MARKER = ".vulkanlab-build-directory"
INSTALL_MARKER = ".vulkanlab-install-complete"
SYMBOLS_MARKER = ".vulkanlab-physx-pdb-preserved"


def run(command, dry_run):
    command = [str(value) for value in command]
    print("\n> " + (subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)), flush=True)
    if not dry_run:
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
    parser.add_argument("--generator", help="default: Visual Studio 18 2026 on Windows, Unix Makefiles on Linux")
    parser.add_argument("--cmake", default="cmake", help="CMake executable")
    parser.add_argument("--cc", default="clang", help="Linux C compiler")
    parser.add_argument("--cxx", default="clang++", help="Linux C++ compiler")
    parser.add_argument("--usd-python", action="store_true", help="enable OpenUSD Python bindings")
    parser.add_argument("--rebuild", action="store_true", help="rebuild SDKs even when previously installed")
    parser.add_argument("--dry-run", action="store_true", help="print commands without building or creating directories")
    return parser.parse_args(argv)


def sdk_paths(name, platform_tag, config):
    sdk_dir = SDK_ROOTS[name] / "install"
    segment = Path(platform_tag) / config.lower()
    prefix = sdk_dir / segment
    build_dir = sdk_dir / ".build" / segment
    return prefix, build_dir


def package_config(name, prefix):
    if name == "onetbb":
        return prefix / "lib/cmake/TBB/TBBConfig.cmake"
    if name == "openusd":
        return prefix / "pxrConfig.cmake"
    return prefix / "lib/cmake/PhysX/PhysXConfig.cmake"


def prepare_build_directory(build_dir):
    if build_dir.is_symlink():
        raise RuntimeError(f"Refusing to build into symbolic link: {build_dir}")
    marker = build_dir / BUILD_MARKER
    if build_dir.exists() and not marker.is_file():
        raise RuntimeError(f"Refusing to reuse unrecognized build directory: {build_dir}")
    build_dir.mkdir(parents=True, exist_ok=True)
    marker.write_text("Managed by VulkanLab build_dependencies.py\n", encoding="utf-8")


def clean_build_directory(build_dir):
    if build_dir.is_symlink() or not (build_dir / BUILD_MARKER).is_file():
        raise RuntimeError(f"Refusing to remove unrecognized build directory: {build_dir}")
    shutil.rmtree(build_dir)
    print(f"Removed temporary build: {build_dir}")


def preserve_physx_pdbs(prefix, build_dir, config):
    """Preserve MSVC PDBs referenced by installed PhysX .lib object files.

    MSVC LINK searches for the matching PDB alongside the static library. PhysX
    may generate these PDBs under its separate output directory or CMake build
    tree. Preserve only names matching installed libraries to conserve disk.
    """
    pdbs = {}
    for path in build_dir.rglob("*"):
        if path.is_file() and not path.is_symlink() and path.suffix.lower() == ".pdb":
            pdbs.setdefault(path.stem.casefold(), []).append(path)

    copied = 0
    libraries = [path for path in prefix.rglob("*") if path.is_file() and path.suffix.lower() == ".lib"]
    for library in libraries:
        matches = pdbs.get(library.stem.casefold(), [])
        if not matches:
            continue
        # Prefer PDBs from the selected CMake configuration when duplicates exist.
        selected = sorted(matches, key=lambda path: (
            not any(part.casefold() == config.casefold() for part in path.relative_to(build_dir).parts),
            "physx_artifacts" not in (part.casefold() for part in path.relative_to(build_dir).parts),
            len(path.parts), str(path).casefold()
        ))[0]
        destination = library.with_suffix(".pdb")
        if destination.is_symlink():
            raise RuntimeError(f"Refusing to overwrite a symbolic link: {destination}")
        shutil.copy2(selected, destination)
        if not destination.is_file() or destination.stat().st_size != selected.stat().st_size:
            raise RuntimeError(f"Could not verify installed PhysX PDB: {destination}")
        copied += 1
        print(f"Preserved PhysX PDB: {destination}")

    if not copied:
        print("Warning: No matching PhysX PDB files were generated for installed libraries; "
              "check MSVC debug information settings if LNK4099 persists.")
    else:
        print(f"Preserved {copied} PhysX PDB file(s) beside installed .lib files.")
    return copied


def build(args):
    system = platform.system()
    if system not in ("Windows", "Linux"):
        raise RuntimeError("Supported platforms: Windows and Linux")
    if platform.machine().lower() not in ("amd64", "x86_64"):
        raise RuntimeError("This script currently supports native x86_64 builds")
    windows = system == "Windows"
    platform_tag = f"{system.lower()}-x86_64"
    generator = args.generator or ("Visual Studio 18 2026" if windows else "Unix Makefiles")
    targets = set(args.targets)
    if "openusd" in targets:
        targets.add("onetbb")
    sources = {"onetbb": SDK_ROOTS["onetbb"], "openusd": SDK_ROOTS["openusd"], "physx": SDK_ROOTS["physx"] / "physx"}
    for name in targets:
        if not (sources[name] / "CMakeLists.txt").is_file():
            raise RuntimeError(f"Missing {name} source. Run: git submodule update --init --recursive")
    if not args.dry_run:
        for executable in [args.cmake] + ([] if windows else [args.cc, args.cxx]):
            if shutil.which(executable) is None:
                raise RuntimeError(f"Executable not found: {executable}")

    tbb_prefix, _ = sdk_paths("onetbb", platform_tag, args.config)
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
        prefix, build_dir = sdk_paths(name, platform_tag, args.config)
        package = package_config(name, prefix)
        installed_marker = prefix / INSTALL_MARKER
        if installed_marker.is_file() and package.is_file() and not args.rebuild:
            # An older Windows PhysX installation may have lost its PDBs when
            # the temporary build tree was deleted. Rebuild it once to repair.
            if not (windows and name == "physx" and not (prefix / SYMBOLS_MARKER).is_file()):
                print(f"Already installed: {name} at {prefix} (use --rebuild to rebuild)")
                continue
            print("Old PhysX install has no PDB-preservation marker; rebuilding to restore symbols.")

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
                options.append("-DCMAKE_CXX_FLAGS=-include algorithm")
        else:
            physx_root = SDK_ROOTS["physx"] / "physx"
            physx_artifacts = build_dir / "physx_artifacts"
            options += [f"-DPHYSX_ROOT_DIR={physx_root.as_posix()}", f"-DTARGET_BUILD_PLATFORM={system.lower()}",
                        f"-DPX_OUTPUT_LIB_DIR={physx_artifacts.as_posix()}", f"-DPX_OUTPUT_BIN_DIR={physx_artifacts.as_posix()}",
                        "-DCMAKE_INSTALL_LIBDIR=lib", "-DPX_OUTPUT_ARCH=x86", "-DPX_GENERATE_GPU_PROJECTS=OFF", "-DPX_GENERATE_GPU_PROJECTS_ONLY=OFF",
                        "-DPX_BUILDSNIPPETS=OFF", "-DPX_BUILDPVDRUNTIME=OFF", "-DPX_CMAKE_SUPPRESS_REGENERATION=ON",
                        f"-DPX_GENERATE_STATIC_LIBRARIES={'OFF' if windows else 'ON'}"]
            if windows:
                options += ["-DNV_USE_STATIC_WINCRT=OFF", "-DNV_USE_DEBUG_WINCRT=ON"]

        if not args.dry_run:
            prepare_build_directory(build_dir)
            installed_marker.unlink(missing_ok=True)
        run([args.cmake, "-S", sources[name], "-B", build_dir] + common + options, args.dry_run)
        if name == "openusd" and not args.dry_run:
            cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
            selected = next((line.split("=", 1)[1] for line in cache.splitlines() if line.startswith("TBB_DIR:")), "")
            if not selected or Path(selected).resolve() != tbb_config.resolve():
                raise RuntimeError(f"OpenUSD selected unexpected TBB_DIR: {selected}")
        run([args.cmake, "--build", build_dir, "--config", config, "--parallel", args.jobs], args.dry_run)
        run([args.cmake, "--install", build_dir, "--config", config], args.dry_run)
        if not args.dry_run:
            if not package.is_file():
                raise RuntimeError(f"Missing installed CMake package after install: {package}")
            if windows and name == "physx":
                preserve_physx_pdbs(prefix, build_dir, args.config)
                (prefix / SYMBOLS_MARKER).write_text("PhysX PDB collection checked\n", encoding="utf-8")
            installed_marker.write_text("Installed by VulkanLab build_dependencies.py\n", encoding="utf-8")
            clean_build_directory(build_dir)
        print(f"{'Would install' if args.dry_run else 'Installed'} {name}: {prefix}")

    print(f"\n{'Planned' if args.dry_run else 'Completed'} all selected SDKs.")


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
