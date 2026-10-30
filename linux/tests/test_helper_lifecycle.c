/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_helper_entry
#include "../src/helper.c"
#undef main
#include <sys/prctl.h>

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


/* A real exec fixture, not a shell whose TERM behavior changes across distros. */
static int tool_fixture(const char *mode)
{
    const char *directory = g_getenv("TUXFLASH_TEST_CHILD_DIR");
    g_autofree char *pids = g_build_filename(directory, "pids", NULL);
    g_autofree char *ready = g_build_filename(directory, "ready", NULL);
    pid_t leaf = 0;
    if (strcmp(mode, "tree") == 0 || strcmp(mode, "daemon") == 0 || strcmp(mode, "failed-service") == 0 || strcmp(mode, "failed-progress") == 0) {
        int started[2];
        if (pipe(started) != 0) return 125;
        leaf = fork();
        if (leaf < 0) return 125;
        if (leaf == 0) {
            close(started[0]); signal(SIGTERM, SIG_IGN);
            if (strcmp(mode, "daemon") == 0 || strcmp(mode, "failed-service") == 0) { close(0); close(1); close(2); }
            if (write(started[1], "R", 1) != 1) _exit(125);
            close(started[1]);
            for (;;) pause();
        }
        close(started[1]); char value;
        if (read(started[0], &value, 1) != 1) return 125;
        close(started[0]); signal(SIGTERM, SIG_DFL);
    } else signal(SIGTERM, SIG_IGN);
    g_autofree char *text = g_strdup_printf("%ld %ld\n", (long)getpid(), (long)leaf);
    if (!g_file_set_contents(pids, text, -1, NULL) ||
        !g_file_set_contents(ready, "ready", -1, NULL)) return 125;
    if (strcmp(mode, "daemon") == 0) return 0;
    if (strcmp(mode, "failed-service") == 0 || strcmp(mode, "failed-progress") == 0) return 7;
    for (;;) pause();
}

static void reap_owned_fixture(pid_t pid)
{
    if (pid <= 1) return;
    siginfo_t info = {0};
    /* Never signal a PID that is no longer one of this fixture's children. */
    if (waitid(P_PID, pid, &info, WEXITED | WNOHANG | WNOWAIT) != 0) return;
    if (info.si_pid == 0) kill(pid, SIGKILL);
    wait_child(pid, NULL);
}

static void managed_tool_case(const char *mode)
{
    g_assert_cmpint(prctl(PR_SET_CHILD_SUBREAPER, 1), ==, 0);
    g_autofree char *dir = g_dir_make_tmp("tuxflash-tool-group-XXXXXX", NULL);
    g_autofree char *pids = g_build_filename(dir, "pids", NULL);
    g_autofree char *ready = g_build_filename(dir, "ready", NULL);
    g_autofree char *outsider = g_build_filename(dir, "outsider", NULL);
    char executable[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable)-1);
    g_assert_cmpint(n, >, 0); executable[n] = 0;
    pid_t runner = fork(); g_assert_cmpint(runner, >=, 0);
    if (runner == 0) {
        alarm(9); cancelled = 0; decompressor_pid = -1;
        if (prctl(PR_SET_CHILD_SUBREAPER, 1) != 0 || install_signal_handlers() != 0) _exit(10);
        g_setenv("TUXFLASH_TEST_CHILD_DIR", dir, TRUE);
        pid_t unrelated = fork();
        if (unrelated < 0) _exit(11);
        if (unrelated == 0) { signal(SIGTERM, SIG_DFL); for (;;) pause(); }
        g_autofree char *outside_pid = g_strdup_printf("%ld", (long)unrelated);
        if (!g_file_set_contents(outsider, outside_pid, -1, NULL)) _exit(11);
        char *args[] = {executable, "--tool-fixture", (char *)mode, NULL};
        int result, saved;
        if (strcmp(mode, "decoder") == 0) {
            int source = open("/dev/null", O_RDONLY | O_CLOEXEC);
            int stream = open_decompressor(executable, source); close(source);
            if (stream < 0) _exit(12);
            char byte;
            while (read(stream, &byte, 1) < 0 && errno == EINTR && !cancelled) {}
            close(stream); result = wait_for_decompressor(); saved = errno;
        } else if (strcmp(mode, "input") == 0) {
            char *input = g_malloc(8 * 1024 * 1024 + 1);
            memset(input, 'A', 8 * 1024 * 1024); input[8 * 1024 * 1024] = 0;
            result = run_command_input(args, input); saved = errno; g_free(input);
        } else if (strcmp(mode, "daemon") == 0 || strcmp(mode, "failed-service") == 0) {
            result = run_command_input(args, NULL); saved = errno;
        } else { result = run_command_progress(args); saved = errno; }
        gboolean good = strcmp(mode, "daemon") == 0 ? result == 0 :
            g_str_has_prefix(mode, "failed-") ? result == -1 && !cancelled && saved == EIO :
            result == -1 && cancelled && saved == ECANCELED;
        good = good && decompressor_pid == -1 && kill(unrelated, 0) == 0;
        g_autofree char *record = NULL;
        if (!g_file_get_contents(pids, &record, NULL, NULL)) _exit(13);
        long leader, leaf; if (sscanf(record, "%ld %ld", &leader, &leaf) != 2) _exit(14);
        if (leaf > 1) {
            int status = 0;
            pid_t done = waitpid((pid_t)leaf, &status, WNOHANG);
            if (strcmp(mode, "daemon") == 0) good = good && done == 0 && kill((pid_t)leaf, 0) == 0;
            else {
                /* KILL has been issued, but the child may need one scheduling turn. */
                gint64 until = g_get_monotonic_time() + 500000;
                while (done == 0 && g_get_monotonic_time() < until) { usleep(1000); done=waitpid((pid_t)leaf,&status,WNOHANG); }
                good = good && done == leaf && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
            }
            if (done == 0) reap_owned_fixture((pid_t)leaf);
        }
        reap_owned_fixture(unrelated); _exit(good ? 0 : 15);
    }
    wait_ready(ready);
    if (strcmp(mode, "daemon") != 0 && !g_str_has_prefix(mode, "failed-")) g_assert_cmpint(kill(runner, SIGTERM), ==, 0);
    gint64 until = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    int status = 0; pid_t done;
    do { done=waitpid(runner,&status,WNOHANG); if(done==0)usleep(1000); }
    while (done==0 && g_get_monotonic_time()<until);
    gboolean timeout = done==0;
    if (timeout) { kill(runner,SIGKILL); wait_child(runner,&status); }
    g_autofree char *record = NULL;
    if (g_file_get_contents(pids, &record, NULL, NULL)) {
        long leader, leaf;
        if (sscanf(record, "%ld %ld", &leader, &leaf) == 2) { reap_owned_fixture((pid_t)leader); reap_owned_fixture((pid_t)leaf); }
    }
    g_autofree char *outside_pid = NULL;
    if (g_file_get_contents(outsider, &outside_pid, NULL, NULL)) reap_owned_fixture((pid_t)g_ascii_strtoll(outside_pid, NULL, 10));
    unlink(outsider);
    /* Reap an unrelated fixture orphan if the pre-fix runner was force-stopped. */
    int orphan;
    while ((orphan=waitpid(-1,NULL,WNOHANG)) > 0) {}
    unlink(pids); unlink(ready); rmdir(dir);
    g_assert_false(timeout); g_assert_true(WIFEXITED(status)); g_assert_cmpint(WEXITSTATUS(status),==,0);
}
static void failed_progress_child(void) { managed_tool_case("failed-progress"); }
static void failed_background_service(void) { managed_tool_case("failed-service"); }
static void inherited_sigchld_ignored(void)
{
    struct sigaction previous;
    g_assert_cmpint(sigaction(SIGCHLD, NULL, &previous), ==, 0);
    signal(SIGCHLD, SIG_IGN);
    g_assert_cmpint(install_signal_handlers(), ==, 0);
    cancelled = 0;
    char *args[] = {"/bin/sh", "-c", "exit 0", NULL};
    int result = run_command_input(args, NULL);
    sigaction(SIGCHLD, &previous, NULL);
    g_assert_cmpint(result, ==, 0);
    g_assert_cmpint(decompressor_pid, ==, -1);
}
static void failed_tool_errno(void)
{
    cancelled = 0;
    char *args[] = {"/bin/sh", "-c", "exit 7", NULL};
    errno = 0;
    g_assert_cmpint(run_command_progress(args), ==, -1);
    g_assert_cmpint(errno, ==, EIO);
    g_assert_cmpint(decompressor_pid, ==, -1);
}
static void stubborn_progress(void) { managed_tool_case("progress"); }
static void stubborn_input(void) { managed_tool_case("input"); }
static void stubborn_decoder(void) { managed_tool_case("decoder"); }
static void cancelled_process_tree(void) { managed_tool_case("tree"); }
static void successful_background_service(void) { managed_tool_case("daemon"); }

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--tool-fixture") == 0) return tool_fixture(argv[2]);
    if (argc == 2 && strcmp(argv[1], "-dc") == 0) return tool_fixture("decoder");
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/lifecycle/frontend-exit-cleanup", closed_output_cleanup);
    g_test_add_func("/lifecycle/input-pipe-short", rejected_input_delivery);
    g_test_add_func("/lifecycle/cancel-input-zero-exit", cancelled_input_zero_exit);
    g_test_add_func("/lifecycle/stubborn-progress", stubborn_progress);
    g_test_add_func("/lifecycle/stubborn-input", stubborn_input);
    g_test_add_func("/lifecycle/stubborn-decoder", stubborn_decoder);
    g_test_add_func("/lifecycle/cancel-process-group", cancelled_process_tree);
    g_test_add_func("/lifecycle/preserve-successful-service", successful_background_service);
    g_test_add_func("/lifecycle/failed-tool-errno", failed_tool_errno);
    g_test_add_func("/lifecycle/stop-failed-service", failed_background_service);
    g_test_add_func("/lifecycle/stop-failed-progress", failed_progress_child);
    g_test_add_func("/lifecycle/inherited-sigchld-ignore", inherited_sigchld_ignored);
    return g_test_run();
}
