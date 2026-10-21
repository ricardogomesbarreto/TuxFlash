#!/bin/sh
export PATH=/opt/lufus/bin:/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/opt/lufus/lib
export LC_ALL=C
/usr/bin/mount -t proc proc /proc
/usr/bin/mount -t sysfs sysfs /sys
/usr/bin/mount -t devtmpfs devtmpfs /dev
/usr/bin/mount -t tmpfs tmpfs /tmp
mkdir -p /dev/pts /mnt/windows /mnt/esp
/usr/bin/mount -t devpts devpts /dev/pts
for mod in usb-storage ntfs3 nls_cp437 nls_iso8859-1 fuse; do modprobe "$mod" || true; done
fatal() { echo "BCD REPAIR FATAL $*"; sync; poweroff -f; }
sleep 2
[ "$(blockdev --getsize64 /dev/sda)" = 34359738368 ] || fatal identity
/usr/bin/mount /dev/sda1 /mnt/esp || fatal esp
/usr/bin/mount -t ntfs3 /dev/sda2 /mnt/windows || fatal windows
export BCD_SYS_ALLOW_ROOT=1
/opt/lufus/libexec/bcd-sys/AppRun /mnt/windows --firmware uefi --syspath /mnt/esp --clean --verbose || fatal provision
bcd=/mnt/esp/EFI/Microsoft/Boot/BCD
default=$(printf 'cd Objects\\{9dea862c-5cdd-4e70-acc1-f32b344d4795}\\Elements\\23000003\nlsval Element\n' | hivexsh "$bcd")
display=$(printf 'cd Objects\\{9dea862c-5cdd-4e70-acc1-f32b344d4795}\\Elements\\24000001\nlsval Element\n' | hivexsh "$bcd")
printf 'DEFAULT %s\nDISPLAY %s\n' "$default" "$display"
[ "$default" = "$display" ] || fatal references
type=$(printf 'cd Objects\\%s\\Description\nlsval Type\n' "$default" | hivexsh "$bcd")
[ "$type" = 270532611 ] || fatal loader-type
sync
/usr/bin/umount /mnt/windows || fatal windows-unmount
/usr/bin/umount /mnt/esp || fatal esp-unmount
blockdev --flushbufs /dev/sda || fatal flush
echo 'BCD REPAIR PASS'
poweroff -f
