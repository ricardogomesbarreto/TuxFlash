#include <glib/gstdio.h>
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "job.h"

#include <glib.h>

static gboolean strv_contains(char **values, const char *needle)
{
    guint index;

    for (index = 0; values[index] != NULL; index++) {
        if (g_strcmp0(values[index], needle) == 0)
            return TRUE;
    }
    return FALSE;
}

static void test_runtime_validation_policy(void)
{
    g_assert_true(lufus_job_supports_runtime_validation(
        LUFUS_JOB_WINDOWS_INSTALLER));
    g_assert_true(lufus_job_supports_runtime_validation(
        LUFUS_JOB_LINUX_PERSISTENT));
    g_assert_true(lufus_job_runtime_validation_default(
        LUFUS_JOB_WINDOWS_INSTALLER));
    g_assert_true(lufus_job_runtime_validation_default(
        LUFUS_JOB_LINUX_PERSISTENT));

    g_assert_false(lufus_job_supports_runtime_validation(LUFUS_JOB_RAW_WRITE));
    g_assert_false(lufus_job_supports_runtime_validation(
        LUFUS_JOB_WINDOWS_TO_GO));
    g_assert_false(lufus_job_supports_runtime_validation(LUFUS_JOB_DEVICE_TEST));
    g_assert_false(lufus_job_supports_runtime_validation(LUFUS_JOB_CAPTURE));
    g_assert_false(lufus_job_runtime_validation_default(LUFUS_JOB_RAW_WRITE));
    g_assert_false(lufus_job_runtime_validation_default(
        LUFUS_JOB_WINDOWS_TO_GO));
    g_assert_false(lufus_job_runtime_validation_default(LUFUS_JOB_DEVICE_TEST));
    g_assert_false(lufus_job_runtime_validation_default(LUFUS_JOB_CAPTURE));
}

static void assert_spawn_validation_flag(LufusJobType type,
                                         gboolean validation,
                                         gboolean expected)
{
    LufusJob job = {
        .type = type,
        .image_path = (char *)"/tmp/test.iso",
        .device_path = (char *)"/dev/test-disk",
        .major_minor = (char *)"240:1",
        .device_size = 8ULL * 1024ULL * 1024ULL * 1024ULL,
        .runtime_validation = validation,
        .disk_sequence = 12345,
        .persistence_size = 512ULL * 1024ULL * 1024ULL,
        .windows = {
            .architecture = (char *)"amd64",
        },
    };
    GError *error = NULL;
    GSubprocess *process;
    char *standard_output = NULL;
    char **arguments;

    process = lufus_job_spawn(&job, "/bin/echo", FALSE, &error);
    g_assert_no_error(error);
    g_assert_nonnull(process);
    g_assert_true(g_subprocess_communicate_utf8(process, NULL, NULL,
                                                &standard_output, NULL,
                                                &error));
    g_assert_no_error(error);
    g_assert_true(g_subprocess_get_successful(process));
    g_assert_nonnull(strstr(standard_output, "--disk-seq 12345"));
    g_strchomp(standard_output);
    arguments = g_strsplit(standard_output, " ", -1);
    g_assert_cmpint(strv_contains(arguments, "--runtime-validation"), ==,
                    expected);

    g_strfreev(arguments);
    g_free(standard_output);
    g_object_unref(process);
}

static void test_runtime_validation_helper_argument(void)
{
    assert_spawn_validation_flag(LUFUS_JOB_WINDOWS_INSTALLER, TRUE, TRUE);
    assert_spawn_validation_flag(LUFUS_JOB_WINDOWS_INSTALLER, FALSE, FALSE);
    assert_spawn_validation_flag(LUFUS_JOB_LINUX_PERSISTENT, TRUE, TRUE);
    assert_spawn_validation_flag(LUFUS_JOB_LINUX_PERSISTENT, FALSE, FALSE);
}

static void test_silent_install_helper_arguments(void)
{
    LufusJob job = {
        .type = LUFUS_JOB_WINDOWS_INSTALLER,
        .image_path = (char *)"/tmp/test.iso",
        .device_path = (char *)"/dev/test-disk",
        .major_minor = (char *)"240:1",
        .device_size = 8ULL * 1024ULL * 1024ULL * 1024ULL,
        .windows_index = 7,
        .windows = {
            .flags = LUFUS_WINDOWS_SILENT_INSTALL,
            .image_index = 7,
            .architecture = (char *)"amd64",
        },
    };
    GError *error = NULL;
    GSubprocess *process;
    char *standard_output = NULL;

    process = lufus_job_spawn(&job, "/bin/echo", FALSE, &error);
    g_assert_no_error(error);
    g_assert_nonnull(process);
    g_assert_true(g_subprocess_communicate_utf8(process, NULL, NULL,
                                                &standard_output, NULL,
                                                &error));
    g_assert_no_error(error);
    g_assert_nonnull(strstr(standard_output, "--windows-flags 64"));
    g_assert_nonnull(strstr(standard_output, "--windows-index 7"));
    g_free(standard_output);
    g_object_unref(process);
}

static void test_silent_install_mode_policy(void)
{
    LufusWindowsOptions options = {
        .flags = LUFUS_WINDOWS_SILENT_INSTALL,
        .image_index = 1,
        .architecture = (char *)"amd64",
    };
    GError *error = NULL;
    LufusJob *job;

    job = lufus_job_new_windows_installer("/does/not/exist.iso", "/dev/missing",
                                          FALSE, LUFUS_WINDOWS_TARGET_BIOS,
                                          FALSE, &options, &error);
    g_assert_null(job);
    g_assert_nonnull(error);
    g_assert_nonnull(strstr(error->message, "requires UEFI"));
    g_clear_error(&error);

    job = lufus_job_new_windows_to_go("/does/not/exist.iso", "/dev/missing",
                                      1, &options, &error);
    g_assert_null(job);
    g_assert_nonnull(error);
    g_assert_nonnull(strstr(error->message, "installer media"));
    g_clear_error(&error);
}

static void test_host_authentication_environment(void)
{
    GError *error = NULL;
    char *dir = g_dir_make_tmp("tuxflash-pkexec-test-XXXXXX", &error);
    g_assert_no_error(error);
    char *program = g_build_filename(dir, "pkexec", NULL);
    const char *script = "#!/bin/sh\nprintf '%s|%s' \"${GIO_MODULE_DIR-UNSET}\" \"${LD_LIBRARY_PATH-UNSET}\"\n";
    g_assert_true(g_file_set_contents(program, script, -1, &error));
    g_assert_no_error(error);
    g_chmod(program, 0700);
    char *old_path = g_strdup(g_getenv("PATH"));
    char *old_gio = g_strdup(g_getenv("GIO_MODULE_DIR"));
    char *old_ld = g_strdup(g_getenv("LD_LIBRARY_PATH"));
    g_setenv("PATH", dir, TRUE);
    g_setenv("GIO_MODULE_DIR", "not-a-host-module", TRUE);
    g_setenv("LD_LIBRARY_PATH", "not-a-host-library", TRUE);
    LufusJob job = {0};
    job.type = LUFUS_JOB_RAW_WRITE;
    job.device_path = "/dev/test-not-opened";
    job.major_minor = "1:1";
    job.device_size = 1024;
    GSubprocess *process = lufus_job_spawn(&job, "/bin/echo", TRUE, &error);
    g_assert_no_error(error);
    char *output = NULL;
    g_assert_true(g_subprocess_communicate_utf8(process, NULL, NULL, &output, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(output, ==, "UNSET|UNSET");
    g_object_unref(process);
    g_setenv("PATH", old_path, TRUE);
    if (old_gio) g_setenv("GIO_MODULE_DIR", old_gio, TRUE); else g_unsetenv("GIO_MODULE_DIR");
    if (old_ld) g_setenv("LD_LIBRARY_PATH", old_ld, TRUE); else g_unsetenv("LD_LIBRARY_PATH");
    g_unlink(program); g_rmdir(dir);
    g_free(output); g_free(old_path); g_free(old_gio); g_free(old_ld); g_free(program); g_free(dir);
}

static void test_private_cancellation_pipe(void)
{
    g_autofree char *dir = g_dir_make_tmp("tuxflash-control-XXXXXX", NULL);
    g_autofree char *helper = g_build_filename(dir, "helper", NULL);
    g_assert_true(g_file_set_contents(helper, "#!/bin/sh\nread -r command\nprintf CONTROL_EOF\nexit 7\n", -1, NULL));
    g_assert_cmpint(g_chmod(helper, 0700), ==, 0);
    LufusJob job = {.type=LUFUS_JOB_RAW_WRITE, .image_path="/test/image.img",
                    .device_path="/dev/test", .major_minor="1:1", .device_size=4096};
    GError *error = NULL;
    GSubprocess *process = lufus_job_spawn(&job, helper, FALSE, &error);
    g_assert_no_error(error); g_assert_nonnull(process);
    g_assert_nonnull(g_subprocess_get_stdin_pipe(process));
    g_assert_true(lufus_job_cancel(process, &error)); g_assert_no_error(error);
    char output[64] = {0}; gsize count = 0;
    g_assert_true(g_input_stream_read_all(g_subprocess_get_stdout_pipe(process), output,
                                        sizeof(output) - 1, &count, NULL, &error));
    g_assert_no_error(error); g_assert_cmpstr(output, ==, "CONTROL_EOF");
    g_assert_true(g_subprocess_wait(process, NULL, &error)); g_assert_no_error(error);
    g_assert_cmpint(g_subprocess_get_exit_status(process), ==, 7);
    g_object_unref(process); g_unlink(helper); g_rmdir(dir);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/job/host-authentication-environment", test_host_authentication_environment);
    g_test_add_func("/job/runtime-validation-policy",
                    test_runtime_validation_policy);
    g_test_add_func("/job/runtime-validation-helper-argument",
                    test_runtime_validation_helper_argument);
    g_test_add_func("/job/silent-install-helper-arguments",
                    test_silent_install_helper_arguments);
    g_test_add_func("/job/silent-install-mode-policy",
                    test_silent_install_mode_policy);
    g_test_add_func("/job/private-cancellation-pipe", test_private_cancellation_pipe);
    return g_test_run();
}
