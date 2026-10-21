/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "licenses.h"
#include <glib/gi18n.h>

/* These are translated summaries, not replacements for original licenses. */
static const LufusLegalComponent components[] = {
    {N_("Rufus — upstream project"), N_("Copyright © Pete Batard and Rufus contributors"), N_("GPL-3.0-or-later. Rufus is the original Windows application. Lufus supplies the native Linux foundation used by TuxFlash. Original source notices are retained. Source: https://github.com/pbatard/rufus"), LUFUS_LEGAL_UPSTREAM},
    {N_("BCD-SYS"), N_("Copyright © Joseph P. Zeller and BCD-SYS contributors"), N_("GPL-3.0-or-later. TuxFlash uses modified BCD scripts and registry templates. Source, patches and original notices accompany the application; the upstream AppImage is not included."), LUFUS_LEGAL_BUNDLED},
    {N_("EfiFs"), N_("Copyright © EfiFs and EDK II contributors"), N_("GPL-3.0-or-later at project level. EDK II and other portions retain their own notices, including BSD-2-Clause-Patent. Source reference: https://github.com/pbatard/efifs"), LUFUS_LEGAL_BUNDLED},
    {N_("Fido"), N_("Copyright © Pete Batard and Fido contributors"), N_("GPL-3.0-or-later. The native Microsoft retail-ISO discovery code follows the Fido workflow. Source reference: https://github.com/pbatard/Fido"), LUFUS_LEGAL_BUNDLED},
    {N_("ms-sys boot-record sources"), N_("Copyright © ms-sys contributors"), N_("GPL-2.0-or-later. Legacy BIOS boot-record code is inherited through Rufus. Its original source and license notices are retained."), LUFUS_LEGAL_BUNDLED},
    {N_("Noto Sans CJK"), N_("Copyright © Noto font authors and contributors"), N_("SIL Open Font License 1.1 (OFL-1.1). The portable package includes this font to display Chinese. Its original notice is included in runtime/share/doc/tuxflash/dependencies/fonts-noto-cjk.copyright."), LUFUS_LEGAL_BUNDLED},
    {N_("ntfs-3g UEFI driver"), N_("Copyright © ntfs-3g and UEFI port contributors"), N_("GPL-2.0-or-later, with LGPL-2.0-or-later library portions. This driver is part of UEFI:NTFS. See original notices and source references in THIRD_PARTY_NOTICES.md."), LUFUS_LEGAL_BUNDLED},
    {N_("UEFI:NTFS"), N_("Copyright © Pete Batard and UEFI:NTFS contributors"), N_("GPL-2.0-or-later. The boot loader and its components retain their original licenses and corresponding source references."), LUFUS_LEGAL_BUNDLED},
    {N_("uefi-md5sum"), N_("Copyright © Pete Batard and uefi-md5sum contributors"), N_("GPL-2.0-or-later. EFI media-verification loaders retain their original license notices. Source reference: https://github.com/pbatard/uefi-md5sum"), LUFUS_LEGAL_BUNDLED},
    {N_("GLib and GIO"), N_("Copyright © GLib contributors"), N_("Mainly LGPL-2.1-or-later, with separately licensed portions. The portable package bundles this dependency. Original notices and source-package identifiers are in runtime/share/doc/tuxflash. System installations retain the distribution notices."), LUFUS_LEGAL_SYSTEM},
    {N_("GTK"), N_("Copyright © GTK contributors"), N_("Mainly LGPL-2.1-or-later, with separately licensed portions. The portable package bundles this dependency. Original notices and source-package identifiers are in runtime/share/doc/tuxflash. System installations retain the distribution notices."), LUFUS_LEGAL_SYSTEM},
    {N_("JSON-GLib"), N_("Copyright © JSON-GLib contributors"), N_("LGPL-2.1-or-later. The portable package bundles this dependency. Original notices and source-package identifiers are in runtime/share/doc/tuxflash. System installations retain the distribution notices."), LUFUS_LEGAL_SYSTEM},
    {N_("libadwaita"), N_("Copyright © libadwaita contributors"), N_("LGPL-2.1-or-later, with separately licensed documentation and assets. The portable package bundles this dependency. Original notices and source-package identifiers are in runtime/share/doc/tuxflash. System installations retain the distribution notices."), LUFUS_LEGAL_SYSTEM},
    {N_("libsoup"), N_("Copyright © libsoup contributors"), N_("Mainly LGPL-2.1-or-later, with separately licensed portions. The portable package bundles this dependency. Original notices and source-package identifiers are in runtime/share/doc/tuxflash. System installations retain the distribution notices."), LUFUS_LEGAL_SYSTEM},
    {N_("Debian/Ubuntu disk and image utilities"), N_("Copyright © their respective upstream authors and contributors"), N_("Utilities such as util-linux, dosfstools, ntfs-3g, e2fsprogs, wimlib, hivex, QEMU, F3, 7-Zip and compression tools keep their own licenses. The portable package bundles selected utilities; other installations may use system packages. See runtime/share/doc/tuxflash/dependencies and dependencies.json for notices, versions and source-package identifiers."), LUFUS_LEGAL_SYSTEM},
};

const LufusLegalComponent *lufus_legal_components(size_t *count)
{
    if (count) *count=G_N_ELEMENTS(components);
    return components;
}

const char *lufus_project_license_notice(void)
{
    return N_("TuxFlash remains free and open-source software under GNU GPL version 3 or later (GPL-3.0-or-later). You may use, study, modify and redistribute it under that license. It is an independent Linux derivative of Rufus and Lufus, not an official Rufus release. Original authors keep their copyrights. Third-party components keep their own licenses; they are not relicensed as TuxFlash. The portable package includes application source in source/, the license in LICENSE.txt, and dependency notices in runtime/share/doc/tuxflash. Translated notices explain the licenses; the original license texts remain unchanged. See the licenses for their conditions and warranty disclaimers.");
}
