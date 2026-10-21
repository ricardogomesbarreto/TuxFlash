#!/usr/bin/env python3
"""Register only the current user's TuxFlash launcher and supplied icon."""
from pathlib import Path
import argparse,os,shutil,subprocess

app='org.tuxflash.TuxFlash'
p=argparse.ArgumentParser(description='Instalar TuxFlash no menu deste usuário')
p.add_argument('--remove',action='store_true')
args=p.parse_args()
root=Path(__file__).resolve().parents[1]
data=Path(os.environ.get('XDG_DATA_HOME',str(Path.home()/'.local/share')))
desktop=data/'applications'/f'{app}.desktop'
icon=data/'icons/hicolor/512x512/apps'/f'{app}.png'
if args.remove:
    if desktop.exists() and 'X-TuxFlash-Managed=true' in desktop.read_text():desktop.unlink()
    if icon.exists():icon.unlink()
    print('Atalho TuxFlash removido do menu do usuário.')
    raise SystemExit(0)
launcher=next((root/name for name in ['tuxflash.sh','tuxflash'] if (root/name).is_file()),None)
source=next((root/name for name in [f'linux/assets/icons/hicolor/512x512/apps/{app}.png',f'runtime/share/icons/hicolor/512x512/apps/{app}.png'] if (root/name).is_file()),None)
assert launcher is not None and source is not None,'Launcher ou ícone não encontrado'
# Desktop Entry Exec uses quoted arguments, not a shell command.
value=str(launcher).replace('%','%%')
for char in ['\\','"','`','$']:value=value.replace(char,'\\'+char)
entry='''[Desktop Entry]
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
'''.replace('@EXEC@',value)
desktop.parent.mkdir(parents=True,exist_ok=True);icon.parent.mkdir(parents=True,exist_ok=True)
shutil.copyfile(source,icon);desktop.write_text(entry);desktop.chmod(0o755)
update=shutil.which('update-desktop-database')
if update:subprocess.run([update,str(desktop.parent)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
print('TuxFlash instalado no menu do usuário com o ícone do pinguim.')
