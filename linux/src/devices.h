/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <glib.h>
#include <stdint.h>

typedef struct {
    char *path;
    char *name;
    char *model;
    char *vendor;
    char *serial;
    char *transport;
    char *major_minor;
    uint64_t size;
    uint64_t disk_sequence;
    gboolean removable;
    gboolean hotplug;
    gboolean read_only;
    gboolean mounted;
} LufusDevice;

GPtrArray *lufus_devices_enumerate(gboolean include_fixed, GError **error);
void lufus_device_free(LufusDevice *device);
char *lufus_device_display_name(const LufusDevice *device);
char *lufus_format_size(uint64_t size);
