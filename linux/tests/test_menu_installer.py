# SPDX-License-Identifier: GPL-3.0-or-later
"""Menu installation tests use an isolated XDG_DATA_HOME and no system writes."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

source_root = Path(__file__).resolve().parents[2]
script = source_root / "scripts/install-menu.py"


def run(script_path, data_dir, *args):
    env = dict(os.environ, XDG_DATA_HOME=str(data_dir))
    return subprocess.run(
        [sys.executable, str(script_path), *args],
        env=env, text=True, capture_output=True, check=False,
    )


with tempfile.TemporaryDirectory(prefix="tuxflash-menu-") as temp:
    root = Path(temp) / "app with spaces"
    (root / "scripts").mkdir(parents=True)
    shutil.copy2(script, root / "scripts/install-menu.py")
    launcher = root / "tuxflash"
    launcher.write_text("#!/bin/sh\nexit 0\n")
    launcher.chmod(0o755)
    asset = root / "linux/assets/icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png"
    asset.parent.mkdir(parents=True)
    asset.write_bytes(b"TESTICON")

    home = Path(temp) / "XDG data"
    desktop = home / "applications/org.tuxflash.TuxFlash.desktop"
    icon = home / "icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png"
    # A source checkout with only a top-level launcher must not install a
    # broken shortcut or leave any XDG files behind.
    result = run(root / "scripts/install-menu.py", home)
    assert result.returncode != 0 and "make" in result.stderr
    assert not desktop.exists() and not icon.exists()

    local = root / "linux/runtime/bin/tuxflash"
    helper = root / "linux/runtime/libexec/tuxflash-helper"
    for binary in (local, helper):
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_text("#!/bin/sh\nexit 0\n")
        binary.chmod(0o755)

    result = run(root / "scripts/install-menu.py", home)
    assert result.returncode == 0, result.stderr
    assert desktop.is_file() and icon.read_bytes() == b"TESTICON"
    assert f'Exec="{launcher}"' in desktop.read_text()
    assert "X-TuxFlash-Managed=true" in desktop.read_text()
    assert desktop.stat().st_mode & 0o777 == 0o644

    # Idempotent update, followed by a safe removal of managed assets.
    assert run(root / "scripts/install-menu.py", home).returncode == 0
    assert run(root / "scripts/install-menu.py", home, "--remove").returncode == 0
    assert not desktop.exists() and not icon.exists()

    # A partially built portable bundle takes precedence in scripts/run.sh,
    # and must not create a broken shortcut even if the local build exists.
    nested = root / "dist/portable/tuxflash"
    nested.parent.mkdir(parents=True)
    nested.write_text("#!/bin/sh\nexit 127\n")
    nested.chmod(0o755)
    failed = run(root / "scripts/install-menu.py", home)
    assert failed.returncode != 0 and "dist/portable" in failed.stderr
    assert not desktop.exists() and not icon.exists()
    nested.unlink()

    # Never replace/remove an entry owned by another program.
    desktop.write_text("[Desktop Entry]\nName=Someone else\n")
    icon.write_bytes(b"OTHERICON")
    assert run(root / "scripts/install-menu.py", home).returncode != 0
    assert desktop.read_text() == "[Desktop Entry]\nName=Someone else\n"
    assert run(root / "scripts/install-menu.py", home, "--remove").returncode == 0
    assert icon.read_bytes() == b"OTHERICON"

    # Portable archives use the icon inside runtime/share, not linux/assets.
    desktop.unlink()
    asset.unlink()
    # An unmanaged orphan icon must not be overwritten either.
    assert run(root / "scripts/install-menu.py", home).returncode != 0
    assert icon.read_bytes() == b"OTHERICON"
    icon.unlink()
    portable_icon = root / "runtime/share/icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png"
    portable_icon.parent.mkdir(parents=True)
    portable_icon.write_bytes(b"PORTABLEICON")
    # A partially extracted portable archive is rejected before writing
    # either the launcher or icon. Then verify a complete portable layout.
    local.unlink()
    helper.unlink()
    failed = run(root / "scripts/install-menu.py", home)
    assert failed.returncode != 0 and "Pacote portátil incompleto" in failed.stderr
    assert not desktop.exists() and not icon.exists()
    portable = root / "runtime"
    (portable / ".tuxflash-portable").write_text("TuxFlash private-loader runtime v1\n")
    for binary in (portable / "bin/tuxflash", portable / "run-elf"):
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_text("#!/bin/sh\nexit 0\n")
        binary.chmod(0o755)

    assert run(root / "scripts/install-menu.py", home).returncode == 0
    assert icon.read_bytes() == b"PORTABLEICON"

print("PASS: menu runtime preflight, missing builds, incomplete portable, unmanaged preservation")
