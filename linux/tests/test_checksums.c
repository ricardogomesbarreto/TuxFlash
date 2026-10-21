/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "checksums.h"
#include <glib/gstdio.h>
#include <unistd.h>

static void test_vectors(void)
{
    const char *data[] = {"", "abc", "The quick brown fox jumps over the lazy dog"};
    const char *names[] = {"MD5", "SHA-1", "SHA-256", "SHA-512"};
    const GChecksumType types[] = {G_CHECKSUM_MD5, G_CHECKSUM_SHA1,
                                  G_CHECKSUM_SHA256, G_CHECKSUM_SHA512};
    for (guint i = 0; i < G_N_ELEMENTS(data); i++) {
        char *path = NULL;
        GError *error = NULL;
        int fd = g_file_open_tmp("lufus-checksum-XXXXXX", &path, &error);
        g_assert_no_error(error);
        g_assert_cmpint(fd, >=, 0);
        close(fd);
        g_assert_true(g_file_set_contents(path, data[i], -1, &error));
        g_assert_no_error(error);
        char *actual = lufus_checksums_file(path, NULL, &error);
        g_assert_no_error(error);
        GString *expected = g_string_new(NULL);
        for (guint j = 0; j < G_N_ELEMENTS(types); j++) {
            char *digest = g_compute_checksum_for_string(types[j], data[i], -1);
            g_string_append_printf(expected, "%s  %s\n", names[j], digest);
            g_free(digest);
        }
        g_assert_cmpstr(actual, ==, expected->str);
        g_string_free(expected, TRUE);
        g_free(actual);
        g_unlink(path);
        g_free(path);
    }
}

static void test_cancellation_and_nonregular(void)
{
    char *path = NULL;
    GError *error = NULL;
    int fd = g_file_open_tmp("lufus-cancel-XXXXXX", &path, &error);
    g_assert_no_error(error);
    g_assert_cmpint(fd, >=, 0);
    close(fd);
    GCancellable *cancellable = g_cancellable_new();
    g_cancellable_cancel(cancellable);
    g_assert_null(lufus_checksums_file(path, cancellable, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    g_assert_null(lufus_checksums_file("/dev/null", NULL, &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_object_unref(cancellable);
    g_unlink(path);
    g_free(path);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/checksums/original-rufus-vectors", test_vectors);
    g_test_add_func("/checksums/cancellation-and-nonregular", test_cancellation_and_nonregular);
    return g_test_run();
}
