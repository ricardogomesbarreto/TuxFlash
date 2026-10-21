/*
 * ISO capability scanner regression tests
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "image_probe.h"

#include <glib/gstdio.h>
#include <unistd.h>

static void test_windows_installer(void)
{
    static const char listing[] =
        "'/BOOTMGR'\n"
        "'/BOOTMGR.EFI'\n"
        "'/EFI/BOOT/BOOTX64.EFI'\n"
        "'/SOURCES/BOOT.WIM'\n"
        "'/SOURCES/INSTALL.ESD'\n";
    LufusImageReport report;

    lufus_image_report_from_listing(listing, &report);
    g_assert_true(report.is_iso);
    g_assert_true(report.has_windows);
    g_assert_true(report.has_bootmgr);
    g_assert_true(report.has_bootmgr_efi);
    g_assert_true(report.has_efi);
    g_assert_true(report.supports_windows_installer);
    g_assert_true(report.supports_windows_to_go);
    g_assert_false(report.supports_persistence);
}

static void test_ubuntu_persistence(void)
{
    static const char listing[] =
        "'/EFI/BOOT/BOOTX64.EFI'\n"
        "'/boot/grub/i386-pc/normal.mod'\n"
        "'/casper/vmlinuz'\n"
        "'/casper/filesystem.squashfs'\n";
    LufusImageReport report;

    lufus_image_report_from_listing(listing, &report);
    g_assert_false(report.has_windows);
    g_assert_true(report.has_efi);
    g_assert_true(report.has_grub);
    g_assert_true(report.uses_casper);
    g_assert_true(report.supports_persistence);
    g_assert_false(report.supports_windows_installer);
}

static void test_debian_persistence(void)
{
    static const char listing[] =
        "'/isolinux/isolinux.bin'\n"
        "'/isolinux/live.cfg'\n"
        "'/live/vmlinuz'\n"
        "'/live/filesystem.squashfs'\n";
    LufusImageReport report;

    lufus_image_report_from_listing(listing, &report);
    g_assert_true(report.has_syslinux);
    g_assert_true(report.uses_debian_live);
    g_assert_true(report.supports_persistence);
    g_assert_false(report.has_windows);
}

static void test_generic_iso_is_not_windows(void)
{
    static const char listing[] =
        "'/EFI/BOOT/BOOTX64.EFI'\n"
        "'/images/rootfs.img'\n"
        "'/README'\n";
    LufusImageReport report;

    lufus_image_report_from_listing(listing, &report);
    g_assert_true(report.is_iso);
    g_assert_true(report.has_efi);
    g_assert_false(report.has_windows);
    g_assert_false(report.supports_windows_installer);
    g_assert_false(report.supports_windows_to_go);
    g_assert_false(report.supports_persistence);
}

static void test_reactos_is_not_persistent_linux(void)
{
    static const char listing[] =
        "'/isolinux/isolinux.bin'\n"
        "'/live/vmlinuz'\n"
        "'/loader/freeldr.sys'\n";
    LufusImageReport report;

    lufus_image_report_from_listing(listing, &report);
    g_assert_true(report.has_reactos);
    g_assert_false(report.supports_persistence);
}

static void test_winpe_requires_complete_marker_set(void)
{
    LufusImageReport complete;
    LufusImageReport incomplete;

    lufus_image_report_from_listing(
        "'/amd64/ntdetect.com'\n"
        "'/amd64/setupldr.bin'\n"
        "'/amd64/txtsetup.sif'\n", &complete);
    lufus_image_report_from_listing(
        "'/amd64/ntdetect.com'\n"
        "'/amd64/setupldr.bin'\n", &incomplete);
    g_assert_true(complete.has_winpe);
    g_assert_true(complete.has_windows);
    g_assert_false(incomplete.has_winpe);
    g_assert_false(incomplete.has_windows);
}

static void test_reject_non_optical_files(void)
{
    char *path = NULL;
    GError *error = NULL;
    LufusImageReport report;
    int fd = g_file_open_tmp("lufus-invalid-iso-XXXXXX", &path, &error);
    g_assert_no_error(error);
    g_assert_cmpint(fd, >=, 0);
    close(fd);
    g_assert_true(g_file_set_contents(path, "abc", 3, &error));
    g_assert_no_error(error);
    g_assert_false(lufus_image_probe_iso(path, &report, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    g_assert_false(report.is_iso);
    g_clear_error(&error);
    char *blank = g_malloc0(65536);
    g_assert_true(g_file_set_contents(path, blank, 65536, &error));
    g_assert_no_error(error);
    g_assert_false(lufus_image_probe_iso(path, &report, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    g_clear_error(&error);
    g_free(blank);
    g_unlink(path);
    g_free(path);
}

static void write_marker(const char *root, const char *relative)
{
    char *path = g_build_filename(root, relative, NULL);
    char *parent = g_path_get_dirname(path);
    GError *error = NULL;

    g_assert_cmpint(g_mkdir_with_parents(parent, 0700), ==, 0);
    g_assert_true(g_file_set_contents(path, "marker", -1, &error));
    g_assert_no_error(error);
    g_free(parent);
    g_free(path);
}

static void test_real_iso_probe(void)
{
    char *temporary;
    char *tree;
    char *iso;
    GSubprocess *builder;
    LufusImageReport report;
    GError *error = NULL;
    const char *markers[] = {
        "bootmgr", "bootmgr.efi", "EFI/BOOT/bootx64.efi",
        "sources/boot.wim", "sources/install.swm", NULL
    };
    guint index;

    temporary = g_dir_make_tmp("lufus-image-probe-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(temporary);
    tree = g_build_filename(temporary, "tree", NULL);
    iso = g_build_filename(temporary, "windows.iso", NULL);
    for (index = 0; markers[index] != NULL; index++)
        write_marker(tree, markers[index]);
    builder = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &error, "xorriso", "-as", "mkisofs", "-quiet", "-o", iso, tree,
        NULL);
    g_assert_no_error(error);
    g_assert_nonnull(builder);
    g_assert_true(g_subprocess_wait_check(builder, NULL, &error));
    g_assert_no_error(error);
    g_object_unref(builder);

    g_assert_true(lufus_image_probe_iso(iso, &report, &error));
    g_assert_no_error(error);
    g_assert_true(report.supports_windows_installer);
    g_assert_true(report.supports_windows_to_go);

    for (index = 0; markers[index] != NULL; index++) {
        char *path = g_build_filename(tree, markers[index], NULL);
        g_assert_cmpint(g_remove(path), ==, 0);
        g_free(path);
    }
    {
        char *boot = g_build_filename(tree, "EFI", "BOOT", NULL);
        char *efi = g_build_filename(tree, "EFI", NULL);
        char *sources = g_build_filename(tree, "sources", NULL);
        g_assert_cmpint(g_rmdir(boot), ==, 0);
        g_assert_cmpint(g_rmdir(efi), ==, 0);
        g_assert_cmpint(g_rmdir(sources), ==, 0);
        g_free(boot);
        g_free(efi);
        g_free(sources);
    }
    g_assert_cmpint(g_rmdir(tree), ==, 0);
    g_assert_cmpint(g_remove(iso), ==, 0);
    g_assert_cmpint(g_rmdir(temporary), ==, 0);
    g_free(iso);
    g_free(tree);
    g_free(temporary);
}

int main(int argc, char **argv)
{
    if (argc == 3 && g_strcmp0(argv[1], "--probe") == 0) {
        LufusImageReport report;
        GError *error = NULL;
        if (!lufus_image_probe_iso(argv[2], &report, &error)) {
            g_printerr("%s\n", error->message);
            g_clear_error(&error);
            return 1;
        }
        g_print("is_iso=%d windows=%d windows_installer=%d windows_to_go=%d "
                "efi=%d bios=%d persistence=%d casper=%d debian_live=%d\n",
                report.is_iso, report.has_windows,
                report.supports_windows_installer,
                report.supports_windows_to_go, report.has_efi,
                report.has_bootmgr || report.has_winpe,
                report.supports_persistence, report.uses_casper,
                report.uses_debian_live);
        return 0;
    }
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/image-probe/windows-installer", test_windows_installer);
    g_test_add_func("/image-probe/ubuntu-persistence", test_ubuntu_persistence);
    g_test_add_func("/image-probe/debian-persistence", test_debian_persistence);
    g_test_add_func("/image-probe/generic-not-windows", test_generic_iso_is_not_windows);
    g_test_add_func("/image-probe/reactos-not-persistence", test_reactos_is_not_persistent_linux);
    g_test_add_func("/image-probe/winpe-markers", test_winpe_requires_complete_marker_set);
    g_test_add_func("/image-probe/real-iso", test_real_iso_probe);
    g_test_add_func("/image-probe/reject-non-optical-files", test_reject_non_optical_files);
    return g_test_run();
}
