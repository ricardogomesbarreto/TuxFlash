#!/bin/sh
export PATH=/opt/lufus/bin:/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/opt/lufus/lib
export LC_ALL=C
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs tmpfs /tmp
mkdir -p /dev/pts /run /mnt/check /home
mount -t devpts devpts /dev/pts
modprobe usb-storage
modprobe isofs
modprobe udf
modprobe ntfs3
modprobe nls_cp437 2>/dev/null || true
modprobe nls_iso8859-1
modprobe fuse 2>/dev/null || true
sleep 2
lsblk -o NAME,SIZE,TYPE,RM,MAJ:MIN
h=/opt/lufus/libexec/lufus-helper
ln -sf /work/windows.iso "/work/Windows 10 PRO 2021.iso"
sha256sum "$h" /work/*.iso
d=$(for x in /sys/class/block/sd?; do [ "$(cat "$x/size")" = 131072 ] && echo /dev/${x##*/}; done)
[ -b "$d" ] || { echo "QA FATAL missing 64MiB disk"; poweroff -f; }
disk_name=${d##*/}
ident=$(cat /sys/class/block/$disk_name/dev)
size=$(blockdev --getsize64 "$d")
passed=0
failed=0
run_case() {
 name=$1
 shift
 echo "QA START $name"
 (set -e; "$@") > /tmp/case.log 2>&1
 status=$?
 umount /home 2>/dev/null || true
 if [ "$status" -eq 0 ]; then
  passed=$((passed+1)); cat /tmp/case.log; echo "QA PASS $name"
 else
  failed=$((failed+1)); echo "QA FAIL $name"; cat /tmp/case.log
 fi
}
write_image() { "$h" write --source "$1" --target "$d" --major-minor "$ident" --size "$size" --verify; }
reject() {
 if "$@"; then echo 'Unexpected success'; return 1; fi
}
raw_roundtrip() {
 write_image /work/source.img
 dd if="$d" of=/tmp/readback bs=1M count=8 status=none
 cmp /work/source.img /tmp/readback
}
compressed_roundtrip() {
 write_image "/work/source.img.$1"
 dd if="$d" of=/tmp/readback bs=1M count=8 status=none
 cmp /work/source.img /tmp/readback
}
verify_existing() {
 "$h" verify --source /work/source.img --target "$d" --major-minor "$ident" --size "$size"
}
verify_existing_unchanged() {
 before=$(sha256sum "$d")
 verify_existing
 [ "$before" = "$(sha256sum "$d")" ]
}
verify_wrong_byte() {
 printf X | dd of="$d" bs=1 count=1 conv=notrunc,fsync status=none
 before=$(sha256sum "$d")
 reject verify_existing
 [ "$before" = "$(sha256sum "$d")" ]
 raw_roundtrip
}
verify_rejects_pretest() {
 before=$(sha256sum "$d")
 set +e
 "$h" verify --source /work/source.img --target "$d" --major-minor "$ident" --size "$size" --passes 1
 result=$?
 set -e
 [ "$result" -eq 64 ]
 [ "$before" = "$(sha256sum "$d")" ]
}
capture_roundtrip() {
 fmt=$1
 out=/tmp/capture.$fmt
 "$h" capture --target "$d" --major-minor "$ident" --size "$size" --output "$out" --capture-format "$fmt"
 if [ "$fmt" = raw ]; then
  cmp -n "$size" "$out" "$d"
 else
  qemu-img compare -f "$( [ "$fmt" = vhd ] && echo vpc || echo vhdx )" -F raw "$out" "$d"
  qemu-img info "$out"
  write_image "$out"
 fi
 rm "$out"
}
wrong_identity() {
 before=$(sha256sum "$d")
 reject "$h" write --source /work/source.img --target "$d" --major-minor 255:255 --size "$size"
 [ "$before" = "$(sha256sum "$d")" ]
}
wrong_capacity() {
 before=$(sha256sum "$d")
 reject "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size 1
 [ "$before" = "$(sha256sum "$d")" ]
}
protected_mount() {
 mkfs.ext4 -F "$d"
 mount -o ro "$d" /home
 before=$(sha256sum "$d")
 reject "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$size"
 [ "$before" = "$(sha256sum "$d")" ]
 umount /home
}
partition_guard() {
 echo 'label: dos

size=8192,type=83' | sfdisk "$d"
 sleep 1
 pident=$(cat /sys/class/block/${disk_name}1/dev)
 reject "$h" write --source /work/source.img --target "${d}1" --major-minor "$pident" --size 4194304
}
oversized() {
 truncate -s 128M /tmp/large.img
 reject write_image /tmp/large.img
 rm /tmp/large.img
}
existing_capture() {
 echo keep > /tmp/exists.vhd
 reject "$h" capture --target "$d" --major-minor "$ident" --size "$size" --output /tmp/exists.vhd --capture-format vhd
 [ "$(cat /tmp/exists.vhd)" = keep ]
}
near_capacity() {
 before=$(sha256sum "$d")
 reject "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$((size+512))"
 [ "$before" = "$(sha256sum "$d")" ]
}
wrong_sequence() {
 seq=$(cat /sys/class/block/$disk_name/diskseq)
 before=$(sha256sum "$d")
 reject "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$size" --disk-seq "$((seq+1))"
 [ "$before" = "$(sha256sum "$d")" ]
}
correct_sequence() {
 seq=$(cat /sys/class/block/$disk_name/diskseq)
 "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$size" --disk-seq "$seq" --verify
}
read_only_guard() {
 before=$(sha256sum "$d")
 blockdev --setro "$d"
 set +e
 "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$size"
 status=$?
 "$h" verify --source /work/source.img --target "$d" --major-minor "$ident" --size "$size"
 read_status=$?
 blockdev --setrw "$d"
 set -e
 [ "$status" -ne 0 ] && [ "$read_status" -eq 0 ] && [ "$before" = "$(sha256sum "$d")" ]
}
protected_submount() {
 mkfs.ext4 -F "$d"
 mkdir -p /home/guard-test
 mount -o ro "$d" /home/guard-test
 before=$(sha256sum "$d")
 set +e
 "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$size"
 status=$?
 after=$(sha256sum "$d")
 umount /home/guard-test
 set -e
 [ "$status" -ne 0 ] && [ "$before" = "$after" ]
}
active_swap_guard() {
 mkswap "$d"
 swapon "$d"
 before=$(sha256sum "$d")
 set +e
 "$h" write --source /work/source.img --target "$d" --major-minor "$ident" --size "$size"
 status=$?
 swapoff "$d"
 set -e
 [ "$status" -ne 0 ] && [ "$before" = "$(sha256sum "$d")" ]
}
run_case exact-capacity-guard near_capacity
run_case disk-sequence-guard wrong_sequence
run_case disk-sequence-valid correct_sequence
run_case read-only-guard read_only_guard
run_case protected-submount-guard protected_submount
run_case active-swap-guard active_swap_guard
printf "cd Select\nlsval Current\n" | hivexsh /work/SYSTEM-fixture
run_case device-identity-guard wrong_identity
run_case device-capacity-guard wrong_capacity
run_case protected-mount-guard protected_mount
run_case partition-guard partition_guard
run_case raw-write-verify raw_roundtrip
run_case verify-existing-without-rewrite verify_existing_unchanged
run_case verify-detects-wrong-byte verify_wrong_byte
run_case verify-rejects-pretest verify_rejects_pretest
for fmt in gz xz bz2 zst; do run_case compressed-$fmt compressed_roundtrip "$fmt"; done
run_case reject-corrupt-gzip reject write_image /work/corrupt.img.gz
run_case reject-truncated-gzip reject write_image /work/truncated.img.gz
run_case reject-oversize oversized
run_case reject-ffu reject write_image /work/fake.ffu
run_case capture-existing-guard existing_capture
cancel_write() {
 cp /work/source.img /tmp/cancel.img
 truncate -s 64M /tmp/cancel.img
 "$h" write --source /tmp/cancel.img --target "$d" --major-minor "$ident" --size "$size" > /tmp/cancel.log 2>&1 &
 pid=$!
 for attempt in $(seq 1 300); do
  grep -q STARTED /tmp/cancel.log && break
  usleep 10000
 done
 kill -TERM "$pid"
 set +e
 wait "$pid"
 result=$?
 set -e
 cat /tmp/cancel.log
 [ "$result" -ne 0 ]
 grep -qi cancelled /tmp/cancel.log
 ! grep -q '^COMPLETE' /tmp/cancel.log
 rm /tmp/cancel.img
}
device_test_case() {
 "$h" device-test --target "$d" --major-minor "$ident" --size "$size" --passes 4
}

run_case cancellation cancel_write
run_case device-test-four-passes device_test_case
run_case restore-source raw_roundtrip
for fmt in raw vhd vhdx; do run_case capture-$fmt-roundtrip capture_roundtrip "$fmt"; done
media=$(for x in /sys/class/block/sd?; do [ "$(cat "$x/size")" = 2097152 ] && echo /dev/${x##*/}; done)
media_name=${media##*/}
media_ident=$(cat /sys/class/block/$media_name/dev)
media_size=$(blockdev --getsize64 "$media")
# /init uses busybox ash, so use shift instead of Bash argument slicing.
media_helper() { op=$1; image=$2; shift 2; "$h" "$op" --source "$image" --target "$media" --major-minor "$media_ident" --size "$media_size" "$@"; }
media_cleanup() {
 for p in /mnt/check /mnt/esp /mnt/persist; do umount "$p" 2>/dev/null || true; done
}
persistence_case() {
 flavor=$1
 media_helper linux-persistent "/work/$flavor.iso" --persistence-size 268435456
 [ "$(blkid -s TYPE -o value "${media}1")" = ntfs ]
 [ "$(blkid -s TYPE -o value "${media}2")" = ext4 ]
 mount -t ntfs3 "${media}1" /mnt/check
 if [ "$flavor" = casper ]; then
  grep 'persistent ---' /mnt/check/boot/grub/grub.cfg
  [ "$(blkid -s LABEL -o value "${media}2")" = casper-rw ]
 else
  grep 'boot=live persistence' /mnt/check/boot/grub/grub.cfg
  mount "${media}2" /mnt/persist
  [ "$(cat /mnt/persist/persistence.conf)" = '/ union' ]
  umount /mnt/persist
 fi
 umount /mnt/check
 e2fsck -fn "${media}2"
}
windows_case() {
 mode=$1
 shift
 media_helper windows-installer "/work/Windows 10 PRO 2021.iso" --target-system "$mode" --windows-flags 0 "$@"
 expected='WINDOWS 10'
 if [ "$mode" != uefi ] || [ "${1:-}" = --uefi-ntfs ]; then
  expected='Windows 10 PRO 2021'
 fi
 [ "$(blkid -p -s LABEL -o value "${media}1")" = "$expected" ]
 if [ "$mode" = uefi ]; then
  [ "$(blkid -p -s PART_ENTRY_NAME -o value "${media}1")" = "$expected" ]
 fi
 mount "${media}1" /mnt/check
 cmp /work/efi-fixture.efi /mnt/check/efi/boot/bootx64.efi
 cmp /work/windows-marker.txt /mnt/check/fixture.txt
 test -f /mnt/check/sources/boot.wim
 umount /mnt/check
}
windows_options_case() {
 media_helper windows-installer /work/windows.iso --target-system uefi --windows-flags 31 --architecture amd64 --local-account Tester
 [ "$(blkid -p -s LABEL -o value "${media}1")" = WINDOWS ]
 [ "$(blkid -p -s PART_ENTRY_NAME -o value "${media}1")" = WINDOWS ]
 mount "${media}1" /mnt/check
 test -s /mnt/check/sources/appraiserres.bak
 test ! -s /mnt/check/sources/appraiserres.dll
 wimlib-imagex extract /mnt/check/sources/boot.wim 2 /Autounattend.xml --dest-dir=/tmp/unattend
 grep 'Tester' /tmp/unattend/Autounattend.xml
 grep 'BypassTPMCheck' /tmp/unattend/Autounattend.xml
 umount /mnt/check
}
windows_oem_case() {
 media_helper windows-installer "/work/Windows 10 PRO 2021.iso" --target-system uefi --windows-flags 2
 [ "$(blkid -p -s LABEL -o value "${media}1")" = "WINDOWS 10" ]
 [ "$(blkid -p -s PART_ENTRY_NAME -o value "${media}1")" = "WINDOWS 10" ]
 mount "${media}1" /mnt/check
 test -s '/mnt/check/sources/$OEM$/$$/Panther/unattend.xml'
 umount /mnt/check
}
validation_case() {
 media_helper windows-installer /work/windows.iso --target-system uefi --windows-flags 0 --runtime-validation
 mount "${media}1" /mnt/check
 cmp /work/efi-fixture.efi /mnt/check/efi/boot/bootx64_original.efi
 test -s /mnt/check/md5sum.txt
 (cd /mnt/check; grep -v "^#" md5sum.txt | md5sum -c -)
 umount /mnt/check
}
togo_case() {
 disk=$(for x in /sys/class/block/sd?; do [ "$(cat "$x/size")" = 16777216 ] && echo /dev/${x##*/}; done; true)
 test -b "$disk"
 id=$(cat /sys/class/block/${disk##*/}/dev)
 "$h" windows-to-go --source /work/windows.iso --target "$disk" --major-minor "$id" --size 8589934592 --windows-index 1 --windows-flags 32
 [ "$(blkid -p -s LABEL -o value "${disk}2")" = windows ]
 [ "$(blkid -p -s PART_ENTRY_NAME -o value "${disk}2")" = windows ]
 mount "${disk}1" /mnt/esp
 mount -t ntfs3 "${disk}2" /mnt/check
 test -s /mnt/esp/EFI/Microsoft/Boot/BCD
 cmp /work/efi-fixture.efi /mnt/esp/EFI/Boot/bootx64.efi
 printf 'cd Objects\nls\n' | hivexsh /mnt/esp/EFI/Microsoft/Boot/BCD
 # A readable hive is insufficient: Boot Manager needs UTF-16 GUID lists.
 bcd=/mnt/esp/EFI/Microsoft/Boot/BCD
 default=$(printf 'cd Objects\\{9dea862c-5cdd-4e70-acc1-f32b344d4795}\\Elements\\23000003\nlsval Element\n' | hivexsh "$bcd")
 display=$(printf 'cd Objects\\{9dea862c-5cdd-4e70-acc1-f32b344d4795}\\Elements\\24000001\nlsval Element\n' | hivexsh "$bcd")
 [ "$default" = "$display" ]
 type=$(printf 'cd Objects\\%s\\Description\nlsval Type\n' "$default" | hivexsh "$bcd")
 [ "$type" = 270532611 ]
 printf 'cd ControlSet001\\Services\\partmgr\\Parameters\nlsval SanPolicy\n' | hivexsh /mnt/check/Windows/System32/config/SYSTEM > /tmp/san-policy
 cat /tmp/san-policy
 [ "$(cat /tmp/san-policy)" = 4 ]
 test -f /mnt/check/Windows/System32/fixture.txt
 umount /mnt/check
 umount /mnt/esp
}
mkdir -p /mnt/esp /mnt/persist
run_case casper-persistence-layout persistence_case casper
media_cleanup
run_case debian-persistence-layout persistence_case debian
media_cleanup
run_case windows-uefi-ntfs windows_case uefi --uefi-ntfs
media_cleanup
run_case windows-bios-ntfs windows_case bios
media_cleanup
run_case windows-hybrid-ntfs windows_case bios-uefi
media_cleanup
run_case windows-setup-options windows_options_case
media_cleanup
run_case windows-oem-options windows_oem_case
media_cleanup
run_case runtime-validation-manifest validation_case
media_cleanup
run_case windows-to-go-provisioning togo_case
media_cleanup
run_case windows-uefi-fat32 windows_case uefi
media_cleanup

# Leave the BIOS fixture on sda for the subsequent firmware boot.
run_case final-boot-image raw_roundtrip
echo "QA SUMMARY passed=$passed failed=$failed"
sync
poweroff -f
