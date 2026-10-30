/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "release_notes.h"
#include <glib/gi18n.h>

char *tuxflash_release_notes(void)
{
    GString *xml=g_string_new(NULL);
    {
        const char *items[] = {
            N_("Updated in 0.3.8"),
            N_("Cancellation now reaches subprocesses that remain in the writing tool process group."),
            N_("Stubborn tools are force-stopped after a two-second grace period. Successful background filesystem services are preserved."),
            N_("Failed external commands report an I/O error instead of a stale success message."),
        };
        g_autofree char *title = g_markup_escape_text(g_dgettext("tuxflash", items[0]), -1);
        g_string_append_printf(xml, "<p>%s</p><ul>", title);
        for (guint i = 1; i < G_N_ELEMENTS(items); i++) {
            g_autofree char *item = g_markup_escape_text(g_dgettext("tuxflash", items[i]), -1);
            g_string_append_printf(xml, "<li>%s</li>", item);
        }
        g_string_append(xml, "</ul>");
    }
    {
        const char *items[] = {
            N_("Updated in 0.3.7"),
            N_("Protects the administrative helper from premature termination when the frontend log pipe is closed."),
            N_("Media-creation commands no longer count as successful if input delivery fails or cancellation was requested."),
            N_("Expanded regression and VM tests cover log-pipe loss and abrupt frontend exit without accessing physical disks."),
        };
        g_autofree char *title = g_markup_escape_text(g_dgettext("tuxflash", items[0]), -1);
        g_string_append_printf(xml, "<p>%s</p><ul>", title);
        for (guint i = 1; i < G_N_ELEMENTS(items); i++) {
            g_autofree char *item = g_markup_escape_text(g_dgettext("tuxflash", items[i]), -1);
            g_string_append_printf(xml, "<li>%s</li>", item);
        }
        g_string_append(xml, "</ul>");
    }
    {
        const char *items[] = {
            N_("Updated in 0.3.6"),
            N_("Cancellation uses a private channel from the GUI or CLI to the administrative helper, preserving privilege separation."),
            N_("The window fits the screen and keeps status and action buttons visible. Long text adapts to the available width without visible scrollbars; expanded options remain accessible by mouse, touch and keyboard."),
            N_("Compressed images are opened once and decoded from the validated file descriptor. Symbolic links, empty files and non-regular sources are rejected."),
            N_("RAW, VHD and VHDX captures reserve the output exclusively. Virtual-disk tools and ownership changes use the reserved file descriptor, preserving existing files if a path is replaced."),
            N_("Cancellation now interrupts blocked writes instead of restarting them. New regression tests cover all four decoders, file replacement and VHD/VHDX output integrity."),
        };
        g_autofree char *title = g_markup_escape_text(g_dgettext("tuxflash", items[0]), -1);
        g_string_append_printf(xml, "<p>%s</p><ul>", title);
        for (guint i = 1; i < G_N_ELEMENTS(items); i++) {
            g_autofree char *item = g_markup_escape_text(g_dgettext("tuxflash", items[i]), -1);
            g_string_append_printf(xml, "<li>%s</li>", item);
        }
        g_string_append(xml, "</ul>");
    }
    {
        g_autofree char *title = g_markup_escape_text(g_dgettext("tuxflash", N_("Updated in 0.3.5")), -1);
        g_string_append_printf(xml, "<p>%s</p><ul>", title);
        {
            g_autofree char *item = g_markup_escape_text(g_dgettext("tuxflash", N_("Raw and compressed read-back verification now requires the source to end at the exact written length. Changed sources and damaged compressed trailers cannot be reported as verified.")), -1);
            g_string_append_printf(xml, "<li>%s</li>", item);
        }
        {
            g_autofree char *item = g_markup_escape_text(g_dgettext("tuxflash", N_("Verification progress is rate-limited, avoiding excessive events from small decompressor pipe reads.")), -1);
            g_string_append_printf(xml, "<li>%s</li>", item);
        }
        {
            g_autofree char *item = g_markup_escape_text(g_dgettext("tuxflash", N_("Cancellation interrupts source, target and end-of-stream reads. Regression tests cover gzip, xz, bzip2 and zstd, truncated and enlarged sources, target corruption and padding.")), -1);
            g_string_append_printf(xml, "<li>%s</li>", item);
        }
        g_string_append(xml, "</ul>");
    }
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
