#!/bin/sh
export PATH=/opt/lufus/bin:/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/opt/lufus/lib
export LC_ALL=C
/usr/bin/mount -t proc proc /proc
/usr/bin/mount -t sysfs sysfs /sys
/usr/bin/mount -t devtmpfs devtmpfs /dev
/usr/bin/mount -t tmpfs tmpfs /tmp
mkdir -p /dev/pts /mnt/source /mnt/iso /mnt/check
/usr/bin/mount -t devpts devpts /dev/pts
for mod in usb-storage isofs udf ntfs3 nls_cp437 nls_iso8859-1 fuse 9pnet_virtio 9p; do modprobe "$mod" || true; done
fatal() { echo "WINDOWS FIXED FATAL $*"; sync; poweroff -f; }
/usr/bin/mount -t 9p -o trans=virtio,version=9p2000.L,ro windows-source /mnt/source || fatal source
iso=/mnt/source/windows.iso
h=/opt/lufus/libexec/lufus-helper
sha256sum "$h"
sleep 2
disk=$(for x in /sys/class/block/sd?; do [ "$(cat "$x/size")" = 30322688 ] && echo /dev/${x##*/}; done)
[ -b "$disk" ] || fatal disk
id=$(cat /sys/class/block/${disk##*/}/dev)
# No pre-mounted ISO may influence the helper's source mount flags.
"$h" windows-installer --source "$iso" --target "$disk" --major-minor "$id" --size 15525216256 --target-system uefi --windows-flags 0 --architecture amd64 || fatal installer
blockdev --flushbufs "$disk" || fatal cache
/usr/bin/mount -o ro "${disk}1" /mnt/check || fatal target-mount
/usr/bin/mount -o loop,ro,unhide "$iso" /mnt/iso || fatal reference-mount
findmnt /mnt/iso
(cd /mnt/iso && find . -type f -exec sha256sum '{}' \;) > /tmp/source-files.sha256 || fatal manifest
[ "$(wc -l < /tmp/source-files.sha256)" = 906 ] || fatal source-count
(cd /mnt/check && sha256sum -c /tmp/source-files.sha256) || fatal comparison
test -s /mnt/check/sources/ws.dat || fatal hidden-file
sha256sum /mnt/check/sources/ws.dat
/usr/bin/umount /mnt/iso || fatal source-unmount
/usr/bin/umount /mnt/check || fatal target-unmount
fsck.vfat -n "${disk}1" || fatal fsck
echo 'WINDOWS FIXED SUMMARY files=906 failed=0'
sync
poweroff -f
