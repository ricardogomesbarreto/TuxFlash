/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/devices.c"

static void test_protected_paths(void)
{
    const char *unsafe[] = {"/", "/boot", "/boot/efi", "/home/user", "/var/lib", "/usr/local", "/etc", "/root", "[SWAP]", NULL};
    const char *safe[] = {"", "/media/usb", "/run/media/user/usb", "/mnt/usb", "/home-backup", "/bootable", NULL};
    for (guint i = 0; unsafe[i]; i++) g_assert_true(tuxflash_mount_is_protected(unsafe[i]));
    for (guint i = 0; safe[i]; i++) g_assert_false(tuxflash_mount_is_protected(safe[i]));
}
static void test_multi_parent_system(void)
{
    const char *lines[] = {
        "PATH=/dev/a TYPE=disk", "PATH=/dev/b TYPE=disk", "PATH=/dev/c TYPE=disk",
        "PATH=/dev/a1 TYPE=part PKNAME=/dev/a", "PATH=/dev/b1 TYPE=part PKNAME=/dev/b",
        "PATH=/dev/raid TYPE=raid1 PKNAME=/dev/a1", "PATH=/dev/raid TYPE=raid1 PKNAME=/dev/b1",
        "PATH=/dev/crypt TYPE=crypt PKNAME=/dev/raid MOUNTPOINTS=/home/user", NULL};
    GPtrArray *rows = g_ptr_array_new_with_free_func((GDestroyNotify)row_free);
    for (guint i = 0; lines[i]; i++) g_ptr_array_add(rows, parse_row(lines[i]));
    g_assert_true(disk_is_system(rows, g_ptr_array_index(rows, 0)));
    g_assert_true(disk_is_system(rows, g_ptr_array_index(rows, 1)));
    g_assert_false(disk_is_system(rows, g_ptr_array_index(rows, 2)));
    g_ptr_array_unref(rows);
}
static void test_swap_disk(void)
{
    GPtrArray *rows = g_ptr_array_new_with_free_func((GDestroyNotify)row_free);
    g_ptr_array_add(rows, parse_row("PATH=/dev/a TYPE=disk"));
    g_ptr_array_add(rows, parse_row("PATH=/dev/a1 TYPE=part PKNAME=/dev/a MOUNTPOINTS=[SWAP]"));
    g_assert_true(disk_is_system(rows, g_ptr_array_index(rows, 0)));
    g_ptr_array_unref(rows);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/device-safety/protected-path-boundaries", test_protected_paths);
    g_test_add_func("/device-safety/multiple-system-disk-parents", test_multi_parent_system);
    g_test_add_func("/device-safety/active-swap-disk", test_swap_disk);
    return g_test_run();
}
