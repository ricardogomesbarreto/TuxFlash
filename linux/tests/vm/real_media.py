#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Qualify real images using a disposable USB file and the unmodified helper.

The guest template must contain a Linux userspace plus USB storage, virtio block and ext4 support
matching --kernel. No physical target path is accepted.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import subprocess
import time


def regular(path):
    path = Path(path).expanduser().absolute()
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError('Input must be a regular file, not a symlink or device')
    return path.resolve()


def digest(path, size=None):
    result = hashlib.sha256()
    left = size
    with path.open('rb') as stream:
        while left is None or left:
            block = stream.read(8 * 1024**2 if left is None else min(left, 8 * 1024**2))
            if not block:
                break
            result.update(block)
            if left is not None:
                left -= len(block)
    if left not in (None, 0):
        raise ValueError('Short read during independent comparison')
    return result.hexdigest()


def prepare_work(path):
    path = Path(path).expanduser().absolute()
    if any(c in str(path) for c in ',\n\r') or path.is_symlink():
        raise ValueError('Unsafe QEMU work path')
    path.mkdir(parents=True, exist_ok=True)
    path = path.resolve()
    if any(path.iterdir()):
        raise ValueError('Work directory must be empty; existing jobs are preserved')
    (path / '.tuxflash-real-media-qa').write_text('Disposable regular-file VM test only\n')
    return path


def init_script(image, capacity, mode, index, unattended=False, uefi_ntfs=False):
    options = 'write --verify'
    if mode == 'windows-installer':
        # Silent Disk 0 installation is an explicit, VM-only QA opt-in.
        flags = 94 if unattended else 30
        filesystem = '--uefi-ntfs ' if uefi_ntfs else ''
        options = (f'windows-installer --target-system uefi --windows-flags {flags} '
                   f'{filesystem}--architecture amd64 --windows-index {index} '
                   '--local-account TuxFlashQA --runtime-validation')
    return r'''#!/bin/sh
export PATH=/opt/tuxflash/bin:/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/opt/tuxflash/lib
export LC_ALL=C
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs tmpfs /tmp
mkdir -p /dev/pts /run /inputs
mount -t devpts devpts /dev/pts
modprobe usb-storage
# Minimal QA guests do not run a udev module loader.
modprobe isofs 2>/dev/null || true
modprobe udf 2>/dev/null || true
modprobe ntfs3 2>/dev/null || true
modprobe nls_cp437 2>/dev/null || true
modprobe nls_iso8859-1 2>/dev/null || true
modprobe fuse 2>/dev/null || true
modprobe virtio_blk 2>/dev/null || true
modprobe ext4 2>/dev/null || true
i=0
while [ ! -b /dev/vda ] && [ "$i" -lt 30 ]; do sleep 1; i=$((i+1)); done
if ! mount -t ext4 -o ro,noload /dev/vda /inputs; then
 echo 'MEDIA FATAL source-disk';poweroff -f
fi
i=0
while [ ! -b /dev/sda ] && [ "$i" -lt 30 ]; do sleep 1; i=$((i+1)); done
d=/dev/sda
[ -b "$d" ] && [ "$(cat /sys/class/block/sda/removable)" = 1 ] &&
[ "$(blockdev --getsize64 "$d")" = @CAPACITY@ ] || { echo 'MEDIA FATAL unsafe-target';poweroff -f; }
ident=$(cat /sys/class/block/sda/dev);seq=$(cat /sys/class/block/sda/diskseq)
echo 'MEDIA START'
sha256sum /opt/tuxflash/libexec/.tuxflash-helper.bin
/opt/tuxflash/libexec/tuxflash-helper @OPTIONS@ --source @IMAGE@ --target "$d" --major-minor "$ident" --size @CAPACITY@ --disk-seq "$seq" > /tmp/job.log 2>&1 &
job=$!
while kill -0 "$job" 2>/dev/null; do
 tail -n 1 /tmp/job.log | sed 's/^/MEDIA PROGRESS /'
 sleep 5
done
wait "$job";status=$?
echo 'MEDIA LOG BEGIN';cat /tmp/job.log;echo 'MEDIA LOG END'
echo "MEDIA RESULT status=$status"
sync
poweroff -f
'''.replace('@CAPACITY@', str(capacity)).replace('@OPTIONS@', options).replace('@IMAGE@', shlex.quote('/inputs/' + image))


def completion(text, exit_code):
    # The helper uses a byte count for RAW and a message for extracted media.
    # Only records inside its completed log are evidence; polling is not.
    sections = re.findall(r'^MEDIA LOG BEGIN\r?\n(.*?)^MEDIA LOG END\r?$', text, re.M | re.S)
    statuses = re.findall(r'^MEDIA RESULT status=(\d+)\r?$', text, re.M)
    records = re.findall(r'^COMPLETE ([^\r\n]+)\r?$', sections[0], re.M) if len(sections) == 1 else []
    passed = exit_code == 0 and statuses == ['0'] and len(records) == 1 and 'MEDIA FATAL' not in text
    return (records[0] if len(records) == 1 else ''), passed


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work', required=True, type=Path)
    p.add_argument('--image', required=True, type=Path)
    p.add_argument('--expanded-reference', type=Path)
    p.add_argument('--expected-sha256', help='Expected digest of the supplied image file')
    p.add_argument('--guest-template', required=True, type=Path)
    p.add_argument('--runtime', required=True, type=Path)
    p.add_argument('--qemu-root', required=True, type=Path)
    p.add_argument('--kernel', required=True, type=Path)
    p.add_argument('--capacity-gib', default=16, type=int)
    p.add_argument('--accel', choices=('kvm', 'tcg'), default='tcg')
    p.add_argument('--mode', choices=('raw', 'windows-installer'), default='raw')
    p.add_argument('--windows-index', default=1, type=int)
    p.add_argument('--uefi-ntfs', action='store_true',
                   help='Use the two-partition UEFI:NTFS Windows layout')
    p.add_argument('--unattended-vm-setup', action='store_true',
                   help='DANGER: generated media erases Windows Disk 0 on boot; disposable VMs only')
    p.add_argument('--timeout', default=5400, type=int)
    a = p.parse_args()
    if a.capacity_gib < 1 or a.windows_index < 1 or a.timeout < 1:
        p.error('Capacity, image index and timeout must be positive')
    if a.unattended_vm_setup and a.mode != 'windows-installer':
        p.error('Unattended VM Setup requires Windows installer mode')
    if a.uefi_ntfs and a.mode != 'windows-installer':
        p.error('UEFI:NTFS requires Windows installer mode')
    image = regular(a.image)
    kernel = regular(a.kernel)
    reference = regular(a.expanded_reference) if a.expanded_reference else image
    if a.mode == 'raw' and image.suffix in ('.bz2', '.xz', '.gz', '.zst') and not a.expanded_reference:
        p.error('Compressed RAW qualification needs an independently expanded reference')
    source_hash = digest(image)
    if a.expected_sha256 and source_hash != a.expected_sha256.lower():
        p.error('Image does not match the expected SHA-256')
    capacity = a.capacity_gib * 1024**3
    if a.mode == 'raw' and reference.stat().st_size > capacity:
        p.error('Expanded image exceeds virtual target capacity')
    q = a.qemu_root.resolve()
    w = prepare_work(a.work)
    inputs = w / 'inputs'; inputs.mkdir()
    copied = inputs / image.name
    subprocess.run(['cp', '--reflink=auto', '--', str(image), str(copied)], check=True)
    if digest(copied) != source_hash:
        raise ValueError('Source changed while copying')
    input_disk = w / 'input.img'
    input_size = max(1024**3, (image.stat().st_size * 12 // 10 + 256 * 1024**2))
    with input_disk.open('xb') as output:
        output.truncate(input_size)
    subprocess.run(['mke2fs', '-q', '-F', '-t', 'ext4', '-d', str(inputs), str(input_disk)], check=True)
    guest = w / 'guest'
    shutil.copytree(a.guest_template.resolve(), guest, symlinks=True)
    legacy = guest / 'opt/lufus'
    if legacy.is_symlink():
        legacy.unlink()
    elif legacy.exists():
        shutil.rmtree(legacy)
    runtime = guest / 'opt/tuxflash'
    if runtime.is_symlink():
        runtime.unlink()
    elif runtime.exists():
        shutil.rmtree(runtime)
    shutil.copytree(a.runtime.resolve(), runtime, symlinks=True)
    helper = runtime / 'libexec/.tuxflash-helper.bin'
    helper_hash = digest(regular(helper))
    (guest / 'init').write_text(init_script(image.name, capacity, a.mode, a.windows_index, a.unattended_vm_setup, a.uefi_ntfs))
    (guest / 'init').chmod(0o755)
    initrd = w / 'initramfs.cpio.gz'
    with initrd.open('xb') as output:
        find = subprocess.Popen(['find', '.', '-print0'], cwd=guest, stdout=subprocess.PIPE)
        cpio = subprocess.Popen(['cpio', '--null', '-o', '--format=newc', '--owner=0:0', '--quiet'],
                                cwd=guest, stdin=find.stdout, stdout=subprocess.PIPE)
        find.stdout.close()
        subprocess.run(['gzip', '-1'], stdin=cpio.stdout, stdout=output, check=True)
        cpio.stdout.close()
        if find.wait() or cpio.wait():
            raise RuntimeError('Initramfs preparation failed')
    disk = w / 'target.img'
    with disk.open('xb') as output:
        output.truncate(capacity)
    if not stat.S_ISREG(disk.lstat().st_mode) or disk.resolve().parent != w:
        raise ValueError('Only the newly reserved virtual target is allowed')
    args = [str(q / 'usr/bin/qemu-system-x86_64'), '-L', str(q / 'usr/share/qemu'),
            '-bios', str(q / 'usr/share/seabios/bios-256k.bin'), '-machine', 'q35',
            '-accel', 'kvm' if a.accel == 'kvm' else 'tcg,thread=multi',
            '-cpu', 'host' if a.accel == 'kvm' else 'max', '-m', '1536', '-smp', '2',
            '-nodefaults', '-nographic', '-serial', 'stdio', '-monitor', 'none', '-no-reboot',
            '-kernel', str(kernel), '-initrd', str(initrd),
            '-append', 'console=ttyS0 quiet panic=-1 rdinit=/init rootfstype=ramfs',
            '-device', 'qemu-xhci,id=xhci', '-drive',
            f'if=none,file={disk},format=raw,id=target,cache=none',
            '-device', 'usb-storage,bus=xhci.0,drive=target,removable=on,serial=TF_VIRTUAL_TEST',
            '-drive', f'if=virtio,file={input_disk},format=raw,readonly=on',
            '-nic', 'none']
    env = dict(os.environ, LD_LIBRARY_PATH=str(q / 'usr/lib/x86_64-linux-gnu'))
    started = time.monotonic()
    try:
        with (w / 'serial.log').open('xb') as log:
            run = subprocess.run(args, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=a.timeout)
        text = (w / 'serial.log').read_text(errors='replace')
        complete, passed = completion(text, run.returncode)
        result = {'mode': a.mode, 'image': image.name, 'input_sha256': source_hash,
                  'helper_sha256': helper_hash, 'acceleration': a.accel.upper(),
                  'elapsed_seconds': round(time.monotonic() - started, 2),
                  'helper_completed': passed, 'qemu_exit': run.returncode,
                  'physical_devices_accessed': False, 'guest_network_enabled': False,
                  'unattended_disk0_media': a.unattended_vm_setup, 'uefi_ntfs': a.uefi_ntfs,
                  'installation_or_boot_qualified': False}
        if a.mode == 'raw' and passed:
            count = reference.stat().st_size
            result.update(bytes_verified=count, independent_reference_sha256=digest(reference),
                          independent_target_sha256=digest(disk, count),
                          automatic_readback_passed='VERIFIED Image verification succeeded' in text,
                          cache_invalidated='Device cache invalidated before verification' in text)
            passed = (complete.isdecimal() and int(complete) == count
                      and result['automatic_readback_passed'] and result['cache_invalidated']
                      and result['independent_target_sha256'] == result['independent_reference_sha256'])
        result['passed'] = passed
    except subprocess.TimeoutExpired:
        result = {'passed': False, 'timeout': True, 'helper_sha256': helper_hash,
                  'physical_devices_accessed': False, 'installation_or_boot_qualified': False}
    (w / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2), flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
