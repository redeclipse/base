"""Regression checks for dependency closure and relocatable release packages."""

import os
from pathlib import Path, PureWindowsPath
import tempfile
import unittest
from unittest.mock import patch

import package as pack


class WindowsFixturePath:
    """Use Windows path spelling while copying real host-local fixture files."""

    def __init__(self, native):
        self.native = native
        self.name = native.name

    def __str__(self):
        return str(PureWindowsPath("C:/fixture") / self.name)

    def resolve(self, **kwargs):
        return self.native.resolve(**kwargs)


class PackageChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.libraries = self.root / "packaged"
        self.libraries.mkdir()

    def file(self, name):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"fixture")
        return path

    def test_linux_keeps_audio_closure_but_uses_host_glibc_and_gpu(self):
        binary = self.file("client")
        sdl = self.file("system/libSDL2.so.0")
        audio = self.file("system/libpulse.so.0")
        # ldd always emits Linux paths, even when this fixture runs on Windows.
        # Map its simulated filesystem to real files in the host's temp folder.
        linux_paths = {"/runtime/libSDL2.so.0": sdl, "/runtime/libpulse.so.0": audio}
        listings = {
            binary: "libSDL2.so.0 => /runtime/libSDL2.so.0 (0x1)\nlibGL.so.1 => /host/libGL.so.1 (0x2)\nlibc.so.6 => /host/libc.so.6 (0x3)",
            sdl: "libpulse.so.0 => /runtime/libpulse.so.0 (0x1)",
            audio: "libc.so.6 => /host/libc.so.6 (0x1)",
        }
        with patch.object(pack, "Path", side_effect=linux_paths.__getitem__), \
                patch.object(pack, "run", side_effect=lambda command, image, **kwargs: listings[image]):
            pack.bundle_linux(binary, self.libraries)
        self.assertEqual({p.name for p in self.libraries.iterdir()}, {sdl.name, audio.name})

    def test_linux_fixture_with_windows_host_paths(self):
        native_file = self.file
        with patch.object(self, "file", side_effect=lambda name: WindowsFixturePath(native_file(name))):
            self.test_linux_keeps_audio_closure_but_uses_host_glibc_and_gpu()

    def test_missing_linux_dependency_rejects_package(self):
        binary = self.file("client")
        with patch.object(pack, "run", return_value="libSDL2.so.0 => not found"):
            with self.assertRaisesRegex(RuntimeError, "Missing Linux runtime"):
                pack.bundle_linux(binary, self.libraries)

    def test_conflicting_library_names_reject_package(self):
        first = self.file("a/libaudio.so")
        second = self.file("b/libaudio.so")
        origins = {}
        pack.copy_library(first, self.libraries / first.name, origins)
        with self.assertRaisesRegex(RuntimeError, "Conflicting runtime library"):
            pack.copy_library(second, self.libraries / second.name, origins)

    def test_macos_inherited_rpath_resolves_transitive_dependency(self):
        image = self.file("origin/libaudio.dylib")
        dependency = self.file("runtime/libcodec.dylib")
        with patch.object(pack, "mac_rpaths", return_value=[]):
            resolved, _ = pack.mac_resolve("@rpath/libcodec.dylib", image, image,
                                          [str(dependency.parent)])
        self.assertEqual(resolved, dependency.resolve())

    def windows_fixture(self):
        binary = self.file("client.exe")
        sdl = self.file("toolchain/bin/SDL2.dll")
        codec = self.file("toolchain/bin/libcodec.dll")
        self.file("windows/System32/KERNEL32.dll")
        return binary, sdl, codec

    def test_windows_collects_transitive_dlls_case_insensitively(self):
        binary, sdl, codec = self.windows_fixture()
        listings = {binary: "DLL Name: sdl2.DLL\nDLL Name: KERNEL32.dll",
                    sdl: "DLL Name: libcodec.dll", codec: "DLL Name: api-ms-win-crt-runtime-l1-1-0.dll"}

        def output(command, *args, **kwargs):
            return str(self.root / "toolchain") if command == "cygpath" else listings[args[-1]]

        with patch.dict(os.environ, {"MINGW_PREFIX": "/ucrt64", "WINDIR": str(self.root / "windows")}), \
                patch.object(pack, "run", side_effect=output):
            pack.bundle_windows(binary, self.libraries)
        self.assertEqual({p.name.lower() for p in self.libraries.iterdir()}, {"sdl2.dll", "libcodec.dll"})

    def test_missing_windows_dll_rejects_package(self):
        binary, _, _ = self.windows_fixture()

        def output(command, *args, **kwargs):
            return str(self.root / "toolchain") if command == "cygpath" else "DLL Name: missing.dll"

        with patch.dict(os.environ, {"MINGW_PREFIX": "/ucrt64", "WINDIR": str(self.root / "windows")}), \
                patch.object(pack, "run", side_effect=output):
            with self.assertRaisesRegex(RuntimeError, "Unresolved Windows dependency"):
                pack.bundle_windows(binary, self.libraries)

    def test_split_archives_reconstruct_exact_bytes_with_complete_checksums(self):
        archive = self.file("eclipse-recoil-windows-x86_64.zip")
        original = bytes(range(256)) * 8
        archive.write_bytes(original)
        pack.publish_files(archive, "windows", "game", limit=1024, part_size=600)
        parts = sorted(self.root.glob("*.zip.???"))
        self.assertEqual(b"".join(p.read_bytes() for p in parts), original)
        self.assertTrue(all(p.stat().st_size <= 600 for p in parts))
        self.assertFalse(archive.exists())
        manifest = (self.root / "eclipse-recoil-windows-x86_64.files.sha256").read_text()
        lines = manifest.splitlines()
        self.assertEqual(len(lines), len(parts) + 2)  # parts, PowerShell, batch
        for line in lines:
            digest, filename = line.split("  ", 1)
            self.assertEqual(pack.sha256(self.root / filename), digest)
        script = (self.root / "eclipse-recoil-windows-x86_64-extract.ps1").read_text()
        self.assertIn(f"$part -le {len(parts)}", script)

    def test_small_archive_stays_a_single_download(self):
        archive = self.file("eclipse-recoil-linux-arm64.tar.gz")
        pack.publish_files(archive, "linux", "game", limit=1024, part_size=600)
        self.assertTrue(archive.exists())
        self.assertEqual(len((self.root / "eclipse-recoil-linux-arm64.files.sha256").read_text().splitlines()), 1)


if __name__ == "__main__":
    unittest.main()
