# End-to-end validation

All release compilation uses Podman. Runtime validation uses unprivileged
`qemu:///session` libvirt guests and USB-attached raw files, which exercise the
same Linux block-device, partition, filesystem, firmware, and installer paths
without risking a physical disk. A physical USB qualification pass is still a
separate release requirement.

## Verified on 2026-08-30 — Rufus-compatible ISO classification

Lufus 0.2.1 replaced the extension-only GUI decision with content-based image
capabilities derived from Rufus's `HAS_WINDOWS`, `HAS_WINTOGO`, and
`HAS_PERSISTENCE` rules. The scanner reads ISO9660/Rock Ridge media and the UDF
tree used by Microsoft retail images without mounting the image or requesting
privileges. GUI job selection now uses stable capability identifiers rather
than dropdown positions.

The scanner was exercised against the actual local release media:

| Image | Detected capabilities |
| --- | --- |
| Ubuntu 24.04.4 Desktop AMD64 | Linux, EFI, casper persistence; no Windows modes |
| Debian Live 13.6.0 GNOME AMD64 | Linux, EFI, Debian persistence; no Windows modes |
| Windows 11 25H2 English International x64 v2 | Windows installer, Windows To Go, BIOS and UEFI; no Linux persistence |

The Ubuntu and Debian images defaulted to lossless DD image mode, with
persistence available as an explicit secondary choice. The Windows image
retained the applicable Windows modes. The Ubuntu 24.04 Podman build passed all
eight test suites, including a generated real-ISO integration test and unit
cases for Windows, Ubuntu, Debian, generic ISO, WinPE, and ReactOS exclusions.

The exact `lufus_0.2.1_amd64.deb` package installed successfully on Ubuntu
24.04, Ubuntu 26.04, and Debian 13. Each guest resolved its native `7zip`
package, passed `dpkg -V lufus`, and reported `Lufus 0.2.1` from the installed
CLI. Its release SHA-256 is recorded in the companion `.deb.sha256` asset.

## Verified on 2026-08-29 — signed APT delivery

The central `openresearchtools/apt` catalogue indexed the stable Lufus v0.2.0
GitHub Release as `lufus 0.2.0` for AMD64. Its public `InRelease` metadata
verified with `gpgv` against the Open Research Tools archive key, and the
published package record contained the release asset's exact size, URL, and
SHA-256 digest.

The documented keyring-package installation flow was then exercised from a
clean Open Research Tools APT state in all three baseline guests:

| Guest | APT installation result |
| --- | --- |
| Ubuntu 24.04 | Keyring, signed index refresh, package download/install, and CLI version passed |
| Ubuntu 26.04 | Keyring, signed index refresh, package download/install, and CLI version passed |
| Debian 13 | Keyring, signed index refresh, package download/install, and CLI version passed |

Each guest selected version 0.2.0 at priority 500 from
`https://apt.openresearchtools.com`, installed the `amd64` package, and
reported `Lufus 0.2.0` from `/usr/bin/lufus --version`.

## Verified on 2026-08-29 — licensing and source-only BCD-SYS runtime

The Ubuntu 24.04 Podman release build passed all seven test suites, including
the ordered application-license registry. Rufus is the first upstream legal
section in the About window; bundled components and Debian/Ubuntu-provided
dependencies are presented separately. The package carries Debian-style
copyright, README.Debian, detailed third-party provenance, and corresponding
source archives.

The BCD-SYS AppImage and its private Perl/shared-library runtime were removed.
The package instead installs the pinned GPL source scripts and templates,
pre-generates the fixed BCD bases with a native libhivex importer, and uses a
native libhivex editor for the Windows To Go SAN policy. Semantic exports of
the generated BCD bases matched the former upstream importer. The final `.deb`
uses XZ control/data members; Debian 13 lintian reported no errors or warnings,
and the exact package installed successfully on Ubuntu 24.04, Ubuntu 26.04,
and Debian 13.

The exact packaged Ubuntu 24.04 executable recreated Windows 11 Pro Windows To
Go media on a 40 GiB USB-emulated disk with all applicable setup options and
internal-disk isolation enabled. Read-back confirmed the removable EFI loader,
Windows Boot Manager BCD, `winload.efi`, unattended setup file, and
`SanPolicy=4`. The same disk then passed Microsoft-key Secure Boot in the
dedicated VM and entered Windows first-boot `Getting ready`; the attached SATA
sentinel's allocation and modification time remained unchanged.

## Verified on 2026-08-28 — silent erase/install

The Rufus **⚠SILENTLY⚠ erase disk and install** workflow was implemented in
the shared Windows answer-file model and exercised from the Ubuntu 24.04-built
Debian package. The Podman release build completed all six test suites. Unit
coverage confirms that ordinary answer files never contain `WillWipeDisk`, a
silent answer requires a 1-based image index, and the GUI/CLI job sends the
same flag and index to the privileged helper.

- The exact packaged helper created a 12 GiB UEFI:NTFS virtual USB from the
  8,486,862,848-byte Windows 11 25H2 English International ISO. Before touching
  the target, it validated WIM image index 6 as Windows 11 Pro and detected its
  default language as `en-GB`.
- Read-back extraction from `sources/boot.wim` index 2 found
  `Autounattend.xml` with the Disk 1/partition 2 `LUFUS_BOOT` media guard,
  `WillWipeDisk=true` for Disk 0, EFI/MSR/Windows partition creation, image
  index 6, `International-Core-WinPE` and OOBE `en-GB` settings, the TPM/Secure
  Boot/RAM bypass, and local user `lufus`.
- A first boot with a deliberately unsupported VirtIO target proved the guard:
  Windows Setup could not establish the expected Disk 1 media layout, so
  `WillShowUI=OnError` displayed disk selection and did not overwrite the USB.
- The same media then booted with one native SATA target. It skipped language,
  edition, disk, network, privacy, and account questions, wiped and partitioned
  the fresh 80 GiB target, installed Windows 11 Pro, and reached the desktop as
  local user `lufus`. The virtual USB was detached at the first reboot, as the
  destructive-media warning requires.

The GUI requires all three Rufus-style irreversible-data-loss acknowledgements
before enabling `CREATE DANGEROUS MEDIA`. Interactive CLI use additionally
requires typing `ERASE WINDOWS DISK 0`; automation must explicitly combine
`--silent-install` with `--yes`.

## Verified on 2026-08-26

The release Debian package identified by its companion `.sha256` file was
installed on all supported baseline guests. Each guest received the same file
and its hash was compared with the release hash before installation:

| Guest | Result |
| --- | --- |
| Ubuntu 24.04 | Package dependencies/install/version passed; GTK GUI launched visibly |
| Ubuntu 26.04 | Package dependencies/install/version passed |
| Debian 13 | Package dependencies/install/version passed |

The release candidate passed these destructive media and safety gates:

- Windows 11 25H2 UEFI:NTFS: the CLI created a GPT disk with an intact NTFS
  payload and 1 MiB signed UEFI:NTFS loader. Loader read-back SHA-256 matched
  `res/uefi/uefi-ntfs.img`. A new VM booted it with Microsoft-enrolled Secure
  Boot keys and TPM 2.0, installed Windows 11 Pro to a separate 80 GiB target,
  detached the USB, and independently reached the desktop as local user
  `lufus`. OOBE did not request a Microsoft account or privacy questions.
  Inside Windows, `Confirm-SecureBootUEFI` returned `True`, TPM was present,
  ready, enabled, activated, and owned, while `manage-bde -status` reported the
  OS volume Fully Decrypted at 0.0%, Protection Off, and no key protectors.
- Windows 11 25H2 legacy BIOS: the CLI produced an MBR disk with an active NTFS
  partition, Rufus-vendored GPL ms-sys MBR/PBR code, and complete source files.
  A newly registered SeaBIOS VM booted that device and reached Windows Setup.
- Ubuntu 24.04.4 desktop ISO: raw write to a 16 GiB USB-emulated disk,
  byte-for-byte verification, Secure Boot boot, full installation, USB detach,
  and independent installed-system boot.
- Windows 11 25H2 English International: installer media creation, signed UEFI
  boot, WIM split discovery, Windows 11 Pro full installation, and independent
  target boot. The VM intentionally had 3 GiB RAM, no TPM, and Secure Boot off;
  the requirements bypass, local account, offline account path, and unattended
  OOBE reached the desktop.
- Windows To Go: Windows 11 Pro applied directly to a 40 GiB USB-emulated disk,
  BCD provisioned, signed boot completed with Secure Boot and TPM enabled, OOBE
  reached the desktop, and a newly discovered 13 GiB SATA sentinel with serial
  `LUFUS-SAN-20260826` was reported `Offline` while the USB system disk remained
  `Online`. QEMU recorded zero sentinel writes.
- Persistent Debian live media: a synthetic bootable ISO produced GPT NTFS live
  data, a 256 MiB ext4 partition labelled `persistence`, `/persistence.conf`
  containing `/ union`, and a kernel command line containing `persistence`.
  Runtime validation installed the signed loader, preserved the original EFI
  executable, generated the media manifest, and all hashes matched on read-back.
- Raw/VHD/VHDX capture: a 256 MiB USB-emulated source was captured in all three
  formats. Both virtual formats converted back to raw and matched the source.
  VHDX-to-device writing with the optional qemu-img comparison also passed.
- Compressed writing: a 32 MiB XZ image passed built-in read-back verification
  and an independent decompression/byte comparison.
- Fake-drive testing: bundled F3 reported the complete real capacity of a
  256 MiB USB-emulated device. Cancelling while F3 was active terminated the
  child, exited the job, and left no orphan process.
- System-disk safety: a direct helper invocation with the correct identity and
  capacity for `/dev/vda` was rejected because the target contained `/`.
- Localization: the packaged GTK GUI was launched visibly under Ubuntu 24.04
  with one of the bundled Rufus gettext catalogs applied.
- Microsoft retail-ISO discovery: the native client retrieved the live Windows
  11 language catalog and resolved English International x64 to a temporary
  official `software.download.prss.microsoft.com` ISO URL.
- Password-free removable-drive authorization: an active Ubuntu 24.04 desktop
  user invoked the installed helper through polkit with no password prompt,
  destructively checked a 256 MiB USB-emulated drive with F3, and received the
  successful real-capacity result. No helper or F3 process remained afterward.

FFU is intentionally not included in this gate. Rufus delegates desktop FFU
capture/apply to proprietary Microsoft DISM/FFU-provider components that are
not available on Linux; Lufus rejects `.ffu` rather than corrupting it by raw
copying container bytes. Physical removable-media electrical/controller
qualification also remains distinct from this QEMU block-device gate.

Large VM disks, screenshots, and ISO files live under `linux/e2e-state` or
outside the repository and are ignored by Git. The reusable destructive writer
gate is `linux/tests/qemu-raw-write.sh`; it requires a running QEMU guest with
the guest agent, the installed Lufus package and a source image, and refuses to
replace an existing test disk.

Example:

```sh
LUFUS_WRITER_VM=gnozzard-test-ubuntu2604 \
LUFUS_GUEST_IMAGE=/tmp/ubuntu.iso \
bash linux/tests/qemu-raw-write.sh
```

## Verified on 2026-08-27

The combined progress, Microsoft-download, and application-icon release was
built once in the Ubuntu 24.04 Podman builder. The resulting package was
`lufus_0.2.0_amd64.deb`, SHA-256
`64f0a1ed279acb56d80e20b9bf7535fda4e7ea55e518af67e8c97e4174b1e10e`.
That exact file installed over the previous build on Ubuntu 24.04, Ubuntu
26.04, and Debian 13; its hash, package version, dynamic libraries, desktop
metadata, AppStream metadata, and all nine hicolor icon sizes were checked.

- The installed CLI wrote and verified a fresh 48 MiB random image on a
  USB-emulated disk in all three guests. Each read-back matched byte-for-byte.
  Overall progress was monotonic across the write (`0-45%`), flush
  (`45-50%`), and verification (`50-100%`) stages.
- The installed Debian 13 GTK application performed the same write from the
  logged-in desktop session. A throttled QEMU target made the staged UI
  observable: the bar advanced, displayed the current `Flushing data to the
  device` stage at 93%, animated while the flush had no measurable total, and
  finished at `READY`/100%. The retained active-session polkit authorization
  required no repeated password prompt.
- The installed Ubuntu 24.04 CLI created Windows 11 25H2 UEFI installer media
  from the real 8,486,862,848-byte ISO using the requirements, offline-account,
  privacy, BitLocker, local-account, internal-drive, and runtime-validation
  options together. Its Rufus-compatible progress protocol produced 146
  continuous ISO-copy updates over the overall 9-53.5% allocation, kept WIM
  splitting inside `Copying ISO files`, emitted the six upstream customization
  milestones over 53.5-98%, then displayed `Finalizing, please wait...` at 99%
  before `READY`. There were no `Synchronizing Windows` events. Read-back
  inspection found a 12 GiB GPT/FAT32 disk, three split WIM parts,
  `Autounattend.xml` in boot.wim index 2, the compatibility bypass and backup,
  signed runtime-validation loader/original pair, and a 1,067-entry manifest.
- The installed Ubuntu 24.04 GTK application repeated the same real-ISO write.
  At `Copying ISO files: 58.0%`, the bar was filled to the corresponding overall
  position (about 35%) rather than displaying a fabricated global percentage;
  it completed at `READY`/100%. Final filesystem flushing used target-scoped
  `syncfs()` rather than the former process-wide `sync()`.
- The Microsoft Windows 11 x64 request was exercised from the installed CLI in
  all three guests. Catalog discovery succeeded; Microsoft then rejected
  temporary-link issuance for the current public network. Each client stopped
  immediately and reported message code `715-123130`, its request session, and
  the official Windows 11 download URL. The GTK flow displayed the same
  persistent error with a `Microsoft Website` action, and that action opened
  Microsoft's real Windows 11 download page in the VM browser.
- The selected USB-C `LUFUS` icon was installed at 16, 22, 24, 32, 48, 64,
  128, 256, and 512 pixels. Package validation rejected the inherited Rufus
  icon and confirmed that no inherited Rufus icon was shipped.

## Verified on 2026-08-27 — durable progress and optional validation

The final follow-up package was again built once in the Ubuntu 24.04 Podman
builder. Its SHA-256 is recorded beside `lufus_0.2.0_amd64.deb` in the release
artifact's `.sha256` companion file. That exact file and hash were installed
successfully on Ubuntu 24.04, Ubuntu 26.04, and Debian 13. The Podman build
completed all six test suites, including new durable-copy, validation-layout,
validation-byte-count, default-policy, and helper-argument coverage.

- All three installed guests wrote a fresh 32 MiB random raw image to their
  USB-emulated test disk, completed Lufus read-back verification, and passed an
  independent byte comparison. Raw and extracted-file copy progress is now
  published only after a target commit, at intervals of at most 32 MiB. WIM
  split/apply is target-committed at least once per second and before each
  reported percentage; the final 100% is withheld until wimlib exits and its
  final commit succeeds.
- Ubuntu 24.04 created FAT32 Windows 11 installer media from the real
  8,486,862,848-byte ISO with all setup customizations and runtime validation
  left at its new default. Recorded durations were 48.849 s for durable ISO
  copy, 0.401 s for customization, 16.291 s for the separately labelled
  `Verifying media` pass, and 0.078 s for finalization. Verification occupied
  overall progress 90-99%; finalization owned only 99-100%.
- Read-back found the expected GPT/FAT32 media and three split WIM files. The
  signed validation loader matched the packaged loader byte-for-byte, the
  original EFI loader was preserved, and all 1,067 generated manifest entries
  passed a second independent `md5sum` scan.
- Repeating the same real-ISO job with `--no-runtime-validation` produced no
  verification stage, manifest, replacement loader, or preserved-loader file.
  Durable copy took 52.371 s, customization 0.598 s, and the final target
  commit/unmount phase 0.728 s. No process-wide `sync()` path remains.
