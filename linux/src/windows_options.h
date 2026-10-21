/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gio/gio.h>

typedef enum {
    LUFUS_WINDOWS_BYPASS_REQUIREMENTS = 1U << 0,
    LUFUS_WINDOWS_NO_ONLINE_ACCOUNT = 1U << 1,
    LUFUS_WINDOWS_NO_DATA_COLLECTION = 1U << 2,
    LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT = 1U << 3,
    LUFUS_WINDOWS_DISABLE_BITLOCKER = 1U << 4,
    LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES = 1U << 5,
    LUFUS_WINDOWS_SILENT_INSTALL = 1U << 6,
} LufusWindowsFlags;

#define LUFUS_WINDOWS_ALL_FLAGS \
    (LUFUS_WINDOWS_BYPASS_REQUIREMENTS | \
     LUFUS_WINDOWS_NO_ONLINE_ACCOUNT | \
     LUFUS_WINDOWS_NO_DATA_COLLECTION | \
     LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT | \
     LUFUS_WINDOWS_DISABLE_BITLOCKER | \
     LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES | \
     LUFUS_WINDOWS_SILENT_INSTALL)

typedef struct {
    guint flags;
    /* 1-based install.wim/install.esd image selected for silent installation. */
    guint image_index;
    /* Installer-media partition used to prove that Disk 1 is the boot USB. */
    guint boot_media_partition;
    char *architecture;
    /* BCP 47 language taken from the selected Windows image (for example en-GB). */
    char *language;
    char *local_account;
} LufusWindowsOptions;

void lufus_windows_options_clear(LufusWindowsOptions *options);
gboolean lufus_windows_options_validate(const LufusWindowsOptions *options,
                                        GError **error);
gboolean lufus_windows_write_unattend(const char *path,
                                      const LufusWindowsOptions *options,
                                      GError **error);
