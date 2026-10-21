/*
 * Lufus progress allocation shared definitions
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LUFUS_RUFUS_STATUS_COPY_ISO "Copying ISO files"
#define LUFUS_RUFUS_STATUS_APPLY_WINDOWS "Applying Windows image"
#define LUFUS_RUFUS_STATUS_CUSTOMIZE_WINDOWS "Applying Windows customization"
#define LUFUS_RUFUS_STATUS_APPLY_USER_OPTIONS "Applying user options..."
#define LUFUS_RUFUS_STATUS_FINALIZE "Finalizing, please wait..."
#define LUFUS_STATUS_VERIFY_MEDIA "Verifying media"
#define LUFUS_RUFUS_PATCH_PROGRESS_TOTAL 207U

typedef struct {
    unsigned int analyze_end;
    unsigned int zero_end;
    unsigned int partition_end;
    unsigned int format_end;
    unsigned int copy_start;
    unsigned int copy_end;
    unsigned int finalize_allocation_start;
    unsigned int finalize_visible_start;
} LufusWindowsProgressLayout;

/* Outer operation ranges.  The media writer maps its native progress into
 * media_start..media_end.  When validation is disabled the verification
 * interval is deliberately empty, leaving only the short final commit. */
typedef struct {
    unsigned int media_start;
    unsigned int media_end;
    unsigned int verification_start;
    unsigned int verification_end;
    unsigned int finalize_start;
    unsigned int finalize_end;
} LufusMediaProgressLayout;

LufusWindowsProgressLayout lufus_rufus_windows_progress_layout(bool ntfs,
                                                               bool customization);
LufusMediaProgressLayout lufus_media_progress_layout(bool validation);

/* Convert byte counts to the helper protocol's 0..1000 relative scale without
 * multiplying two 64-bit values.  A zero total is indeterminate (0). */
unsigned int lufus_byte_progress_thousandths(uint64_t completed,
                                             uint64_t total);
