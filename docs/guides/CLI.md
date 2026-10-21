# Interface e linha de comando / GUI and CLI

Native C/GTK 4/libadwaita application and CLI, based on
[Lufus](https://github.com/openresearchtools/lufus) and
[Rufus](https://github.com/pbatard/rufus). Current development credit:
[Ricardo Barreto](https://github.com/ricardogomesbarreto) and contributors.

The current feature comparison, safety changes, requirements and limits are in
[English](../../README.md) and [Português (Brasil)](../../README.md).
[Review 0.3.3](../validation/REVISAO-0.3.3.md) records this update's evidence separately from
the historical upstream reports in E2E.md and PARITY.md.

## Run and inspect

From the project root:

```sh
./tuxflash.sh
./tuxflash.sh --version
./tuxflash.sh doctor
./tuxflash.sh list
./tuxflash.sh probe --image /path/image.iso
./tuxflash.sh checksum --image /path/image.iso
./tuxflash.sh write --help
./tuxflash.sh capture --help
./tuxflash.sh test-device --help
```

The canonical launcher delegates to `dist/portable/tuxflash` when available,
otherwise `linux/runtime/bin/tuxflash`. Compatibility aliases named `lufus`
remain for existing scripts. Portable launchers use their private ELF loader;
start through the launcher, not the hidden ELF directly.

## Write and capture

Both frontends submit the same job to the independently validating helper.
These examples are destructive where marked. Replace the placeholder with the
intended whole USB disk; do not copy an arbitrary `/dev/sdX` from a tutorial.

```sh
# DESTRUCTIVE: raw or compressed image and read-back verification.
./tuxflash.sh write --mode raw --image /path/image.iso \
  --device /dev/disk/by-id/usb-CHOOSE_TARGET --verify

# DESTRUCTIVE: UEFI Windows installer; FAT32 splitting is handled by the backend.
./tuxflash.sh write --mode windows-installer --image /path/windows.iso \
  --device /dev/disk/by-id/usb-CHOOSE_TARGET

# Read the device into a new image file; the existing output must not be overwritten.
./tuxflash.sh capture --device /dev/disk/by-id/usb-CHOOSE_TARGET \
  --output "$PWD/backup.vhdx" --format vhdx
```

The CLI requires exact target-path confirmation. Automation requires explicit
`--yes`; redirected input never implies consent. Silent-install additionally
requires `ERASE WINDOWS DISK 0` and can erase the PC's disk when the USB boots.
F3 checks are destructive. DD preserves image partition tables and filesystems;
formatting controls are therefore disabled for raw images.

Extracted Windows installer and Linux-persistent modes use runtime validation
by default; `--no-runtime-validation` is an explicit opt-out. RAW uses `--verify`.
Target cache is invalidated before raw read-back. Validation success does not
establish that an arbitrary ISO will boot on an arbitrary firmware.

## Privileges and target checks

Only the operation helper is elevated. Both current and legacy policy sources
require active-session administrative authentication and deny inactive/remote
sessions. The portable launcher uses the host's generic `pkexec` authorization;
installing a menu shortcut does not install a system policy or grant access.

Helper checks cover whole-disk selection, exact size, actual opened-device
identity, kernel disk sequence when available, read-only state, protected system
mounts and active swap. Checks repeat before external tools; raw descriptors
are checked before writing and read-back. Read-only media can be verified/captured.
Unresolved active-swap backing or inspection failures cause refusal.

LVM/RAID/device-mapper slave traversal and multiple-parent device discovery
strengthen system-volume protection. Kernel disk sequences are unavailable on
some older kernels; remaining checks cannot identify every same-number reconnect.
External programs reopen paths, so this is not a claim of complete race freedom.
Other namespaces, complex storage and untested kernels require qualification.

## Build and repeat validation

```sh
bash linux/build-local.sh
make check
make check-sanitize
```

The native build uses Meson, Ninja, pkg-config, a C compiler, GTK >= 4.14,
libadwaita >= 1.5, GLib/GIO, libsoup >= 3.4, JSON-GLib >= 1.8, hivex and gettext.
Media tools include filesystem utilities, 7-Zip, wimlib, QEMU image tools, F3
and decompressors. They are bundled for the portable x86-64 build.

The isolated [VM harness](../development/VM-TESTING.md) attaches only disposable regular
files, with no host disk passthrough. It tests helper behavior and minimal
firmware fixtures; it does not qualify a full Windows/Linux desktop.

For packaging, `tools/build-portable.py` consumes trusted native runtime files,
collects ELF dependencies, includes a private loader, fonts, locales and component
notices, and copies the corresponding source. It does not copy host application
icons or authentication executables. Build separately for each architecture.
No official TuxFlash APT repository or ARM64 release is claimed here; inherited
Lufus distribution instructions apply to that upstream project.

## Limits and licensing

No Wine or FUSE is needed for the portable build. Host requirements still include
`/bin/sh`, `/proc`, `/sys`, compatible kernel, device access, X11/Wayland for the
GUI and host Polkit/pkexec for GUI operations. FFU remains unsupported. Windows
To Go through a real desktop, Secure Boot and persistence across real reboots
remain incompletely qualified. See [compatibility](../validation/COMPATIBILIDADE.md).

GPL-3.0-or-later; original authors, component terms and source are preserved.
See [notices](../legal/THIRD_PARTY_NOTICES.md), [license](../../LICENSE.txt) and
[release notes](../releases/NOTAS-DA-VERSAO.md). The application credits include direct links
to the Lufus repository and Ricardo Barreto's public GitHub profile.
