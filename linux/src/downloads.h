/*
 * Lufus official-image discovery and download support
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The Microsoft request sequence is a native C port of the GPL-3.0-or-later
 * Fido project by Pete Batard: https://github.com/pbatard/Fido
 */

#pragma once

#include <gio/gio.h>

typedef struct {
    char *session_id;
    char *sku_id;
    char *architecture;
} LufusWindowsSku;

typedef struct {
    char *name;
    char *display_name;
    GPtrArray *skus;
} LufusWindowsLanguage;

typedef struct {
    char *release;
    char *release_name;
    char *locale;
    GPtrArray *languages;
    gpointer session;
} LufusWindowsCatalog;

typedef void (*LufusDownloadProgress)(guint64 completed, guint64 total,
                                      gpointer user_data);

const char *lufus_windows_release_name(const char *release);
const char *lufus_windows_release_download_page(const char *release);
gboolean lufus_windows_service_error_is_sentinel(gint64 type,
                                                   const char *key,
                                                   const char *value);
LufusWindowsCatalog *lufus_windows_catalog_discover(const char *release,
                                                     const char *locale,
                                                     GCancellable *cancellable,
                                                     GError **error);
LufusWindowsCatalog *lufus_windows_catalog_discover_for_architecture(
    const char *release, const char *locale, const char *architecture,
    GCancellable *cancellable, GError **error);
void lufus_windows_catalog_free(LufusWindowsCatalog *catalog);
const LufusWindowsLanguage *lufus_windows_catalog_find_language(
    const LufusWindowsCatalog *catalog, const char *name);
char *lufus_windows_catalog_resolve(const LufusWindowsCatalog *catalog,
                                    const LufusWindowsLanguage *language,
                                    const char *architecture,
                                    GCancellable *cancellable,
                                    GError **error);
gboolean lufus_download_url(const char *url, const char *output,
                            LufusDownloadProgress progress, gpointer user_data,
                            GCancellable *cancellable, GError **error);
