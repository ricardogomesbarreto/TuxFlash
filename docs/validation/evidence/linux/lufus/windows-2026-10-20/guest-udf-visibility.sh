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
/usr/bin/mount -o loop,ro "$iso" /mnt/iso || fatal default-mount
findmnt /mnt/iso
printf 'UDF DEFAULT regular-files='
find /mnt/iso -type f | wc -l
if find /mnt/iso -type f | grep -q '/sources/ws.dat$'; then echo 'UDF DEFAULT ws.dat listed'; else echo 'UDF DEFAULT ws.dat omitted'; fi
/usr/bin/umount /mnt/iso || fatal default-unmount
/usr/bin/mount -t udf -o loop,ro,unhide "$iso" /mnt/iso || fatal unhide-mount
findmnt /mnt/iso
printf 'UDF UNHIDE regular-files='
find /mnt/iso -type f | wc -l
find /mnt/iso -type f | grep '/sources/ws.dat$' || fatal hidden-file
sha256sum /mnt/iso/sources/ws.dat
/usr/bin/umount /mnt/iso || fatal unhide-unmount
echo 'UDF VISIBILITY PASS'
sync
poweroff -f
