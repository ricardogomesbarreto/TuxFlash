#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Integration checks using Python's independent hashes and byte comparisons.

Only temporary regular files are written. No device-formatting tests run here.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()


class LinuxProbeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="rufus-linux-probe-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_probe(self, *args, success=True):
        result = subprocess.run(
            [str(PROBE), *map(str, args)], capture_output=True, text=True, timeout=15
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
            return json.loads(result.stdout)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        return result

    def test_hashes_match_independent_implementation(self):
        for data in (b"", b"abc", bytes(range(256)) * 8193 + b"tail"):
            with self.subTest(length=len(data)):
                source = self.root / "source.img"
                source.write_bytes(data)
                hashes = self.run_probe("hash", source)
                expected = {
                    name: hashlib.new(name, data).hexdigest()
                    for name in ("md5", "sha1", "sha256", "sha512")
                }
                self.assertEqual(hashes, expected)

    def test_copy_is_exact_including_partial_last_chunk(self):
        for length in (0, 1, 511, 512, 1048576 + 31):
            with self.subTest(length=length):
                data = (bytes(range(256)) * ((length + 255) // 256))[:length]
                source = self.root / "source.img"
                destination = self.root / f"copy-{length}.img"
                source.write_bytes(data)
                report = self.run_probe("copy-image", source, destination)
                self.assertEqual(report, {
                    "copied": True, "byte_comparison": True, "fsync": True
                })
                self.assertEqual(destination.read_bytes(), data)
                self.assertEqual(source.read_bytes(), data)
                self.assertEqual(destination.stat().st_mode & 0o777, 0o600)

    def test_copy_refuses_existing_destination(self):
        source = self.root / "source.img"
        destination = self.root / "existing.img"
        source.write_bytes(b"source")
        destination.write_bytes(b"keep this content")
        self.run_probe("copy-image", source, destination, success=False)
        self.assertEqual(destination.read_bytes(), b"keep this content")
        self.run_probe("copy-image", source, source, success=False)
        self.assertEqual(source.read_bytes(), b"source")

    def test_destination_symlinks_are_not_followed(self):
        source = self.root / "source.img"
        target = self.root / "target.img"
        link = self.root / "link.img"
        source.write_bytes(b"source")
        target.write_bytes(b"target")
        link.symlink_to(target)
        self.run_probe("copy-image", source, link, success=False)
        self.assertEqual(target.read_bytes(), b"target")
        link.unlink()
        link.symlink_to(self.root / "absent.img")
        self.run_probe("copy-image", source, link, success=False)
        self.assertFalse((self.root / "absent.img").exists())

    def test_nonregular_sources_and_device_destinations_are_refused(self):
        fifo = self.root / "fifo"
        os.mkfifo(fifo)
        for source in (self.root, fifo, Path("/dev/null")):
            with self.subTest(source=source):
                self.run_probe("hash", source, success=False)
                self.run_probe("inspect", source, success=False)
                self.run_probe("copy-image", source, self.root / "output", success=False)
                self.assertFalse((self.root / "output").exists())
        source = self.root / "source.img"
        source.write_bytes(b"source")
        self.run_probe("copy-image", source, "/dev/null", success=False)

    def test_probe_reports_only_observed_signatures(self):
        source = self.root / "sample.img"
        for length in (0, 1, 511):
            source.write_bytes(bytes(length))
            report = self.run_probe("inspect", source)
            self.assertEqual(report["size_bytes"], length)
            self.assertTrue(all(value is False for key, value in report.items()
                                if key != "size_bytes"))
        data = bytearray(40000)
        data[510:512] = b"\x55\xaa"
        data[512:520] = b"EFI PART"
        data[4096:4104] = b"EFI PART"
        data[32768:32775] = b"\x01CD001\x01"
        source.write_bytes(data)
        report = self.run_probe("inspect", source)
        self.assertEqual(report["size_bytes"], 40000)
        self.assertTrue(all(value is True for key, value in report.items()
                            if key != "size_bytes"))
        self.assertNotIn("bootable", report)

    def test_inventory_is_explicitly_read_only(self):
        report = self.run_probe("list")
        self.assertEqual(report["mode"], "read_only_inventory")
        self.assertIsInstance(report["devices"], list)
        for device in report["devices"]:
            self.assertTrue(device["device"].startswith("/dev/"))
            self.assertIsInstance(device["size_bytes"], int)


if __name__ == "__main__":
    unittest.main(verbosity=2)
