from pathlib import Path
import os,subprocess,json,shutil,sys
from common import BASE as b, QEMU_ROOT, KERNEL, require_virtual_disk
results=[]
for kind,disk,marker in [('bios','usb-raw.img','LUFUS_BIOS_BOOT_OK'),('uefi','usb-media.img','LUFUS_UEFI_BOOT_OK'),('togo-uefi','usb-togo.img','LUFUS_UEFI_BOOT_OK')]:
 require_virtual_disk(b/disk)
 cmd=[str(QEMU_ROOT/'usr/bin/qemu-system-x86_64'),'-L',str(QEMU_ROOT/'usr/share/qemu'),'-machine','q35','-accel','tcg','-cpu','max','-m','256','-nodefaults','-display','none','-serial','stdio','-monitor','none','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=4','-device','qemu-xhci,id=xhci','-drive',f'if=none,file={b}/{disk},format=raw,id=usb','-device','usb-storage,bus=xhci.0,drive=usb,removable=on,bootindex=1','-nic','none']
 if kind=='bios':
  cmd+=['-bios',str(QEMU_ROOT/'usr/share/seabios/bios-256k.bin')]
 else:
  var=b/(kind+'-vars.fd');shutil.copyfile(QEMU_ROOT/'usr/share/OVMF/OVMF_VARS_4M.fd',var)
  cmd+=['-drive',f'if=pflash,format=raw,unit=0,readonly=on,file={QEMU_ROOT}/usr/share/OVMF/OVMF_CODE_4M.fd','-drive',f'if=pflash,format=raw,unit=1,file={var}']
 log=b/(kind+'-boot.log')
 env=dict(os.environ,LD_LIBRARY_PATH=str(QEMU_ROOT/'usr/lib/x86_64-linux-gnu'))
 timed_out=False
 with log.open('wb') as out:
  try:r=subprocess.run(cmd,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=90);code=r.returncode
  except subprocess.TimeoutExpired:code=None;timed_out=True
 data=log.read_text(errors='replace')
 passed=marker in data and code==33
 results.append(dict(kind=kind,command=cmd,expected_marker=marker,exit_code=code,timeout=timed_out,passed=passed,log=log.name))
 print(kind,'PASS' if passed else 'FAIL',flush=True)
 if not passed:print(data[-3000:],flush=True)
(b/'boot-results.json').write_text(json.dumps(results,indent=2)+'\n')
sys.exit(0 if all(r['passed'] for r in results) else 1)
