/*
 * Lufus block-device discovery
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "devices.h"
#include "device_safety.h"

#include <gio/gio.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *path;
    char *type;
    char *parent;
    char *mountpoints;
    char *model;
    char *vendor;
    char *serial;
    char *transport;
    char *major_minor;
    uint64_t size;
    gboolean removable;
    gboolean hotplug;
    gboolean read_only;
} DeviceRow;

static void row_free(DeviceRow *row)
{
    if (row == NULL)
        return;
    g_free(row->path);
    g_free(row->type);
    g_free(row->parent);
    g_free(row->mountpoints);
    g_free(row->model);
    g_free(row->vendor);
    g_free(row->serial);
    g_free(row->transport);
    g_free(row->major_minor);
    g_free(row);
}

void lufus_device_free(LufusDevice *device)
{
    if (device == NULL)
        return;
    g_free(device->path);
    g_free(device->name);
    g_free(device->model);
    g_free(device->vendor);
    g_free(device->serial);
    g_free(device->transport);
    g_free(device->major_minor);
    g_free(device);
}

static char hex_digit(char value)
{
    if (value >= '0' && value <= '9')
        return (char)(value - '0');
    if (value >= 'a' && value <= 'f')
        return (char)(value - 'a' + 10);
    if (value >= 'A' && value <= 'F')
        return (char)(value - 'A' + 10);
    return -1;
}

static char *decode_lsblk(const char *value)
{
    GString *decoded = g_string_sized_new(strlen(value));
    const char *cursor;
    for (cursor = value; *cursor != '\0'; cursor++) {
        if (cursor[0] == '\\' && cursor[1] == 'x' && cursor[2] != '\0' && cursor[3] != '\0') {
            char high = hex_digit(cursor[2]);
            char low = hex_digit(cursor[3]);
            if (high >= 0 && low >= 0) {
                g_string_append_c(decoded, (char)((high << 4) | low));
                cursor += 3;
                continue;
            }
        }
        g_string_append_c(decoded, *cursor);
    }
    return g_string_free(decoded, FALSE);
}

static char *pair_value(char **arguments, int argument_count, const char *key)
{
    size_t key_length = strlen(key);
    int index;
    for (index = 0; index < argument_count; index++) {
        if (strncmp(arguments[index], key, key_length) == 0 && arguments[index][key_length] == '=')
            return decode_lsblk(arguments[index] + key_length + 1);
    }
    return g_strdup("");
}

static gboolean value_is_true(const char *value)
{
    return strcmp(value, "1") == 0 || g_ascii_strcasecmp(value, "true") == 0;
}

static DeviceRow *parse_row(const char *line)
{
    DeviceRow *row;
    char **arguments = NULL;
    int argument_count = 0;
    char *size;
    char *removable;
    char *hotplug;
    char *read_only;
    if (!g_shell_parse_argv(line, &argument_count, &arguments, NULL))
        return NULL;
    row = g_new0(DeviceRow, 1);
    row->path = pair_value(arguments, argument_count, "PATH");
    row->type = pair_value(arguments, argument_count, "TYPE");
    row->parent = pair_value(arguments, argument_count, "PKNAME");
    row->mountpoints = pair_value(arguments, argument_count, "MOUNTPOINTS");
    row->model = pair_value(arguments, argument_count, "MODEL");
    row->vendor = pair_value(arguments, argument_count, "VENDOR");
    row->serial = pair_value(arguments, argument_count, "SERIAL");
    row->transport = pair_value(arguments, argument_count, "TRAN");
    row->major_minor = pair_value(arguments, argument_count, "MAJ:MIN");
    size = pair_value(arguments, argument_count, "SIZE");
    removable = pair_value(arguments, argument_count, "RM");
    hotplug = pair_value(arguments, argument_count, "HOTPLUG");
    read_only = pair_value(arguments, argument_count, "RO");
    row->size = g_ascii_strtoull(size, NULL, 10);
    row->removable = value_is_true(removable);
    row->hotplug = value_is_true(hotplug);
    row->read_only = value_is_true(read_only);
    g_free(size);
    g_free(removable);
    g_free(hotplug);
    g_free(read_only);
    g_strfreev(arguments);
    return row;
}

static DeviceRow *find_row(GPtrArray *rows, const char *path)
{
    guint index;
    for (index = 0; index < rows->len; index++) {
        DeviceRow *row = g_ptr_array_index(rows, index);
        if (g_strcmp0(row->path, path) == 0)
            return row;
    }
    return NULL;
}

static DeviceRow *whole_disk_for(GPtrArray *rows, DeviceRow *row)
{
    guint guard = 0;
    while (row != NULL && strcmp(row->type, "disk") != 0 && row->parent[0] != '\0' && guard++ < 32)
        row = find_row(rows, row->parent);
    return row != NULL && strcmp(row->type, "disk") == 0 ? row : NULL;
}

static gboolean protected_mounts(const char *mountpoints)
{
    g_auto(GStrv) items = g_strsplit(mountpoints, "\n", -1);
    for (guint i = 0; items[i]; i++)
        if (tuxflash_mount_is_protected(items[i])) return TRUE;
    return FALSE;
}

/* lsblk can repeat an LVM/RAID node under several parents. Follow all
 * occurrences, so every physical member of the system volume is hidden. */
static gboolean row_uses_disk(GPtrArray *rows, DeviceRow *row, DeviceRow *disk, guint depth)
{
    if (g_strcmp0(row->path, disk->path) == 0) return TRUE;
    if (depth >= 64) return TRUE; /* Fail closed on malformed/cyclic topology. */
    if (!row->parent[0]) return FALSE;
    for (guint i = 0; i < rows->len; i++) {
        DeviceRow *parent = g_ptr_array_index(rows, i);
        if (g_strcmp0(parent->path, row->parent) == 0 && row_uses_disk(rows, parent, disk, depth + 1))
            return TRUE;
    }
    return FALSE;
}

static gboolean disk_is_system(GPtrArray *rows, DeviceRow *disk)
{
    for (guint i = 0; i < rows->len; i++) {
        DeviceRow *row = g_ptr_array_index(rows, i);
        if (protected_mounts(row->mountpoints) && row_uses_disk(rows, row, disk, 0)) return TRUE;
    }
    return FALSE;
}

static gboolean disk_has_mounts(GPtrArray *rows, DeviceRow *disk)
{
    guint index;
    for (index = 0; index < rows->len; index++) {
        DeviceRow *row = g_ptr_array_index(rows, index);
        if (row->mountpoints[0] != '\0' && whole_disk_for(rows, row) == disk)
            return TRUE;
    }
    return FALSE;
}

static gboolean disk_is_external(const DeviceRow *row)
{
    return row->removable || row->hotplug ||
           strcmp(row->transport, "usb") == 0 ||
           strcmp(row->transport, "mmc") == 0 ||
           strcmp(row->transport, "firewire") == 0;
}

GPtrArray *lufus_devices_enumerate(gboolean include_fixed, GError **error)
{
    static const char *command[] = {
        "lsblk", "--pairs", "--bytes", "--paths", "--output",
        "PATH,TYPE,PKNAME,MOUNTPOINTS,SIZE,MODEL,VENDOR,SERIAL,TRAN,RM,HOTPLUG,RO,MAJ:MIN",
        NULL
    };
    GPtrArray *rows = g_ptr_array_new_with_free_func((GDestroyNotify)row_free);
    GPtrArray *devices = g_ptr_array_new_with_free_func((GDestroyNotify)lufus_device_free);
    char *standard_output = NULL;
    char *standard_error = NULL;
    int exit_status = 0;
    char **lines;
    guint index;
    if (!g_spawn_sync(NULL, (char **)command, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      &standard_output, &standard_error, &exit_status, error))
        goto fail;
    if (!g_spawn_check_wait_status(exit_status, error)) {
        if (standard_error != NULL && standard_error[0] != '\0')
            g_prefix_error(error, "%s: ", standard_error);
        goto fail;
    }
    lines = g_strsplit(standard_output, "\n", -1);
    for (index = 0; lines[index] != NULL; index++) {
        DeviceRow *row;
        if (lines[index][0] == '\0')
            continue;
        row = parse_row(lines[index]);
        if (row != NULL)
            g_ptr_array_add(rows, row);
    }
    g_strfreev(lines);
    for (index = 0; index < rows->len; index++) {
        DeviceRow *row = g_ptr_array_index(rows, index);
        LufusDevice *device;
        if (strcmp(row->type, "disk") != 0 || row->path[0] == '\0' ||
            disk_is_system(rows, row) || (!include_fixed && !disk_is_external(row)))
            continue;
        device = g_new0(LufusDevice, 1);
        device->path = g_strdup(row->path);
        device->name = g_path_get_basename(row->path);
        device->model = g_strdup(g_strstrip(row->model));
        device->vendor = g_strdup(g_strstrip(row->vendor));
        device->serial = g_strdup(g_strstrip(row->serial));
        device->transport = g_strdup(g_strstrip(row->transport));
        device->major_minor = g_strdup(row->major_minor);
        device->size = row->size;
        unsigned int dev_major, dev_minor;
        if (sscanf(row->major_minor, "%u:%u", &dev_major, &dev_minor) == 2)
            device->disk_sequence = tuxflash_disk_sequence(makedev(dev_major, dev_minor));
        device->removable = row->removable;
        device->hotplug = row->hotplug;
        device->read_only = row->read_only;
        device->mounted = disk_has_mounts(rows, row);
        g_ptr_array_add(devices, device);
    }
    g_free(standard_output);
    g_free(standard_error);
    g_ptr_array_unref(rows);
    return devices;

fail:
    g_free(standard_output);
    g_free(standard_error);
    g_ptr_array_unref(rows);
    g_ptr_array_unref(devices);
    return NULL;
}

char *lufus_format_size(uint64_t size)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = (double)size;
    guint unit = 0;
    while (value >= 1000.0 && unit + 1 < G_N_ELEMENTS(units)) {
        value /= 1000.0;
        unit++;
    }
    return unit == 0 ? g_strdup_printf("%.0f %s", value, units[unit])
                     : g_strdup_printf("%.1f %s", value, units[unit]);
}

char *lufus_device_display_name(const LufusDevice *device)
{
    char *size = lufus_format_size(device->size);
    char *identity = g_strjoin(" ", device->vendor, device->model, NULL);
    char *stripped = g_strstrip(identity);
    char *transport_upper = NULL;
    char *result;
    if (stripped[0] == '\0') {
        g_free(identity);
        identity = g_strdup(device->name);
    }
    if (device->transport[0] == '\0') {
        result = g_strdup_printf("%s [%s] — %s", identity, size, device->path);
    } else {
        transport_upper = g_ascii_strup(device->transport, -1);
        result = g_strdup_printf("%s [%s] — %s (%s)", identity, size, device->path,
                                 transport_upper);
    }
    g_free(transport_upper);
    g_free(identity);
    g_free(size);
    return result;
}
