/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "release_notes.h"
#include <glib/gi18n.h>

char *tuxflash_release_notes(void)
{
    GString *xml=g_string_new(NULL);
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Updated in 0.3.4")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Fixed persistent-media creation for Zorin OS images with internal directory links. Internal aliases are copied as files and directories; links outside the ISO tree and cycles are rejected.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("The ISO tree is checked before persistent-media partitions are created. Capacity checks include the expanded aliases.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Added real-image tests for SteamOS recovery, Kali Installer and Zorin OS. The validation report distinguishes image writing, firmware boot stages and remaining limits.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Updated in 0.3.3")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Stronger system-disk and active-swap protection, exact capacity checks and device sequence validation when supported by the kernel.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("The helper rechecks the target before external tools run. The interface reports success only after the helper exits successfully and its output has been read.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Explicit Lufus repository credit, a native Linux description, safer legacy Polkit defaults and a portable package without unrelated application icons.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Updated in 0.3.2")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Release notes now have their own page, separate from Legal.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Application and component license summaries are translated into Brazilian Portuguese, English, Simplified Chinese and Spanish.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Legal notices now describe the portable dependencies and the Noto Sans CJK font accurately, while preserving original licenses and authors.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Recent improvements")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Version 0.3.1 introduced a saved interface-language selection and Ricardo Barreto’s developer credit with a GitHub link.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Version 0.3.0 introduced the TuxFlash name and penguin icon, the x86-64 portable runtime and clearer partition/filesystem indicators.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Windows volume labels follow the ISO filename. DD/RAW offers read-back verification, and NTFS mounting can fall back from ntfs3 to ntfs-3g.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Rufus, Lufus and TuxFlash")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Rufus is the original Windows application by Pete Batard and contributors. Its original Windows code and resources remain in this project.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("Lufus provides the native C/GTK Linux foundation and a separate privileged helper for device operations.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("TuxFlash builds on that foundation with the current identity, Ricardo Barreto’s contributions, four interface languages, packaging and integration fixes. It runs natively on Linux without Wine.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("The Linux GTK interface follows the supplied Lufus layout; it is not the original Win32 window. TuxFlash is an independent derivative, not an official Rufus Linux release.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Scope and known limits")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("The ready-to-run portable package is Linux x86-64. Other architectures require a separate build and validation; no claim is made for every Linux distribution or every ISO.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("FFU is not supported. Windows To Go through a real desktop, Secure Boot and real Linux persistence across reboots still lack complete validation. Full feature parity with Rufus Windows has not been achieved.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    {
        g_autofree char *title=g_markup_escape_text(g_dgettext("tuxflash",N_("Free and open source")), -1);
        g_string_append_printf(xml,"<p>%s</p><ul>",title);
        {
            g_autofree char *item=g_markup_escape_text(g_dgettext("tuxflash",N_("TuxFlash continues under GPL-3.0-or-later. You can use, study, modify and redistribute it under that license. Third-party licenses and original credits remain intact; see Legal for details.")), -1);
            g_string_append_printf(xml,"<li>%s</li>",item);
        }
        g_string_append(xml,"</ul>");
    }
    return g_string_free(xml,FALSE);
}
