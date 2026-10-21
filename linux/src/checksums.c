/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "checksums.h"
#include "platform.h"
#include <errno.h>

static int cancelled(uint64_t completed, uint64_t total, void *context)
{
    GCancellable *cancellable = context;
    (void)completed;
    (void)total;
    return cancellable != NULL && g_cancellable_is_cancelled(cancellable);
}

char *lufus_checksums_file(const char *path, GCancellable *cancellable, GError **error)
{
    struct rf_hashes hashes;
    if (rf_hash_image(path, &hashes, cancelled, cancellable) != 0) {
        int saved = errno;
        if (saved == ECANCELED)
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                "Checksum calculation cancelled");
        else
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved),
                        "Could not hash image: %s", g_strerror(saved));
        return NULL;
    }
    return g_strdup_printf("MD5  %s\nSHA-1  %s\nSHA-256  %s\nSHA-512  %s\n",
                           hashes.md5, hashes.sha1, hashes.sha256, hashes.sha512);
}
