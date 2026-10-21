/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RUFUS_LINUX_PLATFORM_H
#define RUFUS_LINUX_PLATFORM_H

#include <stdint.h>

/* The Linux port is in progress. These functions operate on regular files only.
 * There is deliberately no raw block-device writer in this initial layer. */
typedef int (*rf_progress_fn)(uint64_t done, uint64_t total, void *context);

struct rf_hashes {
    char md5[33];
    char sha1[41];
    char sha256[65];
    char sha512[129];
};

struct rf_image_info {
    uint64_t size;
    int mbr_signature;
    int gpt_header_512;
    int gpt_header_4096;
    int iso9660;
};

/* Return 0 on success, -1 with errno on error/cancellation. A nonzero callback
 * return cancels. Progress reaching total does not mean fsync/verification passed. */
int rf_hash_image(const char *path, struct rf_hashes *out,
                  rf_progress_fn progress, void *context);
int rf_inspect_image(const char *path, struct rf_image_info *out);

/* Creates a new mode-0600 regular file, refuses existing destinations, flushes,
 * and verifies every byte by rereading source and destination. A failure leaves
 * the new, possibly incomplete file for inspection; it is never called success. */
int rf_copy_new_image(const char *source, const char *destination,
                      rf_progress_fn progress, void *context);

#endif
