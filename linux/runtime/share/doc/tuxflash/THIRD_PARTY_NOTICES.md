# TuxFlash: open-source license and component notices

TuxFlash application code remains GPL-3.0-or-later. Ricardo Barreto is credited
for development of this version; Pete Batard/Rufus contributors and Open
Research Tools/Lufus contributors retain their attributions. This is an
independent Linux derivative, not an official Rufus release.

The original license text is in LICENSE.txt (project root); docs/legal/UPSTREAM-LICENSE.txt
is retained. The license grants and conditions are not changed by translated UI
summaries. Read the original terms, including their warranty disclaimers.
GNU reference: https://www.gnu.org/licenses/gpl-3.0.html.

## Current package layout

The portable archive includes this application's corresponding source under
source/: native C frontend/helper, Meson files, tests, catalogs and the Rufus
source used by the Linux build. Modified prepared BCD scripts are included under
source/linux/runtime/libexec/bcd-sys; the supplied BCD source is under
source/linux/third_party/bcd-sys. In a source checkout these paths have no source/ prefix.

Bundled dependencies remain separately licensed. Their original distribution
notices are in runtime/share/doc/tuxflash/dependencies, with versions and
source-package identifiers in runtime/share/doc/tuxflash/dependencies.json.
Additional component notices remain in runtime/share/doc. These identifiers
and upstream references do not mean every dependency source archive is included
in this package. The UI does not claim that all components are GPL-3.0-or-later.

## Component index

| Component | License summary / reference |
| --- | --- |
| Rufus, supplied Lufus foundation and TuxFlash application code | GPL-3.0-or-later; inherited file notices remain authoritative |
| BCD-SYS | GPL-3.0-or-later; supplied source, modified scripts and notices retained |
| EfiFs | GPL-3.0-or-later at project level; EDK II/other portions retain notices including BSD-2-Clause-Patent; https://github.com/pbatard/efifs |
| Fido workflow | GPL-3.0-or-later; https://github.com/pbatard/Fido |
| ms-sys boot records | GPL-2.0-or-later; inherited source under source/upstream/rufus/src/ms-sys |
| UEFI:NTFS | GPL-2.0-or-later, with component-specific notices; https://github.com/pbatard/uefi-ntfs |
| ntfs-3g UEFI driver | GPL-2.0-or-later and LGPL-2.0-or-later portions; constituent of the UEFI:NTFS resource |
| uefi-md5sum | GPL-2.0-or-later; https://github.com/pbatard/uefi-md5sum |
| GTK, GLib/GIO, JSON-GLib, libadwaita and libsoup | Principally LGPL-2.1-or-later; separately licensed portions retain original notices |
| GNOME translations | Original translation headers, project licenses and translator attributions retained; linux/po/toolkit/SOURCES.json records origin and hashes |
| Noto Sans CJK Regular | SIL Open Font License 1.1 (OFL-1.1); original notice in dependencies/fonts-noto-cjk.copyright |
| util-linux, dosfstools, ntfs-3g, e2fsprogs, wimlib, hivex, QEMU, F3, 7-Zip and compression tools | Their own GPL, LGPL, BSD and other licenses; see the individual bundled distribution notices and source-package identifiers |

System installations may use distribution-provided dependencies; the portable
package includes selected tools and private shared libraries. Neither packaging
mode transfers ownership or replaces the component licenses. No authentication
program (pkexec, sudo or su) is redistributed in the portable runtime.

The supplied older Lufus packaging description is preserved separately in
LUFUS-NOTICES-HISTORICO.md. Its statements about complete third-party source
archives or /usr/share/lufus/sources do not describe this portable archive.

Developer profile: https://github.com/ricardogomesbarreto.
