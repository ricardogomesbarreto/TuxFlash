/*
 * Lufus progress allocation shared definitions
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "progress.h"

LufusWindowsProgressLayout lufus_rufus_windows_progress_layout(bool ntfs,
                                                               bool customization)
{
    LufusWindowsProgressLayout layout = {
        .analyze_end = 10,
        .zero_end = 20,
        .partition_end = 30,
        .format_end = ntfs ? 150 : 80,
        .copy_start = ntfs ? 160 : 90,
        .finalize_allocation_start = ntfs ? 970 : 980,
        .finalize_visible_start = ntfs ? 980 : 990,
    };

    layout.copy_end = layout.finalize_allocation_start;
    if (customization) {
        /* Rufus gives OP_FILE_COPY and OP_PATCH equal shares of all space left
         * after its fixed 1% operation slots.  Keep the half-percent boundary
         * in thousandths instead of rounding it away in either frontend. */
        layout.copy_end = layout.copy_start +
                          (layout.finalize_allocation_start - layout.copy_start) / 2;
    }
    return layout;
}

LufusMediaProgressLayout lufus_media_progress_layout(bool validation)
{
    LufusMediaProgressLayout layout = {
        .media_start = 0,
        .media_end = validation ? 900 : 990,
        .verification_start = validation ? 900 : 990,
        .verification_end = 990,
        .finalize_start = 990,
        .finalize_end = 1000,
    };

    return layout;
}

unsigned int lufus_byte_progress_thousandths(uint64_t completed,
                                             uint64_t total)
{
    if (total == 0)
        return 0;
    if (completed >= total)
        return 1000;

    /* GCC and Clang provide 128-bit integers on every architecture supported
     * by the Debian package.  The wider intermediate preserves exact floor
     * semantics all the way through UINT64_MAX. */
    return (unsigned int)(((__uint128_t)completed * 1000U) / total);
}
