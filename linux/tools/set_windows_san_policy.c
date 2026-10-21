/*
 * Set the Windows To Go SAN policy in an offline SYSTEM registry hive.
 *
 * This is the Linux/libhivex equivalent of applying the
 * Microsoft-Windows-PartitionManager SanPolicy=4 offline-servicing setting.
 * It replaces the Perl hivexregedit frontend that was carried by the former
 * BCD-SYS AppImage runtime.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <errno.h>
#include <hivex.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static hive_node_h require_child(hive_h *hive, hive_node_h parent,
                                 const char *name)
{
    hive_node_h node;

    errno = 0;
    node = hivex_node_get_child(hive, parent, name);
    if (node == 0)
        fprintf(stderr, "Required registry key %s is unavailable: %s\n",
                name, errno != 0 ? strerror(errno) : "not found");
    return node;
}

static hive_node_h ensure_child(hive_h *hive, hive_node_h parent,
                                const char *name)
{
    hive_node_h node;

    errno = 0;
    node = hivex_node_get_child(hive, parent, name);
    if (node != 0)
        return node;
    errno = 0;
    node = hivex_node_add_child(hive, parent, name);
    if (node == 0)
        fprintf(stderr, "Could not create registry key %s: %s\n",
                name, strerror(errno));
    return node;
}

int main(int argc, char **argv)
{
    hive_h *hive = NULL;
    hive_node_h root;
    hive_node_h select;
    hive_node_h control_set;
    hive_node_h services;
    hive_node_h partmgr;
    hive_node_h parameters;
    hive_value_h current_value;
    int32_t current;
    char control_set_name[32];
    const uint8_t policy[] = {4, 0, 0, 0};
    hive_set_value value = {
        .key = "SanPolicy",
        .t = hive_t_REG_DWORD,
        .len = sizeof(policy),
        .value = (char *)policy,
    };
    int status = 1;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s SYSTEM-hive\n", argv[0]);
        return 2;
    }

    hive = hivex_open(argv[1], HIVEX_OPEN_WRITE);
    if (hive == NULL) {
        fprintf(stderr, "Could not open SYSTEM hive: %s\n", strerror(errno));
        return 1;
    }
    root = hivex_root(hive);
    if (root == 0) {
        fprintf(stderr, "Could not read SYSTEM hive root: %s\n", strerror(errno));
        goto out;
    }
    select = require_child(hive, root, "Select");
    if (select == 0)
        goto out;
    current_value = hivex_node_get_value(hive, select, "Current");
    if (current_value == 0) {
        fprintf(stderr, "Could not determine the active Windows control set\n");
        goto out;
    }
    errno = 0;
    current = hivex_value_dword(hive, current_value);
    if (errno != 0 || current < 1 || current > 999) {
        fprintf(stderr, "Invalid active Windows control set: %d\n", current);
        goto out;
    }
    snprintf(control_set_name, sizeof(control_set_name),
             "ControlSet%03d", current);
    control_set = require_child(hive, root, control_set_name);
    if (control_set == 0)
        goto out;
    services = ensure_child(hive, control_set, "Services");
    if (services == 0)
        goto out;
    partmgr = ensure_child(hive, services, "partmgr");
    if (partmgr == 0)
        goto out;
    parameters = ensure_child(hive, partmgr, "Parameters");
    if (parameters == 0)
        goto out;
    if (hivex_node_set_value(hive, parameters, &value, 0) != 0) {
        fprintf(stderr, "Could not set Windows SAN policy: %s\n", strerror(errno));
        goto out;
    }
    if (hivex_commit(hive, NULL, 0) != 0) {
        fprintf(stderr, "Could not commit Windows SAN policy: %s\n", strerror(errno));
        goto out;
    }
    status = 0;

out:
    if (hivex_close(hive) != 0 && status == 0) {
        fprintf(stderr, "Could not close SYSTEM hive: %s\n", strerror(errno));
        status = 1;
    }
    return status;
}
