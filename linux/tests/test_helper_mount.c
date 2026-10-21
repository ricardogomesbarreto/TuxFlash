/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_helper_entry
#include "../src/helper.c"
#undef main

static void check_mount(bool ntfs, gboolean kernel_ok, gboolean fuse_ok, int expected)
{
    GError *error = NULL;
    char *dir = g_dir_make_tmp("tuxflash-mount-test-XXXXXX", &error);
    g_assert_no_error(error);
    char *mount = g_build_filename(dir, "mount", NULL);
    char *fuse = g_build_filename(dir, "ntfs-3g", NULL);
    char *log = g_build_filename(dir, "calls", NULL);
    const char *mount_script = "#!/bin/sh\nprintf 'mount %s\\n' \"$*\" >> \"$TUXFLASH_MOUNT_TEST_LOG\"\nexit \"$TUXFLASH_MOUNT_TEST_KERNEL\"\n";
    const char *fuse_script = "#!/bin/sh\nprintf 'fuse %s\\n' \"$*\" >> \"$TUXFLASH_MOUNT_TEST_LOG\"\nexit \"$TUXFLASH_MOUNT_TEST_FUSE\"\n";
    g_assert_true(g_file_set_contents(mount, mount_script, -1, &error));
    g_assert_true(g_file_set_contents(fuse, fuse_script, -1, &error));
    g_assert_no_error(error);
    chmod(mount, 0700); chmod(fuse, 0700);
    char *old_path = g_strdup(g_getenv("PATH"));
    g_setenv("PATH", dir, TRUE);
    g_setenv("TUXFLASH_MOUNT_TEST_LOG", log, TRUE);
    g_setenv("TUXFLASH_MOUNT_TEST_KERNEL", kernel_ok ? "0" : "32", TRUE);
    g_setenv("TUXFLASH_MOUNT_TEST_FUSE", fuse_ok ? "0" : "1", TRUE);
    g_assert_cmpint(mount_partition_once("/dev/test-only", "/test-only", ntfs), ==, expected);
    char *calls = NULL;
    g_assert_true(g_file_get_contents(log, &calls, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpint(strstr(calls, "fuse ") != NULL, ==, ntfs && !kernel_ok);
    if (ntfs) g_assert_nonnull(strstr(calls, "-t ntfs3 --"));
    else g_assert_null(strstr(calls, "ntfs3"));
    g_setenv("PATH", old_path, TRUE);
    g_unsetenv("TUXFLASH_MOUNT_TEST_LOG"); g_unsetenv("TUXFLASH_MOUNT_TEST_KERNEL"); g_unsetenv("TUXFLASH_MOUNT_TEST_FUSE");
    unlink(mount); unlink(fuse); unlink(log); rmdir(dir);
    g_free(calls); g_free(old_path); g_free(mount); g_free(fuse); g_free(log); g_free(dir);
}
static void kernel_mount(void) { check_mount(true, TRUE, FALSE, 0); }
static void fuse_fallback(void) { check_mount(true, FALSE, TRUE, 0); }
static void both_fail(void) { check_mount(true, FALSE, FALSE, -1); }
static void fat_no_ntfs(void) { check_mount(false, TRUE, FALSE, 0); }
static void test_regular_target_rejected(void)
{
    g_autofree char *path = NULL;
    int fd = g_file_open_tmp("tuxflash-target-XXXXXX", &path, NULL);
    g_assert_cmpint(fd, >=, 0);
    TargetInfo target = {0};
    g_assert_cmpint(target_descriptor_matches(&target, fd), ==, -1);
    close(fd); unlink(path);
}
static void test_interrupted_wait_reaps_child(void)
{
    struct sigaction action = {0}, previous;
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    g_assert_cmpint(sigaction(SIGUSR1, &action, &previous), ==, 0);
    cancelled = 0;
    pid_t child = fork(); g_assert_cmpint(child, >=, 0);
    if (!child) { usleep(30000); kill(getppid(), SIGUSR1); usleep(30000); _exit(7); }
    int status = 0;
    g_assert_cmpint(wait_child(child, &status), ==, child);
    g_assert_true(WIFEXITED(status)); g_assert_cmpint(WEXITSTATUS(status), ==, 7);
    g_assert_cmpint(waitpid(child, &status, WNOHANG), ==, -1); g_assert_cmpint(errno, ==, ECHILD);
    sigaction(SIGUSR1, &previous, NULL); cancelled = 0;
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/safety/regular-file-is-not-device", test_regular_target_rejected);
    g_test_add_func("/safety/interrupted-wait-reaps-child", test_interrupted_wait_reaps_child);
    g_test_add_func("/mount/ntfs3", kernel_mount);
    g_test_add_func("/mount/ntfs3g-fallback", fuse_fallback);
    g_test_add_func("/mount/both-fail", both_fail);
    g_test_add_func("/mount/fat-does-not-use-ntfs", fat_no_ntfs);
    return g_test_run();
}
