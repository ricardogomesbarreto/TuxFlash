/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "windows_options.h"

#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

static void test_full_answer_file(void)
{
    LufusWindowsOptions options = {
        .flags = LUFUS_WINDOWS_BYPASS_REQUIREMENTS |
                 LUFUS_WINDOWS_NO_ONLINE_ACCOUNT |
                 LUFUS_WINDOWS_NO_DATA_COLLECTION |
                 LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT |
                 LUFUS_WINDOWS_DISABLE_BITLOCKER |
                 LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES |
                 LUFUS_WINDOWS_SILENT_INSTALL,
        .image_index = 4,
        .boot_media_partition = 1,
        .architecture = "amd64",
        .language = "en-GB",
        .local_account = "lufus-user",
    };
    GError *error = NULL;
    char *path = NULL;
    char *contents = NULL;
    int descriptor = g_file_open_tmp("lufus-unattend-test.XXXXXX", &path, &error);
    g_assert_no_error(error);
    g_assert_cmpint(descriptor, >=, 0);
    close(descriptor);
    g_assert_true(lufus_windows_write_unattend(path, &options, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_get_contents(path, &contents, NULL, &error));
    g_assert_no_error(error);
    g_assert_nonnull(strstr(contents, "BypassTPMCheck"));
    g_assert_nonnull(strstr(contents, "BypassSecureBootCheck"));
    g_assert_nonnull(strstr(contents, "BypassRAMCheck"));
    g_assert_nonnull(strstr(contents, "BypassNRO"));
    g_assert_nonnull(strstr(contents, "lufus-user"));
    g_assert_nonnull(strstr(contents, "PreventDeviceEncryption"));
    g_assert_nonnull(strstr(contents, "<SanPolicy>4</SanPolicy>"));
    g_assert_nonnull(strstr(contents, "<WillWipeDisk>true</WillWipeDisk>"));
    g_assert_nonnull(strstr(contents, "<PartitionID>1</PartitionID>\n"
                                      "              <Label>LUFUS_BOOT</Label>"));
    g_assert_nonnull(strstr(contents, "<Key>/IMAGE/INDEX</Key>"));
    g_assert_nonnull(strstr(contents, "<Value>4</Value>"));
    g_assert_nonnull(strstr(contents, "<InstallTo>"));
    g_assert_nonnull(strstr(contents, "<HideOnlineAccountScreens>true"));
    g_assert_nonnull(strstr(contents, "Microsoft-Windows-International-Core-WinPE"));
    g_assert_nonnull(strstr(contents, "<UILanguage>en-GB</UILanguage>"));
    g_assert_nonnull(strstr(contents, "<InputLocale>en-GB</InputLocale>"));
    g_unlink(path);
    g_free(contents);
    g_free(path);
}

static void test_silent_requires_image_index(void)
{
    LufusWindowsOptions options = {
        .flags = LUFUS_WINDOWS_SILENT_INSTALL,
        .architecture = "amd64",
    };
    GError *error = NULL;

    g_assert_false(lufus_windows_options_validate(&options, &error));
    g_assert_error(error, g_quark_from_static_string("lufus-windows-options-error"), 4);
    g_clear_error(&error);
}

static void test_regular_answer_never_wipes_install_disk(void)
{
    LufusWindowsOptions options = {
        .flags = LUFUS_WINDOWS_NO_DATA_COLLECTION,
        .image_index = 9,
        .architecture = "amd64",
    };
    GError *error = NULL;
    char *path = NULL;
    char *contents = NULL;
    int descriptor = g_file_open_tmp("lufus-regular-unattend-test.XXXXXX", &path, &error);

    g_assert_no_error(error);
    g_assert_cmpint(descriptor, >=, 0);
    close(descriptor);
    g_assert_true(lufus_windows_write_unattend(path, &options, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_get_contents(path, &contents, NULL, &error));
    g_assert_no_error(error);
    g_assert_null(strstr(contents, "WillWipeDisk"));
    g_assert_null(strstr(contents, "/IMAGE/INDEX"));
    g_unlink(path);
    g_free(contents);
    g_free(path);
}

static void test_reject_reserved_account(void)
{
    LufusWindowsOptions options = {
        .flags = LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT,
        .architecture = "amd64",
        .local_account = "Administrator",
    };
    GError *error = NULL;
    g_assert_false(lufus_windows_options_validate(&options, &error));
    g_assert_error(error, g_quark_from_static_string("lufus-windows-options-error"), 2);
    g_clear_error(&error);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/windows-options/full-answer-file", test_full_answer_file);
    g_test_add_func("/windows-options/reject-reserved-account", test_reject_reserved_account);
    g_test_add_func("/windows-options/silent-requires-image-index",
                    test_silent_requires_image_index);
    g_test_add_func("/windows-options/regular-answer-does-not-wipe",
                    test_regular_answer_never_wipes_install_disk);
    return g_test_run();
}
