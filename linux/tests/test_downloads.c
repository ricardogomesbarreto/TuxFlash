/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "downloads.h"

#include <glib.h>

static void test_release_catalog(void)
{
    g_assert_nonnull(lufus_windows_release_name("windows11"));
    g_assert_nonnull(lufus_windows_release_name("windows10"));
    g_assert_null(lufus_windows_release_name("windows95"));
    g_assert_cmpstr(lufus_windows_release_download_page("windows11"), ==,
                    "https://www.microsoft.com/software-download/windows11");
    g_assert_cmpstr(lufus_windows_release_download_page("windows10"), ==,
                    "https://www.microsoft.com/software-download/windows10ISO");
    g_assert_null(lufus_windows_release_download_page("windows95"));
}

static void test_invalid_release_fails_without_network(void)
{
    GError *error = NULL;
    LufusWindowsCatalog *catalog =
        lufus_windows_catalog_discover("invalid", "en-US", NULL, &error);
    g_assert_null(catalog);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
}

static void test_unsupported_architecture_fails_without_network(void)
{
    GError *error = NULL;
    LufusWindowsCatalog *catalog =
        lufus_windows_catalog_discover_for_architecture(
            "windows11", "en-US", "x86", NULL, &error);
    g_assert_null(catalog);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
    g_assert_nonnull(g_strstr_len(error->message, -1, "x86"));
    g_clear_error(&error);
}

static void test_sentinel_error_versions(void)
{
    g_assert_true(lufus_windows_service_error_is_sentinel(
        8, "ErrorSettings.SentinelReject",
        "Sentinel marked this request as rejected."));
    g_assert_true(lufus_windows_service_error_is_sentinel(
        9, NULL, "Microsoft rejected the request"));
    g_assert_true(lufus_windows_service_error_is_sentinel(
        0, NULL, "Sentinel marked this request as rejected."));
    g_assert_false(lufus_windows_service_error_is_sentinel(
        8, "ErrorSettings.Other", "A different Microsoft service error"));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/downloads/releases", test_release_catalog);
    g_test_add_func("/downloads/invalid-release", test_invalid_release_fails_without_network);
    g_test_add_func("/downloads/unsupported-architecture",
                    test_unsupported_architecture_fails_without_network);
    g_test_add_func("/downloads/sentinel-errors", test_sentinel_error_versions);
    return g_test_run();
}
