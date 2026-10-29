from pathlib import Path
import subprocess,os,gzip
from common import BASE as b, QEMU_ROOT, KERNEL, require_virtual_disk
with (b/'initramfs.cpio.gz').open('wb') as out:
 p1=subprocess.Popen(['find','.','-print0'],cwd=b/'guest',stdout=subprocess.PIPE)
 p2=subprocess.Popen(['cpio','--null','-o','--format=newc','--owner=0:0','--quiet'],cwd=b/'guest',stdin=p1.stdout,stdout=subprocess.PIPE)
 p1.stdout.close()
 p3=subprocess.run(['gzip','-1'],stdin=p2.stdout,stdout=out,check=True)
 p2.stdout.close(); assert p2.wait()==0 and p1.wait()==0
for name,size in [('usb-raw.img',64*1024**2),('usb-media.img',1024**3),('usb-togo.img',8*1024**3)]:
 p=b/name
 if not p.exists():
  with p.open('wb') as f:f.truncate(size)
for disk in ('usb-raw.img','usb-media.img','usb-togo.img'):require_virtual_disk(b/disk)
cmd=[str(QEMU_ROOT/'usr/bin/qemu-system-x86_64'),'-L',str(QEMU_ROOT/'usr/share/qemu'),'-bios',str(QEMU_ROOT/'usr/share/seabios/bios-256k.bin'),'-machine','q35','-accel','tcg,thread=multi','-cpu','max','-m','2048','-smp','2','-nodefaults','-nographic','-serial','stdio','-monitor','none','-no-reboot','-kernel',str(KERNEL),'-initrd',str(b/'initramfs.cpio.gz'),'-append','console=ttyS0 quiet panic=-1 rdinit=/init','-device','qemu-xhci,id=xhci','-drive',f'if=none,file={b}/usb-raw.img,format=raw,id=raw','-device','usb-storage,bus=xhci.0,drive=raw,removable=on','-drive',f'if=none,file={b}/usb-media.img,format=raw,id=media','-device','usb-storage,bus=xhci.0,drive=media,removable=on','-drive',f'if=none,file={b}/usb-togo.img,format=raw,id=togo','-device','usb-storage,bus=xhci.0,drive=togo,removable=on','-nic','none']
env=dict(os.environ,LD_LIBRARY_PATH=str(QEMU_ROOT/'usr/lib/x86_64-linux-gnu'))
print('Starting disposable VM, only three /tmp image files attached',flush=True)
with (b/'serial.log').open('wb') as log:
 r=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
print('QEMU exit',r.returncode)
data=(b/'serial.log').read_text(errors='replace')
print('\n'.join(line for line in data.splitlines() if line.startswith('QA ')))
import re,sys
summary=re.search(r'QA SUMMARY passed=(\d+) failed=(\d+)',data)
sys.exit(0 if r.returncode==0 and summary and int(summary[1])==48 and int(summary[2])==0 else 1)
