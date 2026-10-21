from pathlib import Path
import subprocess,os,shutil,json,time
b=Path('/tmp/lufus-windows-validation');base=Path('/caminho/dos/testes/.lufus-windows-validation-20261020');root=base/'guest';q=Path('/tmp/lufus-vm-validation/qemu')
old=(root/'init').read_bytes()
try:
 shutil.copyfile(b/'guest-udf-visibility.sh',root/'init')
 with (base/'udf-initramfs.cpio.gz').open('wb') as f:
  p1=subprocess.Popen(['find','.','-print0'],cwd=root,stdout=subprocess.PIPE)
  p2=subprocess.Popen(['cpio','--null','-o','--format=newc','--owner=0:0','--quiet'],cwd=root,stdin=p1.stdout,stdout=subprocess.PIPE);p1.stdout.close()
  subprocess.run(['gzip','-1'],stdin=p2.stdout,stdout=f,check=True);p2.stdout.close();assert p2.wait()==0 and p1.wait()==0
finally:(root/'init').write_bytes(old)
cmd=[str(q/'usr/bin/qemu-system-x86_64'),'-L',str(q/'usr/share/qemu'),'-bios',str(q/'usr/share/seabios/bios-256k.bin'),'-machine','q35','-accel','tcg,thread=multi','-cpu','max','-m','1024','-smp','1','-nodefaults','-nographic','-serial','stdio','-monitor','none','-no-reboot','-kernel','/tmp/lufus-vm-validation/vmlinuz','-initrd',str(base/'udf-initramfs.cpio.gz'),'-append','console=ttyS0 quiet panic=-1 rdinit=/init','-virtfs',f'local,path={base}/source,mount_tag=windows-source,security_model=none,readonly=on','-nic','none']
start=time.monotonic()
with (b/'udf-visibility.log').open('wb') as log:r=subprocess.run(cmd,env=dict(os.environ,LD_LIBRARY_PATH=str(q/'usr/lib/x86_64-linux-gnu')),stdout=log,stderr=subprocess.STDOUT,timeout=120)
s=(b/'udf-visibility.log').read_text(errors='replace');passed=r.returncode==0 and 'UDF VISIBILITY PASS' in s
(b/'udf-visibility.json').write_text(json.dumps({'command':cmd,'exit_code':r.returncode,'passed':passed,'elapsed_seconds':time.monotonic()-start,'disks_attached':False},indent=2)+'\n')
print(s);assert passed
