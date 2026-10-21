/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "volume_label.h"
#include <string.h>

char *lufus_volume_label_for_image(const char *path, gboolean fat32)
{
    char *basename = path != NULL ? g_filename_display_basename(path) : g_strdup("");
    char *extension = strrchr(basename, '.');
    char *ascii;
    GString *label = g_string_new(NULL);
    gsize limit = fat32 ? 11 : 32;

    if (extension != NULL)
        *extension = '\0';
    ascii = g_str_to_ascii(basename, "C");
    /* Restricted characters also make the GPT name safe to quote in sfdisk. */
    for (const char *p = ascii; *p != '\0' && label->len < limit; p++) {
        char c = *p;
        if (g_ascii_isalnum(c) || c == '-' || c == '_')
            g_string_append_c(label, fat32 ? g_ascii_toupper(c) : c);
        else if (label->len > 0 && label->str[label->len - 1] != ' ')
            g_string_append_c(label, ' ');
    }
    g_strchomp(label->str);
    g_string_set_size(label, strlen(label->str));
    if (label->len == 0)
        g_string_assign(label, "INSTALL");
    g_free(ascii);
    g_free(basename);
    return g_string_free(label, FALSE);
}
