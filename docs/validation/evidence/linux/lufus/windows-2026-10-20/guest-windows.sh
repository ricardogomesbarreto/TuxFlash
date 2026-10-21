#!/bin/sh
export PATH=/opt/lufus/bin:/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/opt/lufus/lib
export LC_ALL=C
/usr/bin/mount -t proc proc /proc
/usr/bin/mount -t sysfs sysfs /sys
/usr/bin/mount -t devtmpfs devtmpfs /dev
/usr/bin/mount -t tmpfs tmpfs /tmp
mkdir -p /dev/pts /mnt/source /mnt/iso /mnt/check /mnt/esp
/usr/bin/mount -t devpts devpts /dev/pts
for mod in usb-storage isofs udf ntfs3 nls_cp437 nls_iso8859-1 fuse 9pnet_virtio 9p; do modprobe "$mod" || true; done
fatal() { echo "WINDOWS FATAL $*"; sync; poweroff -f; }
/usr/bin/mount -t 9p -o trans=virtio,version=9p2000.L,ro windows-source /mnt/source || fatal source
iso=/mnt/source/windows.iso
h=/opt/lufus/libexec/lufus-helper
sha256sum "$h"
/usr/bin/mount -o loop,ro "$iso" /mnt/iso || fatal iso
wimlib-imagex info /mnt/iso/sources/install.esd || fatal info
/usr/bin/umount /mnt/iso || fatal unmount-iso
sleep 2
lsblk -b -o NAME,SIZE,TYPE,RM,MAJ:MIN
installer=$(for x in /sys/class/block/sd?; do [ "$(cat "$x/size")" = 30322688 ] && echo /dev/${x##*/}; done)
togo=$(for x in /sys/class/block/sd?; do [ "$(cat "$x/size")" = 67108864 ] && echo /dev/${x##*/}; done)
[ -b "$installer" ] && [ -b "$togo" ] || fatal targets
id=$(cat /sys/class/block/${installer##*/}/dev)
echo 'WINDOWS START real-installer-uefi-fat32'
# Provisioned by the preceding native-helper run; compare it without rewriting.
blockdev --flushbufs "$installer" || fatal installer-cache
/usr/bin/mount -o ro "${installer}1" /mnt/check || fatal mount-installer
/usr/bin/mount -o loop,ro "$iso" /mnt/iso || fatal source-remount
(cd /mnt/iso && find . -type f -exec sha256sum '{}' \;) > /tmp/source-files.sha256 || fatal manifest
(cd /mnt/check && sha256sum -c /tmp/source-files.sha256) || fatal compare-files
test -s /mnt/check/efi/boot/bootx64.efi || fatal efi
/usr/bin/umount /mnt/iso || fatal iso-unmount
/usr/bin/umount /mnt/check || fatal installer-unmount
fsck.vfat -n "${installer}1" || fatal installer-fsck
echo 'WINDOWS PASS real-installer-uefi-fat32-and-all-file-hashes'
id=$(cat /sys/class/block/${togo##*/}/dev)
echo 'WINDOWS START real-windows-to-go-32gib'
"$h" windows-to-go --source "$iso" --target "$togo" --major-minor "$id" --size 34359738368 --windows-index 4 --windows-flags 32 --architecture amd64 || fatal togo
blockdev --flushbufs "$togo" || fatal togo-cache
/usr/bin/mount -o ro "${togo}1" /mnt/esp || fatal esp-mount
/usr/bin/mount -t ntfs3 -o ro "${togo}2" /mnt/check || fatal togo-mount
test -s /mnt/esp/EFI/Microsoft/Boot/BCD || fatal bcd
test -s /mnt/esp/EFI/Boot/bootx64.efi || fatal togo-efi
test -s /mnt/check/Windows/System32/ntoskrnl.exe || fatal kernel
test -s /mnt/check/Windows/System32/winload.efi || fatal winload
printf 'cd Objects\nls\n' | hivexsh /mnt/esp/EFI/Microsoft/Boot/BCD || fatal bcd-parse
printf 'cd ControlSet001\\Services\\partmgr\\Parameters\nlsval SanPolicy\n' | hivexsh /mnt/check/Windows/System32/config/SYSTEM > /tmp/san-policy || fatal san-policy
cat /tmp/san-policy
[ "$(cat /tmp/san-policy)" = 4 ] || fatal san-value
/usr/bin/umount /mnt/check || fatal togo-unmount
/usr/bin/umount /mnt/esp || fatal esp-unmount
echo 'WINDOWS PASS real-windows-to-go-image-bcd-san-policy'
echo 'WINDOWS SUMMARY passed=2 failed=0'
sync
poweroff -f
