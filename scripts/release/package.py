#!/usr/bin/env python3
"""Build client-only release archives and relocate their runtime libraries."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]


def run(*args, env=None):
    return subprocess.check_output([str(arg) for arg in args], text=True, env=env).strip()


def copy_tree(source, destination):
    # Asset submodules contain .git files pointing outside the archive.
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns(
        ".git", ".github", ".DS_Store", "__pycache__"))


def copy_library(source, destination, origins):
    source = source.resolve(strict=True)
    if destination.name in origins and origins[destination.name] != source:
        raise RuntimeError(f"Conflicting runtime library: {destination.name}")
    origins[destination.name] = source
    shutil.copy2(source, destination)
    destination.chmod(0o755)


def mac_dependencies(image):
    return [line.strip().split(" (", 1)[0]
            for line in run("otool", "-L", image).splitlines()[1:]]


def mac_rpaths(image):
    return re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset",
                      run("otool", "-l", image))


def mac_resolve(reference, image, executable, inherited):
    def expand(path):
        return path.replace("@loader_path", str(image.parent)).replace(
            "@executable_path", str(executable.parent))

    paths = [expand(path) for path in mac_rpaths(image)] + list(inherited)
    if reference.startswith("@rpath/"):
        candidates = [Path(path) / reference[7:] for path in paths]
    else:
        candidates = [Path(expand(reference))]
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve(), paths
    raise RuntimeError(f"Unresolved macOS dependency: {reference} in {image}")


def bundle_macos(source, binary, libraries):
    origins, copied, edges = {}, {source.resolve(): binary}, []

    def visit(image, destination, inherited=()):
        for reference in mac_dependencies(image):
            if reference.startswith(("/System/Library/", "/usr/lib/")):
                continue
            dependency, paths = mac_resolve(reference, image, source, inherited)
            if dependency == image.resolve():  # a dylib's own install name
                continue
            if dependency not in copied:
                target = libraries / dependency.name
                copy_library(dependency, target, origins)
                copied[dependency] = target
                visit(dependency, target, paths)
            edges.append((destination, reference, copied[dependency]))

    visit(source.resolve(), binary)
    # Homebrew's SDL2 compatibility layer loads SDL3 with dlopen, so otool
    # cannot discover this dependency. Preserve the name searched by SDL2.
    for name, image in list(origins.items()):
        if name.startswith("libSDL2-") and b"Failed loading SDL3 library" in image.read_bytes():
            libdir = Path(run("pkg-config", "--variable=libdir", "sdl3"))
            dependency = (libdir / "libSDL3.dylib").resolve(strict=True)
            target = libraries / "libSDL3.dylib"
            copy_library(dependency, target, origins)
            copied[dependency] = target
            visit(dependency, target)
    for image, reference, dependency in edges:
        relative = os.path.relpath(dependency, image.parent)
        run("install_name_tool", "-change", reference, f"@loader_path/{relative}", image)
    for image in copied.values():
        if image != binary:
            run("install_name_tool", "-id", f"@rpath/{image.name}", image)
        run("codesign", "--force", "--sign", "-", image)
    for image in copied.values():
        for reference in mac_dependencies(image):
            if reference.startswith("/") and not reference.startswith(("/System/Library/", "/usr/lib/")):
                raise RuntimeError(f"Non-relocatable dependency: {reference}")
    return origins


# These come from the host OS/graphics driver. Shipping them can break GPU loading.
LINUX_SYSTEM = re.compile(
    r"^(ld-linux|lib(c|m|pthread|dl|rt|resolv|util)\.so|"
    r"lib(GL|GLX|GLdispatch|OpenGL|EGL|GLES|vulkan|drm|gbm|X\w*|xcb[\w-]*|wayland[\w-]*)\.)")


def bundle_linux(binary, libraries):
    origins, visited, queue = {}, set(), [binary]
    while queue:
        image = queue.pop()
        if image.resolve() in visited:
            continue
        visited.add(image.resolve())
        listing = run("ldd", image)
        if "not found" in listing:
            raise RuntimeError(f"Missing Linux runtime dependency:\n{listing}")
        for name, path in re.findall(r"^\s*(\S+) => (/\S+) \(", listing, re.M):
            if LINUX_SYSTEM.match(name):
                continue
            target = libraries / name
            copy_library(Path(path), target, origins)
            queue.append(Path(path))
    environment = dict(os.environ, LD_LIBRARY_PATH=str(libraries))
    if "not found" in run("ldd", binary, env=environment):
        raise RuntimeError("Packaged Linux client has unresolved dependencies")
    return origins


def bundle_windows(binary, libraries):
    toolchain = Path(os.environ["MINGW_PREFIX"])
    # MSYS2 Python reports a POSIX prefix; turn it into a native filesystem path.
    toolchain = Path(run("cygpath", "-m", toolchain))
    lookup = {path.name.lower(): path for path in (toolchain / "bin").glob("*.dll")}
    windows = Path(os.environ["WINDIR"]) / "System32"
    system = {path.name.lower() for path in windows.glob("*.dll")}
    origins, visited, queue = {}, set(), [binary]
    while queue:
        image = queue.pop()
        if image.name.lower() in visited:
            continue
        visited.add(image.name.lower())
        for name in re.findall(r"DLL Name: (\S+)", run("objdump", "-p", image)):
            dependency = lookup.get(name.lower())
            if dependency is None:
                if name.lower() in system or name.lower().startswith(("api-ms-", "ext-ms-")):
                    continue
                raise RuntimeError(f"Unresolved Windows dependency: {name}")
            copy_library(dependency, libraries / name, origins)
            queue.append(dependency)
            if name.lower() == "sdl2.dll" and b"Failed loading SDL3 library" in dependency.read_bytes():
                sdl3 = lookup.get("sdl3.dll")
                if sdl3 is None:
                    raise RuntimeError("SDL2 compatibility layer requires SDL3.dll")
                copy_library(sdl3, libraries / "SDL3.dll", origins)
                queue.append(sdl3)
    return origins


def mac_icon(source, resources, work):
    iconset = work / "EclipseRecoil.iconset"
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            suffix = "@2x" if scale == 2 else ""
            run("sips", "-z", size * scale, size * scale, source,
                "--out", iconset / f"icon_{size}x{size}{suffix}.png")
    run("iconutil", "-c", "icns", iconset, "-o", resources / "EclipseRecoil.icns")


def library_notices(origins, destination, platform):
    destination.mkdir(parents=True, exist_ok=True)
    for source in ROOT.joinpath("bin").glob("LICENSE.*"):
        shutil.copy2(source, destination / source.name)
    # Preserve dependency package notices in addition to the game's asset readmes.
    scanned = set()
    for name, source in origins.items():
        locations = []
        if platform == "macos":
            prefix = source.parent.parent
            locations = [prefix]
        elif platform == "linux":
            # usrmerge may resolve /lib to /usr/lib while dpkg records /lib.
            candidates = [source, Path(str(source).replace("/usr/lib/", "/lib/", 1))]
            package = None
            for candidate in candidates:
                try:
                    package = run("dpkg-query", "-S", candidate).split(": ", 1)[0].split(":", 1)[0]
                    break
                except subprocess.CalledProcessError:
                    continue
            if package is None:
                raise RuntimeError(f"Cannot locate dependency license package: {source}")
            locations = [Path("/usr/share/doc") / package]
        else:
            prefix = source.parent.parent
            locations = [prefix / "share/licenses", prefix / "share/doc"]
        for location in locations:
            if not location.is_dir() or location in scanned:
                continue
            scanned.add(location)
            for path in location.rglob("*"):
                if path.is_file() and (path.name.lower().startswith(("license", "copying", "copyright"))):
                    target = destination / name / path.relative_to(location)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, target)


def validate_architecture(binary, platform, arch):
    if platform == "macos":
        expected = "arm64" if arch == "arm64" else "x86_64"
        if run("lipo", "-archs", binary) != expected:
            raise RuntimeError(f"Incorrect macOS binary architecture: {binary}")
    elif platform == "linux":
        expected = "AArch64" if arch == "arm64" else "Advanced Micro Devices X86-64"
        if expected not in run("readelf", "-h", binary):
            raise RuntimeError(f"Incorrect Linux binary architecture: {binary}")
    elif "pei-x86-64" not in run("objdump", "-f", binary):
        raise RuntimeError(f"Incorrect Windows binary architecture: {binary}")


def package(args):
    if args.platform == "windows" and args.arch != "x86_64":
        raise ValueError("Windows packages currently target x86_64")
    source = args.binary.resolve(strict=True)
    validate_architecture(source, args.platform, args.arch)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    name = f"eclipse-recoil-{args.platform}-{args.arch}"
    work = output / f"{name}-staging"
    if work.exists():
        raise RuntimeError(f"Staging directory already exists: {work}")
    work.mkdir()
    if args.platform == "macos":
        top = work / "Eclipse Recoil.app"
        resources = top / "Contents/Resources"
        game = resources / "game"
        binary = top / "Contents/MacOS/eclipse-recoil-bin"
        libraries = top / "Contents/Frameworks"
    else:
        top = game = work / name
        binary = game / ("bin/eclipse-recoil.exe" if args.platform == "windows" else "bin/eclipse-recoil")
        libraries = binary.parent if args.platform == "windows" else game / "lib"
    binary.parent.mkdir(parents=True, exist_ok=True)
    libraries.mkdir(parents=True, exist_ok=True)
    game.mkdir(parents=True, exist_ok=True)
    for folder in ("data", "config", "doc"):
        copy_tree(ROOT / folder, game / folder)
    shutil.copy2(ROOT / "readme.md", game / "README.md")
    required = ["data/maps/echo.mpz", "config/csgopen/tdm.cfg", "config/csgopen/client.cfg",
                "config/csgopen/branding.cfg", "data/csgopen/branding/icon.png",
                "data/csgopen/branding/splash.png", "data/csgopen/branding/logo.png"]
    for path in required:
        if not (game / path).is_file():
            raise RuntimeError(f"Incomplete game package: {path}")
    shutil.copy2(source, binary)
    binary.chmod(0o755)
    bundler = {"macos": bundle_macos, "linux": bundle_linux, "windows": bundle_windows}[args.platform]
    origins = bundler(source, binary, libraries) if args.platform == "macos" else bundler(binary, libraries)
    library_notices(origins, game / "licenses/runtime", args.platform)
    if args.platform == "windows":
        (game / "Eclipse Recoil.bat").write_bytes((ROOT / "scripts/release/launch.bat").read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
    else:
        launcher = game / "eclipse-recoil.sh"
        shutil.copy2(ROOT / "scripts/release/launch.sh", launcher)
        launcher.chmod(0o755)
    if args.platform == "macos":
        launcher = top / "Contents/MacOS/EclipseRecoil"
        launcher.write_text('#!/bin/sh\nROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../Resources/game" && pwd)\nexec "$ROOT/eclipse-recoil.sh" "$@"\n')
        launcher.chmod(0o755)
        with (top / "Contents/Info.plist").open("wb") as stream:
            plistlib.dump({"CFBundleName": "Eclipse Recoil", "CFBundleDisplayName": "Eclipse Recoil",
                          "CFBundleIdentifier": "io.github.agea.eclipse-recoil", "CFBundlePackageType": "APPL",
                          "CFBundleExecutable": "EclipseRecoil", "CFBundleIconFile": "EclipseRecoil.icns",
                          "CFBundleShortVersionString": "0.1.0", "CFBundleVersion": args.build,
                          "LSMinimumSystemVersion": "15.0", "NSHighResolutionCapable": True}, stream)
        mac_icon(ROOT / "data/csgopen/branding/icon.png", resources, work)
    manifest = {"name": "Eclipse Recoil", "platform": args.platform, "architecture": args.arch,
                "commit": args.commit, "build": args.build, "dedicated_server": False,
                "runtime_libraries": sorted(origins)}
    (game / "release.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (game / "PLAY.txt").write_text(
        "Eclipse Recoil — Be kind, reload\n\n"
        "macOS: open Eclipse Recoil.app. Linux: run ./eclipse-recoil.sh.\n"
        "Windows: open Eclipse Recoil.bat. Start an offline TDM match on Echo.\n"
        "Player settings are stored in your user profile, outside the installation.\n"
        "Linux requires glibc 2.35+, X11/Wayland and an OpenGL 3.3-capable graphics driver.\n"
        "macOS requires 15+; this application has an ad-hoc signature, without notarization.\n"
        "Windows requires Windows 10+ x64. No dedicated server is included.\n"
        f"Source: https://github.com/agea/eclipse-recoil/tree/{args.commit}\n")
    for path in top.rglob("*"):
        if ".git" in path.parts or "redeclipse_server" in path.name:
            raise RuntimeError(f"Unexpected package content: {path}")
    # Sign only after every resource has been written.
    if args.platform == "macos":
        run("codesign", "--force", "--deep", "--sign", "-", top)
        run("codesign", "--verify", "--deep", "--strict", top)
    archive = output / (name + (".tar.gz" if args.platform == "linux" else ".zip"))
    if args.platform == "macos":
        run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", top, archive)
    elif args.platform == "windows":
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as stream:
            for path in sorted(top.rglob("*")):
                if path.is_file():
                    stream.write(path, path.relative_to(work))
    else:
        with tarfile.open(archive, "w:gz", compresslevel=6) as stream:
            stream.add(top, arcname=top.name)
    # Hosted runners have limited disk space; don't retain a second asset tree
    # while splitting and uploading the completed archive.
    shutil.rmtree(work)
    publish_files(archive, args.platform, top.name)


def publish_files(archive, platform, top_name, limit=2 * 1024**3, part_size=1500 * 1024**2):
    """Split large archives for GitHub without dropping or changing game assets."""
    size = archive.stat().st_size
    files = [archive]
    if size >= limit:
        files = []
        with archive.open("rb") as source:
            while source.tell() < size:
                target = archive.with_name(f"{archive.name}.{len(files) + 1:03d}")
                with target.open("wb") as output:
                    remaining = part_size
                    while remaining:
                        chunk = source.read(min(1024**2, remaining))
                        if not chunk:
                            break
                        output.write(chunk)
                        remaining -= len(chunk)
                files.append(target)
        if platform == "windows":
            suffixes = (".ps1", ".bat")
        else:
            suffixes = (".command",) if platform == "macos" else (".sh",)
        name = archive.name.removesuffix(".tar.gz").removesuffix(".zip")
        count = len(files)
        for suffix in suffixes:
            template = (ROOT / f"scripts/release/extract{suffix}").read_text()
            script = archive.parent / f"{name}-extract{suffix}"
            for key, value in {"@ARCHIVE@": archive.name, "@COUNT@": str(count),
                               "@NAME@": name, "@TOP@": top_name}.items():
                template = template.replace(key, value)
            script.write_text(template)
            script.chmod(0o755)
            files.append(script)
        # The complete archive is reconstructible from its checked parts.
        archive.unlink()
    name = archive.name.removesuffix(".tar.gz").removesuffix(".zip")
    checksum = archive.parent / f"{name}.files.sha256"
    checksum.write_text("".join(f"{sha256(path)}  {path.name}\n" for path in files))
    print(f"Packaged {archive.name} ({size // 1024**2} MiB; {len(files)} download files)")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=("macos", "linux", "windows"))
    parser.add_argument("--arch", required=True, choices=("arm64", "x86_64"))
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / ".csgopen/release")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--build", required=True)
    package(parser.parse_args())
