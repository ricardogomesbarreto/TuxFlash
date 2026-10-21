from pathlib import Path
import subprocess,os,shutil,json,time,stat,hashlib
b=Path('/tmp/lufus-windows-validation');base=Path('/caminho/dos/testes/.lufus-windows-validation-20261020');root=base/'guest';q=Path('/tmp/lufus-vm-validation/qemu');p=Path('/tmp/lufus-integration/project')
script=p/'linux/runtime/libexec/bcd-sys/winload.sh';shutil.copy2(script,root/'opt/lufus/libexec/bcd-sys/winload.sh');old=(root/'init').read_bytes()
try:
 shutil.copyfile(b/'guest-repair-bcd.sh',root/'init')
 with (base/'repair-initramfs.cpio.gz').open('wb') as f:
  p1=subprocess.Popen(['find','.','-print0'],cwd=root,stdout=subprocess.PIPE);p2=subprocess.Popen(['cpio','--null','-o','--format=newc','--owner=0:0','--quiet'],cwd=root,stdin=p1.stdout,stdout=subprocess.PIPE);p1.stdout.close();subprocess.run(['gzip','-1'],stdin=p2.stdout,stdout=f,check=True);p2.stdout.close();assert p2.wait()==0 and p1.wait()==0
finally:(root/'init').write_bytes(old)
disk=base/'windows-to-go.img';assert disk.resolve().parent==base and stat.S_ISREG(disk.lstat().st_mode) and disk.stat().st_size==34359738368
cmd=[str(q/'usr/bin/qemu-system-x86_64'),'-L',str(q/'usr/share/qemu'),'-bios',str(q/'usr/share/seabios/bios-256k.bin'),'-machine','q35','-accel','tcg,thread=multi','-cpu','max','-m','1024','-smp','1','-nodefaults','-nographic','-serial','stdio','-monitor','none','-no-reboot','-kernel','/tmp/lufus-vm-validation/vmlinuz','-initrd',str(base/'repair-initramfs.cpio.gz'),'-append','console=ttyS0 quiet panic=-1 rdinit=/init','-device','qemu-xhci,id=xhci','-drive',f'if=none,file={disk},format=raw,id=usb','-device','usb-storage,bus=xhci.0,drive=usb,removable=on','-nic','none']
start=time.monotonic()
with (b/'bcd-repair.log').open('wb') as f:r=subprocess.run(cmd,env=dict(os.environ,LD_LIBRARY_PATH=str(q/'usr/lib/x86_64-linux-gnu')),stdout=f,stderr=subprocess.STDOUT,timeout=180)
s=(b/'bcd-repair.log').read_text(errors='replace');passed=r.returncode==0 and 'BCD REPAIR PASS' in s
(b/'bcd-repair.json').write_text(json.dumps({'command':cmd,'exit_code':r.returncode,'passed':passed,'elapsed_seconds':time.monotonic()-start,'winload_script_sha256':hashlib.sha256(script.read_bytes()).hexdigest(),'rewrites_windows_image':False,'physical_devices_attached':False},indent=2)+'\n');print(s[-5000:]);assert passed
