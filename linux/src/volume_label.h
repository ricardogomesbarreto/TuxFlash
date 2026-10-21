/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <glib.h>

/* Caller owns the returned ASCII label. FAT32 uses 11 bytes, NTFS 32. */
char *lufus_volume_label_for_image(const char *path, gboolean fat32);
