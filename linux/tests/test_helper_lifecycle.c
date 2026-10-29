/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_helper_entry
#include "../src/helper.c"
#undef main

static void wait_ready(const char *path)
{
    gint64 until = g_get_monotonic_time() + 2 * G_USEC_PER_SEC;
    while (access(path, F_OK) != 0 && g_get_monotonic_time() < until) usleep(1000);
    if (access(path, F_OK) != 0) _exit(77);
}

static void reap_runner(pid_t runner)
{
    int status = 0;
    g_assert_cmpint(wait_child(runner, &status), ==, runner);
    g_assert_true(WIFEXITED(status));
    g_assert_cmpint(WEXITSTATUS(status), ==, 0);
}

static void closed_output_cleanup(void)
{
    g_autofree char *dir = g_dir_make_tmp("tuxflash-disconnected-XXXXXX", NULL);
    g_autofree char *ready = g_build_filename(dir, "ready", NULL);
    g_autofree char *pidfile = g_build_filename(dir, "tool-pid", NULL);
    g_autofree char *gate = g_build_filename(dir, "gate", NULL);
    g_autofree char *cleanup = g_build_filename(dir, "umount", NULL);
    g_autofree char *released = g_build_filename(dir, "released", NULL);
    g_autofree char *qready = g_shell_quote(ready), *qpid = g_shell_quote(pidfile);
    g_autofree char *qgate = g_shell_quote(gate), *qreleased = g_shell_quote(released);
    g_autofree char *script = g_strdup_printf("printf '%%s' $$ > %s; printf ready > %s; read -r unused < %s", qpid, qready, qgate);
    g_autofree char *umount = g_strdup_printf("#!/bin/sh\nprintf released > %s\n", qreleased);
    g_assert_true(g_file_set_contents(cleanup, umount, -1, NULL));
    g_assert_cmpint(chmod(cleanup, 0700), ==, 0);
    g_assert_cmpint(mkfifo(gate, 0600), ==, 0);
    int gatekeeper = open(gate, O_RDWR | O_CLOEXEC);
    g_assert_cmpint(gatekeeper, >=, 0);
    int control[2], output[2];
    g_assert_cmpint(pipe(control), ==, 0); g_assert_cmpint(pipe(output), ==, 0);
    pid_t runner = fork(); g_assert_cmpint(runner, >=, 0);
    if (runner == 0) {
        alarm(6);
        close(control[1]); close(output[0]); close(gatekeeper);
        signal(SIGPIPE, SIG_DFL);
        if (dup2(output[1], STDOUT_FILENO) < 0 || install_signal_handlers() != 0) _exit(10);
        close(output[1]); cancelled = 0;
        if (start_cancellation_channel(control[0]) != 0) _exit(11);
        close(control[0]);
        char *command[] = {"/bin/sh", "-c", script, NULL};
        if (run_command_progress(command) != -1 || !cancelled || decompressor_pid != -1) _exit(12);
        /* A broken GUI pipe must not terminate the helper before its out/cleanup path. */
        event_message("LOG", "Cleaning up after frontend exit");
        g_setenv("PATH", dir, TRUE);
        char *unmount[] = {"umount", "--", "/test-only-no-mount", NULL};
        if (run_command_input(unmount, NULL) != 0) _exit(13);
        _exit(0);
    }
    close(control[0]); close(output[1]);
    wait_ready(ready);
    /* The frontend dies: its log reader and sole control writer disappear. */
    close(output[0]); close(control[1]);
    int status = 0;
    g_assert_cmpint(wait_child(runner, &status), ==, runner);
    g_autofree char *pidtext = NULL;
    g_assert_true(g_file_get_contents(pidfile, &pidtext, NULL, NULL));
    pid_t tool = (pid_t)g_ascii_strtoll(pidtext, NULL, 10);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) kill(tool, SIGKILL);
    g_assert_true(WIFEXITED(status)); g_assert_cmpint(WEXITSTATUS(status), ==, 0);
    g_assert_cmpint(kill(tool, 0), ==, -1); g_assert_cmpint(errno, ==, ESRCH);
    g_assert_true(g_file_test(released, G_FILE_TEST_IS_REGULAR));
    close(gatekeeper); unlink(ready); unlink(pidfile); unlink(gate); unlink(cleanup); unlink(released); rmdir(dir);
}

static void rejected_input_delivery(void)
{
    pid_t runner = fork(); g_assert_cmpint(runner, >=, 0);
    if (runner == 0) {
        alarm(5); signal(SIGPIPE, SIG_IGN);
        if (install_signal_handlers() != 0) _exit(10);
        cancelled = 0;
        char *input = g_malloc(8 * 1024 * 1024 + 1);
        memset(input, 'A', 8 * 1024 * 1024); input[8 * 1024 * 1024] = 0;
        char *command[] = {"/bin/sh", "-c", "exec 0<&-; exit 0", NULL};
        int result = run_command_input(command, input), saved = errno;
        g_free(input);
        _exit(result == -1 && saved == EPIPE && decompressor_pid == -1 ? 0 : 11);
    }
    reap_runner(runner);
}

static void cancelled_input_zero_exit(void)
{
    g_autofree char *dir = g_dir_make_tmp("tuxflash-input-cancel-XXXXXX", NULL);
    g_autofree char *ready = g_build_filename(dir, "ready", NULL);
    g_autofree char *gate = g_build_filename(dir, "gate", NULL);
    g_autofree char *qready = g_shell_quote(ready), *qgate = g_shell_quote(gate);
    g_autofree char *script = g_strdup_printf("trap 'exit 0' TERM; printf ready > %s; read -r unused < %s", qready, qgate);
    g_assert_cmpint(mkfifo(gate, 0600), ==, 0);
    int keeper = open(gate, O_RDWR | O_CLOEXEC); g_assert_cmpint(keeper, >=, 0);
    pid_t runner = fork(); g_assert_cmpint(runner, >=, 0);
    if (runner == 0) {
        alarm(6); signal(SIGPIPE, SIG_IGN);
        if (install_signal_handlers() != 0) _exit(10);
        cancelled = 0;
        pid_t sender = fork(); if (sender < 0) _exit(11);
        if (sender == 0) { wait_ready(ready); usleep(30000); kill(getppid(), SIGTERM); _exit(0); }
        char *input = g_malloc(8 * 1024 * 1024 + 1);
        memset(input, 'A', 8 * 1024 * 1024); input[8 * 1024 * 1024] = 0;
        char *command[] = {"/bin/sh", "-c", script, NULL};
        int result = run_command_input(command, input), saved = errno, status;
        g_free(input);
        if (wait_child(sender, &status) != sender) _exit(12);
        _exit(result == -1 && saved == ECANCELED && cancelled && decompressor_pid == -1 ? 0 : 13);
    }
    reap_runner(runner); close(keeper); unlink(ready); unlink(gate); rmdir(dir);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/lifecycle/frontend-exit-cleanup", closed_output_cleanup);
    g_test_add_func("/lifecycle/input-pipe-short", rejected_input_delivery);
    g_test_add_func("/lifecycle/cancel-input-zero-exit", cancelled_input_zero_exit);
    return g_test_run();
}
