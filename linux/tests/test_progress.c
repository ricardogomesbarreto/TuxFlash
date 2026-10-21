/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "progress.h"

#include <glib.h>

#ifndef LUFUS_SOURCE_ROOT
#define LUFUS_SOURCE_ROOT "."
#endif

static void assert_layout(LufusWindowsProgressLayout layout,
                          unsigned int analyze_end,
                          unsigned int zero_end,
                          unsigned int partition_end,
                          unsigned int format_end,
                          unsigned int copy_start,
                          unsigned int copy_end,
                          unsigned int finalize_allocation_start,
                          unsigned int finalize_visible_start)
{
    g_assert_cmpuint(layout.analyze_end, ==, analyze_end);
    g_assert_cmpuint(layout.zero_end, ==, zero_end);
    g_assert_cmpuint(layout.partition_end, ==, partition_end);
    g_assert_cmpuint(layout.format_end, ==, format_end);
    g_assert_cmpuint(layout.copy_start, ==, copy_start);
    g_assert_cmpuint(layout.copy_end, ==, copy_end);
    g_assert_cmpuint(layout.finalize_allocation_start, ==,
                     finalize_allocation_start);
    g_assert_cmpuint(layout.finalize_visible_start, ==,
                     finalize_visible_start);

    g_assert_cmpuint(layout.analyze_end, <, layout.zero_end);
    g_assert_cmpuint(layout.zero_end, <, layout.partition_end);
    g_assert_cmpuint(layout.partition_end, <, layout.format_end);
    g_assert_cmpuint(layout.format_end, <, layout.copy_start);
    g_assert_cmpuint(layout.copy_start, <, layout.copy_end);
    g_assert_cmpuint(layout.copy_end, <=, layout.finalize_allocation_start);
    g_assert_cmpuint(layout.finalize_allocation_start, <,
                     layout.finalize_visible_start);
    g_assert_cmpuint(layout.finalize_visible_start, <, 1000);
}

static double map_relative(unsigned int start, unsigned int end,
                           double relative)
{
    g_assert_cmpfloat(relative, >=, 0.0);
    g_assert_cmpfloat(relative, <=, 1.0);
    return start + relative * (end - start);
}

static void test_fat32_layouts(void)
{
    LufusWindowsProgressLayout plain =
        lufus_rufus_windows_progress_layout(false, false);
    LufusWindowsProgressLayout customized =
        lufus_rufus_windows_progress_layout(false, true);

    /* Rufus quick-FAT32 fixed operations occupy 11%, leaving 89% for the
     * analog copy operation, or 44.5% each for copy and customization. */
    assert_layout(plain, 10, 20, 30, 80, 90, 980, 980, 990);
    assert_layout(customized, 10, 20, 30, 80, 90, 535, 980, 990);
    g_assert_cmpfloat_with_epsilon(
        map_relative(customized.copy_start, customized.copy_end, 0.5),
        312.5, 0.0001);
}

static void test_ntfs_layouts(void)
{
    LufusWindowsProgressLayout plain =
        lufus_rufus_windows_progress_layout(true, false);
    LufusWindowsProgressLayout customized =
        lufus_rufus_windows_progress_layout(true, true);

    /* Rufus quick-NTFS fixed operations occupy 19%, leaving 81% for copy,
     * or 40.5% each for copy and customization. */
    assert_layout(plain, 10, 20, 30, 150, 160, 970, 970, 980);
    assert_layout(customized, 10, 20, 30, 150, 160, 565, 970, 980);
    g_assert_cmpfloat_with_epsilon(
        map_relative(customized.copy_start, customized.copy_end, 0.5),
        362.5, 0.0001);
}

static void test_windows_to_go_mapping(void)
{
    LufusWindowsProgressLayout layout =
        lufus_rufus_windows_progress_layout(true, false);
    double previous = map_relative(layout.copy_start, layout.copy_end, 0.0);
    const double samples[] = {0.10, 0.25, 0.50, 0.75, 0.98, 0.99, 1.0};
    guint index;

    /* Upstream Rufus assigns Windows To Go customization no separate OP_PATCH
     * slot. WIM apply occupies the first 98% of OP_FILE_COPY, with its boot
     * preparation advancing that operation through 99% and 100%. */
    for (index = 0; index < G_N_ELEMENTS(samples); index++) {
        double current = map_relative(layout.copy_start, layout.copy_end,
                                      samples[index]);
        g_assert_cmpfloat(current, >=, previous);
        previous = current;
    }
    g_assert_cmpfloat_with_epsilon(
        map_relative(layout.copy_start, layout.copy_end, 0.98),
        953.8, 0.0001);
    g_assert_cmpfloat_with_epsilon(
        map_relative(layout.copy_start, layout.copy_end, 0.99),
        961.9, 0.0001);
    g_assert_cmpfloat_with_epsilon(previous, 970.0, 0.0001);
}

static char *format_percent_status(const char *base, unsigned int tenths)
{
    return g_strdup_printf("%s: %.1f%%", base, tenths / 10.0);
}

static void test_visible_statuses(void)
{
    char *status;

    g_assert_cmpstr(LUFUS_RUFUS_STATUS_COPY_ISO, ==, "Copying ISO files");
    g_assert_cmpstr(LUFUS_RUFUS_STATUS_APPLY_WINDOWS, ==,
                    "Applying Windows image");
    g_assert_cmpstr(LUFUS_RUFUS_STATUS_CUSTOMIZE_WINDOWS, ==,
                    "Applying Windows customization");
    g_assert_cmpstr(LUFUS_RUFUS_STATUS_APPLY_USER_OPTIONS, ==,
                    "Applying user options...");
    g_assert_cmpstr(LUFUS_RUFUS_STATUS_FINALIZE, ==,
                    "Finalizing, please wait...");
    g_assert_cmpstr(LUFUS_STATUS_VERIFY_MEDIA, ==, "Verifying media");

    status = format_percent_status(LUFUS_RUFUS_STATUS_COPY_ISO, 421);
    g_assert_cmpstr(status, ==, "Copying ISO files: 42.1%");
    g_free(status);
    status = format_percent_status(LUFUS_RUFUS_STATUS_APPLY_WINDOWS, 980);
    g_assert_cmpstr(status, ==, "Applying Windows image: 98.0%");
    g_free(status);
    status = format_percent_status(LUFUS_RUFUS_STATUS_CUSTOMIZE_WINDOWS, 5);
    g_assert_cmpstr(status, ==, "Applying Windows customization: 0.5%");
    g_free(status);
}

static void assert_media_layout(LufusMediaProgressLayout layout,
                                unsigned int media_end,
                                unsigned int verification_start,
                                unsigned int verification_end)
{
    g_assert_cmpuint(layout.media_start, ==, 0);
    g_assert_cmpuint(layout.media_end, ==, media_end);
    g_assert_cmpuint(layout.verification_start, ==, verification_start);
    g_assert_cmpuint(layout.verification_end, ==, verification_end);
    g_assert_cmpuint(layout.finalize_start, ==, 990);
    g_assert_cmpuint(layout.finalize_end, ==, 1000);
    g_assert_cmpuint(layout.media_start, <=, layout.media_end);
    g_assert_cmpuint(layout.media_end, <=, layout.verification_start);
    g_assert_cmpuint(layout.verification_start, <=, layout.verification_end);
    g_assert_cmpuint(layout.verification_end, ==, layout.finalize_start);
    g_assert_cmpuint(layout.finalize_start, <, layout.finalize_end);
}

static void test_validation_layout_enabled(void)
{
    /* Media creation ends at 90%; verification is an explicit, visible 9%
     * byte-progress phase; final target commit owns only the last 1%. */
    assert_media_layout(lufus_media_progress_layout(true), 900, 900, 990);
}

static void test_validation_layout_disabled(void)
{
    /* Opting out expands media creation and collapses validation to an empty
     * range.  It must not leave a fake verification pause in the UI. */
    assert_media_layout(lufus_media_progress_layout(false), 990, 990, 990);
}

static void test_byte_progress(void)
{
    guint64 huge = G_MAXUINT64;

    g_assert_cmpuint(lufus_byte_progress_thousandths(0, 0), ==, 0);
    g_assert_cmpuint(lufus_byte_progress_thousandths(0, 10), ==, 0);
    g_assert_cmpuint(lufus_byte_progress_thousandths(1, 4), ==, 250);
    g_assert_cmpuint(lufus_byte_progress_thousandths(1, 3), ==, 333);
    g_assert_cmpuint(lufus_byte_progress_thousandths(3, 4), ==, 750);
    g_assert_cmpuint(lufus_byte_progress_thousandths(10, 10), ==, 1000);
    g_assert_cmpuint(lufus_byte_progress_thousandths(11, 10), ==, 1000);

    /* The implementation must not overflow on real multi-terabyte counters
     * or on the largest values representable by the protocol. */
    g_assert_cmpuint(lufus_byte_progress_thousandths(huge / 2, huge), ==, 499);
    g_assert_cmpuint(lufus_byte_progress_thousandths(huge - 1, huge), ==, 999);
}

static void test_forbidden_windows_statuses(void)
{
    char *helper_path = g_build_filename(LUFUS_SOURCE_ROOT, "src", "helper.c", NULL);
    char *contents = NULL;
    GError *error = NULL;

    g_assert_true(g_file_get_contents(helper_path, &contents, NULL, &error));
    g_assert_no_error(error);
    g_assert_null(g_strstr_len(contents, -1, "Synchronizing Windows media"));
    g_assert_null(g_strstr_len(contents, -1,
                              "Synchronizing Windows To Go media"));

    g_free(contents);
    g_free(helper_path);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/progress/layout/fat32", test_fat32_layouts);
    g_test_add_func("/progress/layout/ntfs", test_ntfs_layouts);
    g_test_add_func("/progress/layout/windows-to-go",
                    test_windows_to_go_mapping);
    g_test_add_func("/progress/status/visible", test_visible_statuses);
    g_test_add_func("/progress/status/forbidden", test_forbidden_windows_statuses);
    g_test_add_func("/progress/validation/enabled", test_validation_layout_enabled);
    g_test_add_func("/progress/validation/disabled", test_validation_layout_disabled);
    g_test_add_func("/progress/validation/bytes", test_byte_progress);
    return g_test_run();
}
