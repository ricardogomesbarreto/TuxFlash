from pathlib import Path
import sys,os,json,socket,subprocess,time,stat,shutil
out=Path('/tmp/lufus-windows-validation');base=Path('/caminho/dos/testes/.lufus-windows-validation-20261020');q=Path('/tmp/lufus-vm-validation/qemu')
mode=sys.argv[1];assert mode in ['installer','togo']
disk=base/('windows-installer-fixed.img' if mode=='installer' else 'windows-to-go.img');assert disk.resolve().parent==base and stat.S_ISREG(disk.lstat().st_mode)
assert (base/'.disposable-lufus-windows-validation').exists()
if mode=='installer':assert json.loads((out/'fixed-installer.json').read_text())['status']=='passed'
else:assert 'WINDOWS PASS real-windows-to-go-image-bcd-san-policy' in (out/'vm-serial.log').read_text(errors='replace')
sock=out/f'boot-{mode}.sock';assert not sock.exists()
vars=base/f'boot-{mode}-vars.fd';shutil.copyfile(q/'usr/share/OVMF/OVMF_VARS_4M.fd',vars)
cmd=[str(q/'usr/bin/qemu-system-x86_64'),'-L',str(q/'usr/share/qemu'),'-machine','q35','-accel','tcg,thread=multi','-cpu','max','-m','2048','-smp','2','-nodefaults','-drive',f'if=pflash,format=raw,readonly=on,file={q}/usr/share/OVMF/OVMF_CODE_4M.fd','-drive',f'if=pflash,format=raw,file={vars},snapshot=on','-device',f'VGA,romfile={q}/usr/share/seabios/vgabios-stdvga.bin','-display','none','-serial',f'file:{out}/boot-{mode}-serial.log','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-device','qemu-xhci,id=xhci','-drive',f'if=none,file={disk},format=raw,snapshot=on,id=usb','-device','usb-storage,bus=xhci.0,drive=usb,removable=on,bootindex=1','-nic','none']
start=time.monotonic();record={'mode':mode,'command':cmd,'physical_disks_attached':False,'network_enabled':False,'secure_boot_enabled':False,'snapshot_writes_only':True,'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'screenshots':[],'status':'running'}
def save():(out/f'boot-{mode}.json').write_text(json.dumps(record,indent=2)+'\n')
with (out/f'boot-{mode}-qemu.log').open('wb') as log:
 p=subprocess.Popen(cmd,env=dict(os.environ,LD_LIBRARY_PATH=str(q/'usr/lib/x86_64-linux-gnu')),stdout=log,stderr=subprocess.STDOUT);record['pid']=p.pid;save()
 try:
  for _ in range(200):
   if sock.exists():break
   if p.poll() is not None:raise RuntimeError('QEMU exited during startup')
   time.sleep(.1)
  conn=socket.socket(socket.AF_UNIX)
  for _ in range(200):
   try:conn.connect(str(sock));break
   except (ConnectionRefusedError,FileNotFoundError):
    if p.poll() is not None:raise RuntimeError('QEMU exited before QMP was ready')
    time.sleep(.1)
  conn.settimeout(10);f=conn.makefile('rwb',buffering=0);json.loads(f.readline())
  def command(name,args=None):
   req={'execute':name}
   if args:req['arguments']=args
   f.write((json.dumps(req)+'\n').encode())
   while True:
    r=json.loads(f.readline())
    if 'error' in r:raise RuntimeError(r)
    if 'return' in r:return r['return']
  command('qmp_capabilities');print(f'Isolated {mode} firmware boot started, PID {p.pid}',flush=True)
  times=[30,60,120,240,360,600,900];n=0
  while p.poll() is None and time.monotonic()-start<930:
   elapsed=time.monotonic()-start
   if n<len(times) and elapsed>=times[n]:
    name=f'boot-{mode}-{times[n]:03d}.ppm';command('screendump',{'filename':str(base/name)})
    from PIL import Image
    png=out/(name[:-4]+'.png');Image.open(base/name).save(png);(base/name).unlink()
    record['screenshots'].append({'elapsed_seconds':elapsed,'path':str(png)});save();print('Screenshot',png,flush=True);n+=1
   if (out/f'boot-{mode}.stop').exists():record['stopped_after_observation']=True;break
   time.sleep(1)
  if p.poll() is None:command('quit')
  p.wait(timeout=20)
 finally:
  if p.poll() is None:p.terminate();p.wait(timeout=10)
  record.update(exit_code=p.returncode,elapsed_seconds=time.monotonic()-start,status='completed; screenshots require visual assessment');save()
  if sock.exists():sock.unlink()
print(json.dumps(record,indent=2))
