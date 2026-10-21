from pathlib import Path
import os,shutil,subprocess
from common import BASE as b, PROJECT
root=b/'guest/work'; trees=b/'fixtures'; trees.mkdir(exist_ok=True)
env=dict(os.environ,LD_LIBRARY_PATH=str(PROJECT/'linux/runtime/lib'))
wim=str(PROJECT/'linux/runtime/bin/wimlib-imagex')
def put(tree,p,data):
 f=tree/p;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(data)
def iso(tree,out):subprocess.run(['xorriso','-as','mkisofs','-quiet','-J','-r','-o',str(root/out),str(tree)],check=True)
for flavor in ('casper','debian'):
 tree=trees/flavor
 put(tree,('casper' if flavor=='casper' else 'live')+'/fixture.txt','Synthetic fixture; no Linux kernel included.\n')
 put(tree,'boot/grub/grub.cfg','menuentry fixture {\n linux /casper/vmlinuz boot=casper quiet ---\n}\n' if flavor=='casper' else 'menuentry fixture {\n linux /live/vmlinuz boot=live quiet\n}\n')
 (tree/'EFI/BOOT').mkdir(parents=True,exist_ok=True);shutil.copy2(b/'BOOTX64.EFI',tree/'EFI/BOOT/BOOTX64.EFI')
 iso(tree,flavor+'.iso')
tree=trees/'windows'
put(tree,'fixture.txt','Synthetic Windows-shaped test media, NOT Windows.\n')
put(tree,'sources/appraiserres.dll','fixture original appraiser\n')
(tree/'efi/boot').mkdir(parents=True,exist_ok=True);shutil.copy2(b/'BOOTX64.EFI',tree/'efi/boot/bootx64.efi')
wimroot=trees/'wimroot';put(wimroot,'Windows/System32/fixture.txt','Synthetic WIM, no operating system.\n')
boot=tree/'sources/boot.wim'
if not boot.exists():
 subprocess.run([wim,'capture',str(wimroot),str(boot),'Fixture PE','--compress=none'],check=True,env=env,stdout=subprocess.DEVNULL)
 subprocess.run([wim,'append',str(wimroot),str(boot),'Fixture Setup'],check=True,env=env,stdout=subprocess.DEVNULL)
# A minimal Windows-shaped WIM exercises apply/BCD/SAN logic without Windows.
for p in ('Windows/Boot/EFI/bootmgfw.efi','Windows/System32/winload.efi'):
 f=wimroot/p; f.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(b/'BOOTX64.EFI',f)
put(wimroot,'Windows/Boot/Fonts/fixture.txt','Fixture font directory.\n')
base=PROJECT/'linux/runtime/libexec/bcd-sys/Templates/BCD-NEW'
importer=str(PROJECT/'build/native/lufus-reg-import')
regs={
'SOFTWARE': '[BCD00000001\\Microsoft\\Windows NT\\CurrentVersion]\n"ProductName"="Windows 11 Fixture"\n',
'SYSTEM': '[BCD00000001\\Select]\n"Current"=dword:00000001\n\n[BCD00000001\\ControlSet001]\n'}
for name,reg in regs.items():
 f=trees/(name+'.reg'); f.write_text('Windows Registry Editor Version 5.00\n\n'+reg)
 out=wimroot/'Windows/System32/config'/name;out.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run([importer,str(base),str(f),str(out)],check=True,env=env)
install=tree/'sources/install.wim'
if install.exists():install.unlink()
subprocess.run([wim,'capture',str(wimroot),str(install),'Fixture Windows','--compress=none'],check=True,env=env,stdout=subprocess.DEVNULL)
iso(tree,'windows.iso')
print('Synthetic fixtures created')

shutil.copy2(b/'BOOTX64.EFI',root/'efi-fixture.efi')
shutil.copy2(tree/'fixture.txt',root/'windows-marker.txt')
shutil.copy2(wimroot/'Windows/System32/config/SYSTEM',root/'SYSTEM-fixture')
