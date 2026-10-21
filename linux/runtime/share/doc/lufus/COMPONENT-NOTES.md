# Lufus for Linux

Lufus is a compiled C/GTK 4/libadwaita port that keeps the compact Rufus
workflow while replacing Win32 device access with a privilege-separated Linux
backend.

## Current foundation

- Compiled native `lufus` GTK application (no Python runtime).
- Compiled `lufus-helper` writer, elevated only for the destructive operation.
- Removable/hotplug whole-disk discovery with an opt-in fixed-disk view.
- Independent helper-side checks for device identity, capacity, whole-disk
  selection, read-only state, and protected system mounts.
- Raw `.iso`, `.img`, `.raw`, and `.dd` writing, plus VHD/VHDX conversion to
  the physical target instead of incorrectly copying the container bytes.
- Streaming `.gz`, `.xz`, `.bz2`, and `.zst` decompression.
- Durable target-commit progress, cancellation, log output, and
  MD5/SHA-1/SHA-256/SHA-512.
- A real CLI that uses the same job model and privileged helper as the GUI.
- GPT/FAT32 Windows installer media with install-image splitting, plus an
  NTFS payload mode using the signed UEFI:NTFS removable-media loader.
- Windows setup options for TPM/Secure Boot/RAM bypass, local accounts,
  online-account removal, privacy questions, BitLocker, internal disks, and
  Rufus-compatible silent Disk 0 erase/install media with edition selection.
- Windows To Go provisioning with direct-to-NTFS wimlib application and a
  separate UEFI System Partition with generated Windows BCD stores.
- Legacy BIOS and combined BIOS+UEFI Windows media using GPL ms-sys boot code.
- Ubuntu/Debian live-media persistence, default-on signed runtime UEFI MD5
  validation with an explicit opt-out, and destructive F3
  fake-capacity/bad-block testing.
- Device capture to raw, fixed VHD, or dynamic VHDX from both GUI and CLI.
- 37 gettext catalogs, native official Microsoft retail-ISO discovery/download,
  and pinned, checksum-verified UEFI Shell retrieval.

Raw image mode preserves the image's partition table, filesystem, and boot
configuration. Its format controls stay disabled. Selecting an ISO enables the
Windows installer and Windows To Go job types and their setup options.

## Command line

The GUI and CLI only collect options. Both submit the same `LufusJob` to the
same helper, so safety checks and media creation logic are not duplicated.

```sh
# Discover eligible removable devices
lufus list

# Check an image
lufus checksum --image image.iso

# Raw or compressed image, with read-back verification
lufus write --mode raw --image image.img.xz \
  --device /dev/disk/by-id/usb-... --verify

# Windows 11 installer with Rufus-style setup choices
lufus write --mode windows-installer --image windows.iso \
  --device /dev/disk/by-id/usb-... \
  --bypass-windows-requirements --no-online-account

# Unattended UEFI disk installation media. When booted, this deliberately
# erases Windows Setup Disk 0 and installs WIM/ESD image index 6 without asking.
# Lufus reads the selected image language and supplies it to WinPE and OOBE.
# Disconnect every other disk from the target PC before using this.
lufus write --mode windows-installer --silent-install --windows-index 6 \
  --image windows.iso --device /dev/disk/by-id/usb-...

# Keep install.wim intact on NTFS and boot it through signed UEFI:NTFS
lufus write --mode windows-installer --uefi-ntfs --image windows.iso \
  --device /dev/disk/by-id/usb-... \
  --bypass-windows-requirements --local-account lufus

# Windows To Go; choose the 1-based edition shown by the ISO
lufus write --mode windows-to-go --windows-index 1 --image windows.iso \
  --device /dev/disk/by-id/usb-... --offline-internal-drives

# BIOS+UEFI Windows installer (signed runtime validation is on by default)
lufus write --mode windows-installer --target-system bios-uefi --uefi-ntfs \
  --image windows.iso --device /dev/disk/by-id/usb-...

# Skip the separate post-write validation pass when speed matters more
lufus write --mode windows-installer --no-runtime-validation \
  --image windows.iso --device /dev/disk/by-id/usb-...

# Persistent Ubuntu/Debian live media (size is MiB)
lufus write --mode linux-persistent --persistence-size 4096 \
  --image debian-live.iso --device /dev/disk/by-id/usb-...

# Test real capacity/blocks independently, or use --test-passes before a write
lufus test-device --device /dev/disk/by-id/usb-... --passes 1

# Capture a device. The GUI provides the same operation through SAVE.
lufus capture --device /dev/disk/by-id/usb-... \
  --output "$PWD/backup.vhdx" --format vhdx

# Download the pinned UEFI Shell release and verify its SHA-256
lufus download --kind uefi-shell --output "$PWD/UEFI-Shell.iso"

# Discover Microsoft languages, resolve a temporary official URL, or download
lufus download --kind windows --release windows11 --list-languages
lufus download --kind windows --release windows11 \
  --language "English International" --architecture x64 \
  --output "$PWD/Windows11.iso"
```

For supported extracted-media modes, validation is enabled by default in both
frontends. Media creation occupies overall progress 0-90%, `Verifying media`
reports target bytes from 90-99%, and only the final target commit/unmount uses
99-100%. The GUI checkbox or CLI `--no-runtime-validation` removes the
verification pass. Copy progress itself is not page-cache progress: Lufus
commits regular/raw writes every 32 MiB and commits external WIM writes at
least once per second before publishing their percentage.

Microsoft controls access to temporary retail-ISO links. If its Sentinel
service rejects the network, Lufus stops instead of repeatedly issuing requests
that may prolong the restriction. The error reports Microsoft's message code
`715-123130`, the request session, and the official Microsoft download page.
This restriction cannot be bypassed by changing the ISO writer.

Interactive CLI writes require typing the exact target path. Automated writes
must add `--yes`; this is intentionally not implied by redirected input.
Silent-install media adds a second interactive `ERASE WINDOWS DISK 0`
acknowledgement. In the GUI, all three Rufus-style data-loss acknowledgements
must be checked before the destructive confirmation button is enabled.

## Deliberate platform limits

Lufus does not claim that a Linux substitute is equivalent where Rufus itself
delegates to proprietary Windows components. Desktop-compatible FFU capture
and apply use Microsoft's private FFU provider through DISM, which Microsoft
documents as requiring Windows or WinPE. Lufus therefore refuses to label raw
container copying as FFU support and does not redistribute Windows or ADK
binaries. Microsoft retail-ISO discovery is implemented natively from the GPL
Fido workflow; the ISO itself is downloaded directly from Microsoft.

## Release and reproducible builds

The signed Open Research Tools APT repository tracks stable Lufus GitHub
Releases. Install its keyring package, refresh the index, and install Lufus:

```sh
wget -qO /tmp/keyring.deb https://keyring.openresearchtools.com && \
  sudo apt install -y /tmp/keyring.deb && \
  sudo apt update && \
  sudo apt install -y lufus
```

GitHub Releases compile and test native AMD64 and ARM64 packages directly on
GitHub's disposable `ubuntu-24.04` and `ubuntu-24.04-arm` runners so the results
use the oldest supported GNU/Linux ABI.
Pushing a tag matching the Meson/Debian version, for example `v0.2.2`, publishes
both Debian packages and their checksum assets after both builds pass:

```text
lufus_0.2.2_amd64.deb
lufus_0.2.2_amd64.deb.sha256
lufus_0.2.2_arm64.deb
lufus_0.2.2_arm64.deb.sha256
```

The same build remains reproducible locally through the Ubuntu 24.04 Podman
image:

```sh
bash linux/build-deb.sh
```

The local build produces `linux/dist/lufus_0.2.2_<architecture>.deb` and its
checksum, where `<architecture>` is `amd64` or `arm64`, matching the build host.
It is compiled on Ubuntu 24.04 and declares dependency floors available on
Ubuntu 24.04, Ubuntu 26.04, and Debian 13. `build-container.sh` remains available
for a compile-only development check. The previous AMD64 0.2.1 package was
installed and tested on all three baseline guests; see `E2E.md` for evidence.

## Runtime dependencies during development

The Debian package dynamically uses the target distribution's GTK,
libadwaita, GLib, libsoup, JSON-GLib, filesystem tools, 7-Zip, wimlib, hivex,
QEMU, F3, and compression packages. Pinned boot components and the separately
licensed BCD-SYS source scripts are installed privately with their notices and
corresponding source. Their command dependencies come from Debian/Ubuntu
packages; the upstream BCD-SYS AppImage and its duplicate runtime are not
shipped.

Rufus is listed first in the application About window's upstream credits and
component license list. Debian-style package attribution is installed in
`/usr/share/doc/lufus/copyright`, with explanatory notes in
`/usr/share/doc/lufus/README.Debian` and complete component provenance in
`/usr/share/doc/lufus/THIRD_PARTY_NOTICES.md`. Runtime packages retain their
own authoritative notices at `/usr/share/doc/<package>/copyright`.

## Privileged operations

Only `/usr/libexec/lufus-helper` runs as root. The installed polkit policy lets
an active local desktop session invoke that exact executable without repeatedly
entering a password. Inactive, remote, and sessionless callers are denied.
Before opening a target the helper independently checks that it is a whole
device, unchanged since selection, writable, the expected capacity, and does
not contain protected system mounts.

## Important safety note

Writing an image destroys all data on the selected device. Lufus intentionally
does not display any disk that contains `/`, `/boot`, `/boot/efi`, `/usr`,
`/var`, or `/home`, and the elevated helper repeats that check immediately
before opening the target for writing.

The **⚠SILENTLY⚠ erase disk and install** option is intentionally more
dangerous than ordinary image writing: the resulting USB asks Windows Setup to
wipe Disk 0 when the USB is later booted. Its answer file first identifies the
known Lufus partition on Disk 1 and uses `WillShowUI=OnError`, matching Rufus'
guard against ambiguous disk ordering. Nevertheless, every non-target disk
must be physically disconnected from the installation PC.
