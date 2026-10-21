# Lufus licensing, third-party notices, and provenance

Lufus is an independent modified Linux fork of Rufus. The application code is
GPL-3.0-or-later; inherited files retain their original copyright and license
notices. Rufus is deliberately listed first here and in the application's
About window.

This document distinguishes three cases that Debian packages treat
differently: the upstream project from which Lufus is forked, components
included in the Lufus package, and separate packages installed by `apt`.

## Ordered license index

| Order | Component | How Lufus uses it | License summary |
|---:|---|---|---|
| 1 | **Rufus** | Upstream project and inherited source | GPL-3.0-or-later |
| 2 | BCD-SYS | Pinned source scripts and templates | GPL-3.0-or-later |
| 3 | EfiFs | Source corresponding to EFI filesystem drivers | GPL-3.0-or-later at project level; retained EDK II/third-party notices include BSD-2-Clause-Patent |
| 4 | Fido | Workflow used by the native retail-ISO discovery implementation | GPL-3.0-or-later |
| 5 | ms-sys | Legacy boot-record sources inherited through Rufus | GPL-2.0-or-later |
| 6 | ntfs-3g UEFI | Filesystem driver within the UEFI:NTFS loader | GPL-2.0-or-later; library portions LGPL-2.0-or-later |
| 7 | UEFI:NTFS | Pinned removable-media loader image | GPL-2.0-or-later, plus its constituent component notices |
| 8 | uefi-md5sum | Pinned media-validation EFI loaders | GPL-2.0-or-later |
| 9 | Debian/Ubuntu dependencies | Separately installed libraries and tools | Each distribution package retains its own license |

## 1. Rufus — upstream project

Rufus is developed by Pete Batard and contributors at
<https://github.com/pbatard/rufus>. Lufus is a modified fork, not an official
Rufus release. Rufus and Lufus are distributed under GPL-3.0-or-later. The
original Rufus source and copyright history remain in the repository, and the
Debian package includes the exact Lufus source used for its build under
`/usr/share/lufus/sources`.

## 2. BCD-SYS

BCD-SYS revision `9f8589cdb095acb06d7fa91c54d27af02a95de90` by Joseph P.
Zeller and contributors is GPL-3.0-or-later. Lufus installs the pinned shell
scripts and templates directly. It changes the UUID generator from Python to
the distribution's `uuidgen`, and invokes the scripts from Lufus's already
authorized root helper.

The upstream AppImage is not shipped. Consequently, Lufus does not duplicate
the AppImage's Perl interpreter or private shared-library tree. Required tools
such as `hivexsh`, `fatattr`, `setfattr`, and `xxd` are declared as
normal Debian package dependencies. Because current Debian/Ubuntu releases do
not provide BCD-SYS's old Perl `hivexregedit` frontend, Lufus builds a small
native GPL registry-template importer in Podman. It generates two base BCD
hives from the pinned `.reg` files. The generated hives are validated with
`hivexsh`, and their semantic exports were compared with the upstream importer.
The build importer itself is not needed at runtime. A small native GPL
libhivex tool applies the Windows To Go SAN policy to the offline SYSTEM hive,
replacing the AppImage's Perl frontend for that runtime operation. The exact
modified scripts, source templates, generated bases, and
upstream license are installed under `/usr/share/lufus/sources`.

## 3. EfiFs

EfiFs revision `bfc6ae29ac15516bddb97928eaf1eda187a6d242` provides source
corresponding to filesystem drivers used by the loader image. The project is
GPL-3.0-or-later. Its source tree also contains EDK II and other portions with
their own retained notices, including BSD-2-Clause-Patent. The complete pinned
source archive is shipped rather than reducing this mixed tree to a misleading
single-license claim.

## 4. Fido retail-ISO discovery

The Microsoft retail-ISO request sequence is a native C implementation of the
Fido workflow by Pete Batard, revision
`3d47260b8915385c58e20c73e24b36e9a9536f3f` (GPL-3.0-or-later). The exact
Fido source archive and license are installed under `/usr/share/lufus/sources`.
ISOs are downloaded directly from temporary URLs issued and hosted by
Microsoft; neither Lufus nor the package redistributes Windows images.

## 5. ms-sys boot records

The legacy Windows MBR and NTFS PBR sources are inherited from Rufus's vendored
ms-sys code and are GPL-2.0-or-later. The exact headers used by the Linux build
are present in the bundled Lufus source archive.

## 6–7. ntfs-3g UEFI and UEFI:NTFS

`uefi-ntfs.img` is copied byte-for-byte from Rufus commit
`2350489fe902bdc71d37f255a7684fe06a0ab2d0` and has SHA-256
`72683fa1250eeea772d3399277b434d4e55ba8dd0dc926e52d817e701fc2eb9e`.
It contains UEFI:NTFS 2.8 (GPL-2.0-or-later), the ntfs-3g UEFI 1.9 driver
(GPL-2.0-or-later, with LGPL-2.0-or-later library portions), and EfiFs-derived
components described above. Matching source archives and upstream license
files are installed under `/usr/share/lufus/sources`.

## 8. uefi-md5sum

The uefi-md5sum 1.2 loaders and source are GPL-2.0-or-later. The prior Lufus
notice incorrectly described these as GPL-3.0-or-later; the pinned upstream
`LICENSE` and source headers specify GPL version 2 or later. The corrected
notice is reflected in the About window, package copyright, and this document.

## 9. Debian/Ubuntu runtime dependencies

GTK, libadwaita, GLib/GIO, libsoup, JSON-GLib, polkit and the command-line
disk/image utilities are dynamically linked or executed as separate
distribution packages. They are not bundled into Lufus and are not relicensed
as part of Lufus.

The principal library licenses are LGPL-2.1-or-later for GTK, libadwaita,
GLib/GIO, libsoup, and JSON-GLib, with additional separately licensed files in
some upstream trees. Utilities including util-linux, dosfstools, ntfs-3g,
e2fsprogs, wimlib, hivex, QEMU, F3, xorriso, 7-Zip, wget, and compression
tools use their respective GPL, LGPL, BSD, public-domain, and other licenses.
The authoritative per-file notices are installed by `apt` at
`/usr/share/doc/<package>/copyright`; Lufus does not replace those records with
an incomplete aggregate license.

## Microsoft signatures and excluded Microsoft software

The package contains no Windows ISO, Windows kernel component, DISM, or Windows
ADK binary. EFI executables inside the pinned UEFI:NTFS and uefi-md5sum
artifacts carry third-party Secure Boot signatures and are redistributed
byte-for-byte. A signature is trust metadata, not a transfer of copyright or a
new software license, and does not make Lufus a Microsoft product.
