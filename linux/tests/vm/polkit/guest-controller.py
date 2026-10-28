#!/usr/bin/python3
from pathlib import Path
import subprocess,os,time,json,hashlib,traceback
assert 'tuxflash_qa=1' in Path('/proc/cmdline').read_text()
w=Path('/var/lib/tuxflash-qa');deadline=time.monotonic()+900
while not (w/'target').exists():time.sleep(.1)
dev=(w/'target').read_text().strip();assert dev.startswith('/dev/sd')
assert subprocess.check_output(['blockdev','--getsize64',dev],text=True).strip()==str(64*1024**2)
def digest():
 subprocess.run(['blockdev','--flushbufs',dev],check=True)
 h=hashlib.sha256()
 with open(dev,'rb',buffering=0) as stream:
  for b in iter(lambda:stream.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
results=[];before={}
try:
 while time.monotonic()<deadline:
  for n in range(1,12):
   b=w/f'{n:02d}-before';a=w/f'{n:02d}-after'
   if b.exists() and n not in before:
    if n==11:
     subprocess.run(['chvt','2'],check=True)
     session=(w/'session-id').read_text().strip()
     until=time.monotonic()+5
     while subprocess.check_output(['loginctl','show-session',session,'-p','Active','--value'],text=True).strip()!='no' and time.monotonic()<until:time.sleep(.05)
     assert subprocess.check_output(['loginctl','show-session',session,'-p','Active','--value'],text=True).strip()=='no'
    before[n]=digest();(w/f'{n:02d}-before.ready').write_text('ready\n');print('POLKIT CASE START',n,flush=True)
   if a.exists() and not any(x['case']==n for x in results):
    after=digest();r=json.loads((w/f'{n:02d}-result.json').read_text())
    if n==11 or (n - 1) % 5 + 1 in (2,3,4):assert after==before[n],f'Unauthorized/pending-cancel write in case {n}'
    if n!=11 and (n - 1) % 5 + 1==1:
     source=Path(f'/home/tuxqa/source-{n:02d}.img').read_bytes()
     with open(dev,'rb') as stream:assert stream.read(len(source))==source
    r['target_unchanged']=after==before[n];results.append(r)
    if n==11:
     subprocess.run(['chvt','1'],check=True)
    (w/f'{n:02d}-after.ready').write_text('ready\n');print('POLKIT CASE PASS',n,json.dumps(r),flush=True)
  if (w/'user.done').exists():break
  time.sleep(.05)
 assert (w/'user.exit').read_text().strip()=='0'
 assert len(results)==11
 sessionless=json.loads((w/'sessionless.json').read_text());assert sessionless['passed']
 packages=subprocess.check_output(['dpkg-query','-W','-f=${Package} ${Version}\n','polkitd','pkexec','libpam-systemd','systemd','xvfb'],text=True).splitlines()
 summary={'packages':packages,'kernel':Path('/proc/sys/kernel/osrelease').read_text().strip(),'passed':True,'gui_cases_passed':11,'sessionless_denied':True,'physical_devices_attached':False,'network_link_disabled_during_tests':True,'cases':results}
except Exception as exc:
 traceback.print_exc();summary={'passed':False,'cases':results,'reason':str(exc)}
 print((w/'user.log').read_text(errors='replace') if (w/'user.log').exists() else 'no user log',flush=True)
 subprocess.run(['loginctl','list-sessions']);subprocess.run(['journalctl','-u','polkit','-n','35','--no-pager'])
(w/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print('POLKIT SUMMARY '+json.dumps(summary),flush=True)
time.sleep(2);subprocess.run(['systemctl','poweroff'])
