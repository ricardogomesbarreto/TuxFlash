/*
 * Linux-native ISO capability scanner
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

typedef struct {
    gboolean is_iso;
    gboolean has_bootmgr;
    gboolean has_bootmgr_efi;
    gboolean has_boot_wim;
    gboolean has_install_image;
    gboolean has_winpe;
    gboolean has_efi;
    gboolean has_syslinux;
    gboolean has_grub;
    gboolean has_reactos;
    gboolean has_kolibrios;
    gboolean uses_casper;
    gboolean uses_debian_live;
    gboolean has_windows;
    gboolean supports_windows_installer;
    gboolean supports_windows_to_go;
    gboolean supports_persistence;
} LufusImageReport;

/* Parse the newline-delimited, shell-quoted path form emitted by
 * `xorriso -find / -type f -exec echo`. Exposed for regression tests. */
void lufus_image_report_from_listing(const char *listing,
                                     LufusImageReport *report);

/* Scan an ISO without mounting it or requiring elevated privileges. */
gboolean lufus_image_probe_iso(const char *path,
                               LufusImageReport *report,
                               GError **error);
