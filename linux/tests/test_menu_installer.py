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
    portable_icon = root / "runtime/share/icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png"
    portable_icon.parent.mkdir(parents=True)
    portable_icon.write_bytes(b"PORTABLEICON")
    assert run(root / "scripts/install-menu.py", home).returncode == 0
    assert icon.read_bytes() == b"PORTABLEICON"

print("PASS: menu install/update/remove, unmanaged preservation, portable icon")
