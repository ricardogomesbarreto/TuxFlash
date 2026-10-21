/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <glib.h>
void tuxflash_language_bind_catalogs(const char *locale_directory);
void tuxflash_language_init(const char *locale_directory);
const char *const *tuxflash_language_names(void);
guint tuxflash_language_selected(void);
gboolean tuxflash_language_save(guint selected, GError **error);
