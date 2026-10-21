/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <glib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

/* Mounts below these system directories must never become write targets.
 * The component boundary keeps /home-backup and removable /media valid. */
static inline gboolean tuxflash_mount_is_protected(const char *path)
{
    static const char *roots[] = {"/boot", "/usr", "/var", "/home", "/etc", "/root", "/opt", "/srv", NULL};
    if (g_strcmp0(path, "/") == 0 || g_strcmp0(path, "[SWAP]") == 0)
        return TRUE;
    if (path == NULL) return FALSE;
    for (guint i = 0; roots[i]; i++) {
        size_t n = strlen(roots[i]);
        if (strncmp(path, roots[i], n) == 0 && (path[n] == 0 || path[n] == '/'))
            return TRUE;
    }
    return FALSE;
}

static inline uint64_t tuxflash_disk_sequence(dev_t device)
{
    g_autofree char *path = g_strdup_printf("/sys/dev/block/%u:%u/diskseq", major(device), minor(device));
    g_autofree char *text = NULL;
    char *end = NULL;
    if (!g_file_get_contents(path, &text, NULL, NULL)) return 0;
    errno = 0;
    guint64 value = g_ascii_strtoull(text, &end, 10);
    while (end && g_ascii_isspace(*end)) end++;
    return errno || end == text || !end || *end ? 0 : value;
}
