# SPDX-License-Identifier: GPL-3.0-or-later
"""Reject unsafe QA inputs before invoking fakeroot, mkfs, QEMU or Polkit."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

runner = Path(__file__).parent / "vm/polkit/run.py"
with tempfile.TemporaryDirectory(prefix="tuxflash-polkit-guard-", dir="/tmp") as name:
    root = Path(name)
    base = [sys.executable, str(runner)]
    for key in ("project", "runtime", "binary", "kernel", "modules", "rootfs-tar", "sums", "signature"):
        base += ["--" + key, "/dev/null"]
    env = dict(os.environ)
    env.pop("FAKEROOTKEY", None)

    def reject(work, message, extra=()):
        result = subprocess.run(base + ["--work", str(work), *extra], env=env,
                                capture_output=True, text=True, timeout=5)
        assert result.returncode != 0, result.stdout
        assert message in result.stderr, result.stderr
        assert not (work / "root.img").exists()
        assert not (work / "usb.img").exists()

    reject(Path("/dev/null"), "below /tmp")
    if os.geteuid() == 0:
        reject(root / "root-caller", "without sudo")
        print("PASS: root caller and outside-/tmp input refused")
        sys.exit(0)
    busy = root / "nonempty"
    busy.mkdir()
    sentinel = busy / "existing-data"
    sentinel.write_bytes(b"PRESERVE")
    reject(busy, "must be empty")
    assert sentinel.read_bytes() == b"PRESERVE"
    empty = root / "nonregular"
    reject(empty, "regular files")
    markerless = root / "markerless"
    markerless.mkdir()
    reject(markerless, "Missing private work marker", ["--prepare-child"])
    fake = root / "forged-child"
    fake.mkdir()
    (fake / ".tuxflash-disposable-polkit-qa").write_text("marker")
    reject(fake, "inside the caller-created fakeroot", ["--prepare-child"])
print("PASS: outside-/tmp, existing data, device inputs and forged preparation refused")
