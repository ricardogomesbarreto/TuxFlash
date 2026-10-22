#!/usr/bin/env python3
"""Register or remove only the current user's TuxFlash menu shortcut.

Never overwrite a desktop entry not managed by this installer. The installed
shortcut points to the extracted bundle/source directory; keep that directory.
"""
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import os
import shutil
import subprocess
from pathlib import Path

APP_ID = "org.tuxflash.TuxFlash"
MANAGED_MARKER = "X-TuxFlash-Managed=true"


def main():
    parser = argparse.ArgumentParser(
        description="Instalar ou remover o TuxFlash do menu do usuário"
    )
    parser.add_argument("--remove", action="store_true", help="Remove o atalho gerenciado")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    data_home = Path(
        os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))
    ).expanduser()
    desktop = data_home / "applications" / f"{APP_ID}.desktop"
    icon = data_home / "icons/hicolor/512x512/apps" / f"{APP_ID}.png"

    # Do not follow a symlink that could redirect an overwrite to another file.
    if desktop.is_symlink() or icon.is_symlink():
        parser.error("Entrada desktop ou ícone é um link simbólico. Operação recusada.")

    managed = (
        desktop.is_file()
        and MANAGED_MARKER in desktop.read_text(encoding="utf-8", errors="replace")
    )
    if args.remove:
        if managed:
            desktop.unlink()
            if icon.is_file():
                icon.unlink()
            print("Atalho TuxFlash removido do menu do usuário.")
        else:
            print("Nenhum atalho TuxFlash gerenciado encontrado; nada foi removido.")
        return

    if desktop.exists() and not managed:
        parser.error("Já existe um atalho não gerenciado nesse local; ele foi preservado.")
    if icon.exists() and not managed:
        parser.error("Já existe um ícone não gerenciado nesse local; ele foi preservado.")

    launcher = next(
        (
            p
            for p in (root / "tuxflash", root / "tuxflash.sh")
            if p.is_file() and os.access(p, os.X_OK)
        ),
        None,
    )
    source = next(
        (
            p
            for p in (
                root / f"linux/assets/icons/hicolor/512x512/apps/{APP_ID}.png",
                root / f"runtime/share/icons/hicolor/512x512/apps/{APP_ID}.png",
            )
            if p.is_file()
        ),
        None,
    )
    if launcher is None:
        parser.error("Lançador executável não encontrado na pasta do TuxFlash.")
    if source is None:
        parser.error("Ícone original do TuxFlash não encontrado.")

    # Desktop Entry Exec values must be double-quoted and escape reserved chars.
    value = str(launcher).replace("%", "%%")
    for char in ('\\', '"', '`', '$'):
        value = value.replace(char, "\\" + char)

    entry = """[Desktop Entry]
Type=Application
Version=1.0
Name=TuxFlash
GenericName=Bootable USB Creator for Linux
GenericName[pt_BR]=Criador de pendrives inicializáveis para Linux
GenericName[es]=Creador de unidades USB de arranque para Linux
GenericName[zh_CN]=Linux 可启动 USB 制作工具
Comment[en]=Turn ISO images into bootable USB drives simply and safely.
Comment[es]=Transforme imágenes ISO en unidades USB de arranque de forma sencilla y segura.
Comment[zh_CN]=简单、安全地将 ISO 镜像制作成可启动 USB 设备。
Comment=Transforme imagens ISO em pendrives inicializáveis com simplicidade e segurança.
Exec="@EXEC@"
Icon=org.tuxflash.TuxFlash
Terminal=false
Categories=Utility;System;
Keywords=USB;ISO;Linux;pendrive;
StartupNotify=true
StartupWMClass=org.tuxflash.TuxFlash
X-TuxFlash-Managed=true
""".replace("@EXEC@", value)

    desktop.parent.mkdir(parents=True, exist_ok=True)
    icon.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, icon)
    desktop.write_text(entry, encoding="utf-8")
    desktop.chmod(0o644)

    update = shutil.which("update-desktop-database")
    if update:
        subprocess.run(
            [update, str(desktop.parent)],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    print("TuxFlash instalado no menu deste usuário com o ícone oficial.")


if __name__ == "__main__":
    main()
