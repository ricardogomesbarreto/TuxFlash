from pathlib import Path
import subprocess,os,json,shutil,hashlib
p=Path('/tmp/lufus-integration/project');b=Path('/tmp/lufus-windows-validation');w=Path('/tmp/lufus-vm-release.5oFA8y')
assert (w/'.lufus-vm-validation').is_file()
helper=p/'linux/runtime/libexec/lufus-helper';digest=hashlib.sha256(helper.read_bytes()).hexdigest();shutil.copy2(helper,w/'guest/opt/lufus/libexec/lufus-helper')
shutil.copy2(p/'linux/runtime/libexec/bcd-sys/winload.sh',w/'guest/opt/lufus/libexec/bcd-sys/winload.sh')
shutil.copy2(p/'linux/tests/vm/guest-init.sh',w/'guest/init')
env=dict(os.environ,LUFUS_VM_WORK=str(w),LUFUS_VM_PROJECT=str(p),LUFUS_QEMU_ROOT='/tmp/lufus-vm-validation/qemu',LUFUS_VM_KERNEL='/tmp/lufus-vm-validation/vmlinuz',LUFUS_VM_KERNEL_VERSION='7.0.0-38-generic',LUFUS_VM_DEVROOT='/tmp/lufus-integration/sysroot')
with (b/'regression.log').open('w') as log:
 subprocess.run(['python3',str(p/'linux/tests/vm/run_guest.py')],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 subprocess.run(['python3',str(p/'linux/tests/vm/boot_check.py')],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
shutil.copy2(w/'serial.log',b/'regression-serial.log');shutil.copy2(w/'boot-results.json',b/'regression-boots.json')
s=(b/'regression-serial.log').read_text();assert digest in s and 'QA SUMMARY passed=34 failed=0' in s
assert all(x['passed'] for x in json.loads((b/'regression-boots.json').read_text()))
(b/'regression-summary.json').write_text(json.dumps({'helper_sha256':digest,'backend_cases_passed':34,'minimal_firmware_boots_passed':3},indent=2)+'\n');print('Current helper: 34 backend cases and 3 minimal boots passed')
