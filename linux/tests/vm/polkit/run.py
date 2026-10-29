#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Full-OS Polkit QA. Only new regular image files below /tmp are attached.

Run as an ordinary host user. The guest alone creates an ephemeral QA admin.
Inputs are verified before a single fakeroot preparation session. No offline
patches of a booted filesystem, host mounts, forwarded ports or physical disks.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import stat
import subprocess
import sys

SOURCE_NAME = 'ubuntu-24.04-minimal-cloudimg-amd64-root.tar.xz'
SOURCE_SHA256 = '17900e3d6ae98e1b518e1f8914259ec7474da7f0c707456c0004f0b835fe0ed4'
SOURCE_URL = 'https://cloud-images.ubuntu.com/minimal/releases/noble/release-20261027/'
HERE = Path(__file__).resolve().parent


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def arguments():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--project', type=Path, required=True)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--kernel', type=Path, required=True)
    p.add_argument('--modules', type=Path, required=True)
    p.add_argument('--rootfs-tar', type=Path, required=True)
    p.add_argument('--sums', type=Path, required=True)
    p.add_argument('--signature', type=Path, required=True)
    p.add_argument('--keyring', type=Path, default=Path('/usr/share/keyrings/ubuntu-cloudimage-keyring.gpg'))
    p.add_argument('--qemu-root', type=Path, default=Path('/'))
    p.add_argument('--prepare-child', action='store_true', help=argparse.SUPPRESS)
    return p.parse_args()


def prepare(a):
    if not os.environ.get('FAKEROOTKEY'):
        raise RuntimeError('Preparation must run inside the caller-created fakeroot')
    root = a.work / 'rootfs'
    root.mkdir()
    subprocess.run(['tar', '--same-owner', '--same-permissions', '-xJf', str(a.rootfs_tar), '-C', str(root)], check=True)

    def write(name, text, mode=0o644):
        path = root / name.lstrip('/')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        path.chmod(mode)

    (root / 'var/lib/tuxflash-qa').mkdir(parents=True, exist_ok=True)
    write('/etc/tuxflash-disposable-qa', 'Disposable virtual TuxFlash QA only\n')
    write('/etc/hostname', 'tuxflash-qa\n')
    write('/etc/hosts', '127.0.0.1 localhost\n127.0.1.1 tuxflash-qa\n')
    write('/etc/fstab', '/dev/vda / ext4 defaults 0 1\n')
    (root / 'etc/resolv.conf').unlink(missing_ok=True)
    write('/etc/resolv.conf', 'nameserver 10.0.2.3\n')
    write('/etc/cloud/cloud-init.disabled', 'TuxFlash QA uses explicit setup\n')
    write('/etc/systemd/network/10-qa.network', '[Match]\nName=en* eth*\n[Network]\nDHCP=yes\n')
    write('/root/.qa-secret', secrets.token_urlsafe(24) + '\n', 0o600)
    for src, dst in [('guest-setup.sh', 'qa-setup.sh'), ('guest-user.sh', 'qa-user-runner'), ('guest-controller.py', 'qa-controller.py'), ('guest-supervise.py', 'qa-supervise.py')]:
        out = root / 'usr/local/libexec' / dst
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(HERE / src, out)
        out.chmod(0o755)
    write('/etc/systemd/system/qa-setup.service', '[Unit]\nWants=network-online.target\nAfter=network-online.target\nBefore=getty@tty1.service qa-controller.service\n[Service]\nType=oneshot\nExecStart=/usr/local/libexec/qa-setup.sh\nRemainAfterExit=yes\nTimeoutStartSec=600\n[Install]\nWantedBy=multi-user.target\n')
    write('/etc/systemd/system/qa-controller.service', '[Unit]\nRequires=qa-setup.service\nAfter=qa-setup.service\n[Service]\nExecStart=/usr/local/libexec/qa-controller.py\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/ttyS0\n[Install]\nWantedBy=multi-user.target\n')
    write('/etc/systemd/system/getty@tty1.service.d/qa.conf', '[Unit]\nRequires=qa-setup.service\nAfter=qa-setup.service\n[Service]\nExecStart=\nExecStart=-/sbin/agetty --autologin tuxqa --noclear %I linux\n')
    wants = root / 'etc/systemd/system/multi-user.target.wants'
    wants.mkdir(parents=True, exist_ok=True)
    for unit in ('qa-setup.service', 'qa-controller.service', 'getty@tty1.service', 'systemd-networkd.service', 'systemd-networkd-wait-online.service'):
        link = wants / unit
        link.unlink(missing_ok=True)
        link.symlink_to('../' + unit if unit.startswith('qa-') else '/usr/lib/systemd/system/' + unit)
    serial_getty = root / 'etc/systemd/system/serial-getty@ttyS0.service'
    serial_getty.unlink(missing_ok=True)
    serial_getty.symlink_to('/dev/null')
    default = root / 'etc/systemd/system/default.target'
    default.unlink(missing_ok=True)
    default.symlink_to('/usr/lib/systemd/system/multi-user.target')
    shutil.copytree(a.runtime, root / 'opt/tuxflash/runtime', symlinks=True)
    shutil.copytree(a.modules, root / 'usr/lib/modules' / a.modules.name,
                    symlinks=True, ignore=shutil.ignore_patterns('build', 'source'))
    shutil.copy2(a.binary, root / 'usr/bin/tuxflash-polkit-test')
    write('/usr/libexec/tuxflash-helper', '#!/bin/sh\nexec /opt/tuxflash/runtime/libexec/tuxflash-helper "$@"\n', 0o755)
    policy = root / 'usr/share/polkit-1/actions/org.tuxflash.TuxFlash.policy'
    policy.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(a.project / 'linux/packaging/org.tuxflash.TuxFlash.policy', policy)
    image = a.work / 'root.img'
    with image.open('xb') as out:
        out.truncate(3 * 1024**3)
    image.chmod(0o600)
    subprocess.run(['mke2fs', '-q', '-F', '-t', 'ext4', '-d', str(root), str(image)], check=True)


def main():
    a = arguments()
    for key, value in vars(a).items():
        if isinstance(value, Path):
            setattr(a, key, value.resolve())
    tmp = Path("/tmp").resolve()
    if tmp not in a.work.parents:
        raise RuntimeError('Use a new work directory below /tmp')
    if any(c in str(path) for path in (a.work, a.qemu_root) for c in (',', '\n', '\r')):
        raise RuntimeError('QEMU option separators are not allowed in work/firmware paths')
    if a.prepare_child:
        if not (a.work / '.tuxflash-disposable-polkit-qa').is_file():
            raise RuntimeError('Missing private work marker')
        prepare(a)
        return
    if os.geteuid() == 0:
        raise RuntimeError('Run the host harness without sudo')
    a.work.mkdir(parents=True, exist_ok=True)
    if any(a.work.iterdir()):
        raise RuntimeError('Work directory must be empty; previously booted images are never patched')
    for name in ('kernel', 'binary', 'rootfs_tar', 'sums', 'signature', 'keyring'):
        if not getattr(a, name).is_file():
            raise RuntimeError('Inputs must be regular files; devices are not accepted')
    if not a.modules.is_dir() or not a.runtime.is_dir():
        raise RuntimeError('Module and runtime directories are required')
    (a.work / '.tuxflash-disposable-polkit-qa').write_text('Disposable virtual Polkit QA\n')
    subprocess.run(['gpgv', '--keyring', str(a.keyring), str(a.signature), str(a.sums)], check=True)
    expected = next(row.split()[0] for row in a.sums.read_text().splitlines() if row.split()[-1].lstrip('*') == SOURCE_NAME)
    if expected != SOURCE_SHA256 or sha(a.rootfs_tar) != SOURCE_SHA256:
        raise RuntimeError('Unqualified cloud source; update pin/provenance explicitly')
    if '/opt/tuxflash/runtime/lib/ld-linux-x86-64.so.2' not in subprocess.check_output(['readelf', '-l', str(a.binary)], text=True):
        raise RuntimeError('Build the VM-only target with its guest interpreter')
    for item in a.runtime.rglob('*'):
        if item.is_symlink() and not item.resolve().is_relative_to(a.runtime):
            raise RuntimeError('Runtime has an external symlink')
    command = ['fakeroot', '--', sys.executable, str(Path(__file__).resolve())]
    for name, value in vars(a).items():
        if name != 'prepare_child':
            command += ['--' + name.replace('_', '-'), str(value)]
    subprocess.run(command + ['--prepare-child'], check=True)
    subprocess.run(['e2fsck', '-fn', str(a.work / 'root.img')], check=True)
    # The image contains an independent copy; do not retain duplicate host trees.
    shutil.rmtree(a.work / 'rootfs')
    usb = a.work / 'usb.img'
    with usb.open('xb') as out:
        out.truncate(64 * 1024**2)
    usb.chmod(0o600)
    for image in (a.work / 'root.img', usb):
        if not stat.S_ISREG(image.lstat().st_mode) or image.resolve().parent != a.work:
            raise RuntimeError('Only private regular image files may be attached')
    q = a.qemu_root
    cmd = [str(q / 'usr/bin/qemu-system-x86_64'), '-L', str(q / 'usr/share/qemu'),
           '-bios', str(q / 'usr/share/seabios/bios-256k.bin'), '-machine', 'q35',
           '-accel', 'tcg,thread=multi', '-cpu', 'max', '-m', '1408', '-smp', '2',
           '-vga', 'none', '-device', 'VGA,romfile=' + str(q / 'usr/share/seabios/vgabios-stdvga.bin'),
           '-display', 'none', '-serial', 'stdio', '-monitor', 'none', '-no-reboot',
           '-kernel', str(a.kernel), '-append', 'console=ttyS0 root=/dev/vda rw rootfstype=ext4 panic=-1 tuxflash_qa=1',
           '-drive', f'if=virtio,file={a.work}/root.img,format=raw', '-device', 'qemu-xhci,id=xhci',
           '-drive', f'if=none,file={usb},format=raw,id=usb,bps_wr=8388608',
           '-device', 'usb-storage,bus=xhci.0,drive=usb,removable=on', '-netdev', 'user,id=n1,ipv6=off',
           '-device', 'virtio-net-pci,netdev=n1', '-rtc', 'base=utc,clock=host']
    env = dict(os.environ)
    if a.qemu_root != Path('/'):
        env['LD_LIBRARY_PATH'] = str(q / 'usr/lib/x86_64-linux-gnu')
    provenance = {'source_url': SOURCE_URL, 'rootfs_sha256': SOURCE_SHA256,
                  'kernel_sha256': sha(a.kernel), 'fixture_sha256': sha(a.binary),
                  'helper_sha256': sha(a.runtime / 'libexec/.tuxflash-helper.bin'),
                  'policy_sha256': sha(a.project / 'linux/packaging/org.tuxflash.TuxFlash.policy'),
                  'memory_mib': 1408, 'vcpus': 2, 'secure_boot': False, 'acceleration': 'TCG'}
    (a.work / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print('POLKIT VM starting; only two new private regular disk images attached', flush=True)
    with (a.work / 'serial.log').open('wb') as log:
        result = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1600)
    lines = (a.work / 'serial.log').read_text(errors='replace').splitlines()
    summaries = [line[len('POLKIT SUMMARY '):] for line in lines if line.startswith('POLKIT SUMMARY ')]
    if result.returncode != 0 or not summaries:
        raise RuntimeError('VM failed or produced no result; inspect private serial.log')
    summary = json.loads(summaries[-1])
    (a.work / 'results.json').write_text(json.dumps({'provenance': provenance, **summary}, indent=2) + '\n')
    if not summary['passed'] or summary['gui_cases_passed'] != 13:
        raise RuntimeError('Integration case failed; inspect private serial.log')
    print('POLKIT VM PASS: 13 GUI cases and sessionless denial', flush=True)


if __name__ == '__main__':
    main()
