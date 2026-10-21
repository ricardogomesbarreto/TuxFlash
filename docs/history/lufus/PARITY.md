# Rufus-to-Lufus parity matrix — received upstream claims

> **Local validation scope:** this matrix was supplied with the Lufus archive.
> Claims below about complete Windows installation, Windows To Go desktop,
> real Ubuntu/Debian boot and Secure Boot were not reproduced in this integration.
> The local Windows To Go result is partial. Use [COMPATIBILIDADE.md](../../validation/COMPATIBILIDADE.md),
> [TESTE-WINDOWS.md](../../validation/TESTE-WINDOWS.md) and [TESTE-FISICO.md](../../validation/TESTE-FISICO.md)
> for current evidence. The GUI is the supplied GTK Lufus interface, not the original
> Win32 Rufus interface; complete settings and feature parity has not been achieved.

This matrix is intentionally explicit: a visible control is not counted as
working until its backend has been exercised against a USB-emulated block
device and booted where applicable. Physical removable-media qualification is
tracked separately. The Windows source remains the behavioral reference.

| Capability | Lufus 0.2 status | Linux implementation |
| --- | --- | --- |
| Removable whole-disk discovery | Working | `lsblk`, with independent UI/helper filtering |
| Opt-in USB/fixed hard-disk listing | Working | Advanced drive property toggle |
| Raw ISO/IMG/DD writing | Working; VM USB write verified | Privileged streaming helper |
| VHD/VHDX writing | Working; conversion and compare passed | Bundled qemu-img converts the virtual disk, with optional compare |
| FFU writing | Windows-only upstream dependency | Rufus calls private Microsoft FFU provider/DISM; raw-copying an FFU would be corrupt |
| gzip/xz/bzip2/zstd images | Working; XZ write, read-back verification and independent compare passed | Streaming decompressor subprocess |
| Native CLI and GUI on one backend | Working | Shared `LufusJob`, checksum module, and helper protocol |
| Read-back verification | Working; VM USB verification passed | Byte-for-byte verification, including compressed sources |
| Write progress and cancellation | Working; Rufus slot accounting verified for FAT32, NTFS, customization, and Windows To Go; cancellation during F3 passed with no orphan | Shared helper protocol maps operation-local one-decimal text onto overall bar fill; raw/file copy is committed every 32 MiB and WIM writes at least every second before progress is published |
| MD5/SHA-1/SHA-256/SHA-512 | Locally validated | Original Rufus hash core integrated into GUI/CLI; supersedes supplied GLib implementation |
| System-disk protection | Working; direct helper attack refused `/dev/vda` because it contained `/` | Mount-tree, sysfs, identity, and capacity revalidation |
| Rufus-shaped compact GTK UI | Working foundation | GTK 4/libadwaita |
| Content-based ISO classification | Working; real Ubuntu, Debian live, and Microsoft retail ISOs verified | Rufus-compatible bootmgr/WinPE, install-image, EFI, Syslinux/GRUB, and persistence capability report over ISO9660/Rock Ridge/UDF trees |
| GPT creation | Working for Windows UEFI modes in VM | Validated `sfdisk` layouts |
| FAT32 and NTFS formatting | Working for Windows modes in VM | Bundled dosfstools and ntfs-3g |
| Extracted ISO file-copy mode | Working for Windows installer in VM | Read-only ISO mount and bounded recursive copy |
| Windows ISO installation media | Working; full FAT32 VM install and UEFI:NTFS Secure Boot install verified | FAT32 with WIM/ESD splitting or intact NTFS payload |
| Windows 11 TPM/Secure Boot/RAM bypass | Working in VM | Boot/install verified with 3 GiB RAM, no TPM, Secure Boot off |
| Silent Disk 0 erase/install | Working; full unattended VM installation reached desktop | Shared GUI/CLI flag, 1-based WIM edition selection, ISO-language detection, Rufus disk-order guard, boot.wim injection, and three-part destructive acknowledgement |
| Local account and unattended privacy OOBE | Working in VM | Local desktop reached through shared answer-file generator |
| Disable automatic BitLocker | Working in VM | Installed OS reported Fully Decrypted, 0.0%, Protection Off, and no key protectors |
| Windows To Go | Working in Secure Boot VM | Direct NTFS wimlib apply + ESP + pinned BCD-SYS |
| Windows To Go internal-disk isolation | Working in VM | Offline SYSTEM-hive SAN policy; new internal sentinel verified Offline |
| UEFI:NTFS | Working in Secure Boot VM | Bundled signed loader image plus Microsoft Basic Data NTFS payload and 1 MiB loader partitions |
| Legacy BIOS Windows boot | Working in SeaBIOS VM | MBR/active NTFS layouts with Rufus-vendored GPL ms-sys MBR/PBR code; Windows 11 Setup reached |
| Linux persistence partition | Working; destructive synthetic Debian-live layout/config passed | GPT NTFS live payload + ext4 `casper-rw` or Debian `persistence` partition |
| Bad-block/fake-drive check | Working in VM | Bundled F3 destructive probe; GUI pre-write or standalone CLI, 1–4 passes |
| Runtime UEFI media validation | Working and default-on with GUI/CLI opt-out; 1,067-entry signed-loader/hash/manifest read-back passed | Explicit byte-based `Verifying media` phase uses clean target reads; bundled signed uefi-md5sum loaders and generated whole-media manifest |
| Raw/VHD/VHDX capture | Working in VM; all three byte-for-byte round trips passed | Direct raw capture and bundled qemu-img fixed VHD/dynamic VHDX |
| FFU capture | Windows-only upstream dependency | Rufus itself shells out to DISM; no public Microsoft FFU provider exists for Linux |
| UEFI Shell download | Working | Pinned official 26H1 URL and SHA-256 |
| Microsoft ISO download | Working when Microsoft permits the requesting network; live catalog and official URL resolution passed | Native C port of GPL Fido workflow; download goes directly from Microsoft; Sentinel rejection reports message code/session and the official-page fallback |
| Localization | Working | Rufus gettext catalogs compiled for 37 locales; translated Ubuntu GUI verified |

## Parity rule

Options that would silently do nothing are disabled in the GUI. Bad-block
testing and device capture are enabled and use the shared job/helper. Raw mode
preserves the source image's existing partition table, filesystem, volume
label, and boot configuration, so the corresponding format controls are shown
for layout familiarity but are disabled until their native backends land.
See `E2E.md` for exact runtime evidence and the physical-media limitation.
