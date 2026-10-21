from pathlib import Path
import subprocess,json,hashlib,re
b=Path('/tmp/lufus-physical-validation');target=json.loads((b/'physical-write-plan.json').read_text())['target'];real=Path(target['by_id']).resolve(strict=True)
row=json.loads(subprocess.check_output(['lsblk','-J','-b','-d','-o','PATH,SIZE,TRAN,SERIAL',str(real)],text=True))['blockdevices'][0]
assert row['serial']==target['serial'] and row['size']==target['bytes']
part=str(real)+'1'
info=subprocess.run(['lsblk','-J','-b','-o','PATH,SIZE,TYPE,FSTYPE,LABEL,PARTTYPE,MOUNTPOINTS',str(real)],capture_output=True,text=True,check=True);(b/'written-lsblk.json').write_text(info.stdout);print(info.stdout)
source=json.loads((b/'mint-partitions.json').read_text())['partitiontable']['partitions']
# Kernel and udev metadata are readable without opening the root-owned disk.
actual=[]
for partrow in json.loads(info.stdout)['blockdevices']:
 if partrow['type']!='part':continue
 name=Path(partrow['path']).name
 actual.append({'node':partrow['path'],'start':int(Path('/sys/class/block',name,'start').read_text()),'size':int(Path('/sys/class/block',name,'size').read_text()),'type':format(int(partrow['parttype'],16),'x')})
assert [(v['start'],v['size'],int(v['type'],16)) for v in source]==[(v['start'],v['size'],int(v['type'],16)) for v in actual]
(b/'written-partitions.json').write_text(json.dumps(actual,indent=2)+'\n')
mount=subprocess.run(['udisksctl','mount','-b',part,'-o','ro'],capture_output=True,text=True);(b/'written-mount.txt').write_text(mount.stdout+mount.stderr);print(mount.stdout+mount.stderr)
r=subprocess.run(['findmnt','-J','--source',part],capture_output=True,text=True,check=True);data=json.loads(r.stdout);fs=data['filesystems'][0];assert fs['fstype']=='iso9660' and 'ro' in fs['options'].split(',');root=Path(fs['target'])
loaders=[]
for f in (root/'EFI').rglob('*'):
 if f.is_file() and f.suffix.lower()=='.efi':
  binary=f.read_bytes();assert binary[:2]==b'MZ';loaders.append({'path':str(f.relative_to(root)),'bytes':len(binary),'sha256':hashlib.sha256(binary).hexdigest()})
assert any(Path(f['path']).name.lower()=='bootx64.efi' for f in loaders)
bios=root/'isolinux/isolinux.bin';assert bios.is_file()
record={'serial':target['serial'],'mount':fs,'partition_layout_matches_source':True,'efi_files':loaders,'bios_loader_bytes':bios.stat().st_size,'physical_firmware_boot_performed':False}
(b/'written-boot-files.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
unmount=subprocess.run(['udisksctl','unmount','-b',part],capture_output=True,text=True);(b/'written-unmount.txt').write_text(unmount.stdout+unmount.stderr);print(unmount.stdout+unmount.stderr);assert unmount.returncode==0
journal=subprocess.run(['journalctl','--no-pager','-k','-b','--since','2026-10-20 11:39:30 UTC','--grep','sda|usb3|usb 3-3|FAT-fs','-n','100'],capture_output=True,text=True);(b/'kernel-after-test.txt').write_text(journal.stdout+journal.stderr);print(journal.stdout+journal.stderr)
