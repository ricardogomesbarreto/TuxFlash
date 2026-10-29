from pathlib import Path
import os,re,shutil,subprocess
from common import BASE, PROJECT, KVER, DEVROOT
ROOT=BASE/'guest'
ROOT.mkdir(exist_ok=True)
for d in ('bin','sbin','usr/bin','usr/sbin','lib64','lib/x86_64-linux-gnu','proc','sys','dev','tmp','run','etc','home','mnt','work','opt'):(ROOT/d).mkdir(parents=True,exist_ok=True)
shutil.copy2('/usr/bin/busybox',ROOT/'bin/busybox')
for a in subprocess.check_output(['/usr/bin/busybox','--list'],text=True).splitlines():
 p=ROOT/'bin'/a
 if not p.exists():p.symlink_to('busybox')
shutil.copytree(PROJECT/'linux/runtime',ROOT/'opt/lufus',dirs_exist_ok=True)
env=dict(os.environ,LD_LIBRARY_PATH=f'{PROJECT}/linux/runtime/lib:{DEVROOT}/usr/lib/x86_64-linux-gnu')
seen=set()
def add_binary(src,dest=None):
 src=Path(src)
 dest=dest or str(src)
 dst=ROOT/str(dest).lstrip('/')
 dst.parent.mkdir(parents=True,exist_ok=True)
 if dst.is_symlink():dst.unlink()
 shutil.copy2(src,dst,follow_symlinks=True)
 for line in subprocess.run(['ldd',str(src)],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL).stdout.splitlines():
  m=re.search(r'(?:=> )?(/[^ ]+)',line)
  if not m:continue
  dep=Path(m[1])
  if str(dep) in seen:continue
  seen.add(str(dep))
  target='/lib64/'+dep.name if 'ld-linux' in dep.name else '/lib/x86_64-linux-gnu/'+dep.name
  add_binary(dep,target)
for name in ('bash','lsblk','findmnt','mount','umount','blockdev','sfdisk','wipefs','mkfs.vfat','mkfs.ntfs','mkfs.ext4','ntfs-3g','gzip','xz','bzip2','zstd','dd','uuidgen','cp','find','sort','sed','grep','sha256sum','blkid','fsck.vfat','e2fsck','md5sum'):
 p=shutil.which(name)
 if p:add_binary(p,'/usr/'+('sbin/' if name.startswith(('mkfs.','fsck.')) or name=='e2fsck' else 'bin/')+name)
for name in ('lufus-helper','bcd-sys/set-windows-san-policy','bcd-sys/lufus-reg-import'):
 p=PROJECT/'linux/runtime/libexec'/name
 if p.exists():add_binary(p,'/opt/lufus/libexec/'+name)
for name in ('qemu-img','wimlib-imagex','hivexsh','f3probe','fatattr','setfattr'):
 p=PROJECT/'linux/runtime/bin'/name
 if p.exists():add_binary(p,'/opt/lufus/bin/'+name)
# Dedicated fixtures are compiled by the normal native build, not shipped as tools.
build = Path(os.environ.get('TUXFLASH_BUILD_DIR', str(PROJECT/'build/native')))
add_binary(build/'test-helper-io', '/opt/lufus/bin/.tuxflash-io-test.bin')
# Execute this ELF directly: runtime/run-elf would change /proc/self/exe and
# resolve the lifecycle test's fake umount to the bundled production tool.
add_binary(build/'test-helper-lifecycle', '/test-only/libexec/helper-lifecycle')
# Kernel modules are copied together with dependencies and decompressed.
kver=KVER
for mod in ('usb-storage','isofs','udf','ntfs3','nls_cp437','nls_iso8859-1','fuse'):
 r=subprocess.run(['modprobe','--show-depends','-S',kver,mod],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
 for line in r.stdout.splitlines():
  if not line.startswith('insmod '):continue
  p=Path(line.split()[1]); dest=ROOT/str(p).lstrip('/');dest.parent.mkdir(parents=True,exist_ok=True)
  if p.suffix=='.zst':
   dest=dest.with_suffix('')
   with dest.open('wb') as out:subprocess.run(['zstd','-dc',str(p)],stdout=out,check=True)
  else:shutil.copy2(p,dest)
for name in ('modules.builtin','modules.builtin.modinfo','modules.order'):
 p=Path('/lib/modules')/kver/name
 if p.exists():shutil.copy2(p,ROOT/'lib/modules'/kver/name)
subprocess.run(['depmod','-b',str(ROOT),kver],check=True,stdout=subprocess.DEVNULL)
(ROOT/'etc/passwd').write_text('root:x:0:0:root:/root:/bin/sh\n')
(ROOT/'etc/group').write_text('root:x:0:\n')
(ROOT/'etc/mtab').symlink_to('/proc/mounts') if not (ROOT/'etc/mtab').exists() else None
(ROOT/'etc/filesystems').write_text('vfat\next4\nntfs3\niso9660\nudf\n')
# Tiny deterministic raw image, with a real BIOS entry point and serial marker.
payload=bytearray(8*1024*1024)
# BIOS real mode: set DS=0; emit string over COM1, signal isa-debug-exit, halt.
code=bytes.fromhex('31c08ed8be227cba f803 ac84c07403eeebf8 baf400b010eef4ebfd'.replace(' ',''))
# Correct SI after code length is known.
code=code[:5]+(0x7c00+len(code)).to_bytes(2,'little')+code[7:]
payload[:len(code)]=code
marker=b'LUFUS_BIOS_BOOT_OK\r\n\0';payload[len(code):len(code)+len(marker)]=marker
payload[510:512]=b'\x55\xaa'
for n in range(512,len(payload),4096):payload[n:n+32]=bytes((n//4096+i)%256 for i in range(32))
(ROOT/'work/source.img').write_bytes(payload)
for ext,command in [('gz',['gzip','-c']),('xz',['xz','-c']),('bz2',['bzip2','-c']),('zst',['zstd','-q','-c'])]:
 with (ROOT/f'work/source.img.{ext}').open('wb') as f:subprocess.run(command,input=payload,stdout=f,check=True)
(ROOT/'work/corrupt.img.gz').write_bytes(b'not gzip\n')
(ROOT/'work/truncated.img.gz').write_bytes((ROOT/'work/source.img.gz').read_bytes()[:-9])
# Match the installed ntfs-3g mount integration, not the legacy read-only driver.
for d in ('usr/sbin','sbin'):
 p=ROOT/d/'mount.ntfs'
 if not p.exists():p.symlink_to('/usr/bin/ntfs-3g')
shutil.copytree('/usr/lib/x86_64-linux-gnu/gconv',ROOT/'usr/lib/x86_64-linux-gnu/gconv',dirs_exist_ok=True)
print('Guest files prepared')
