/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_helper_entry
#include "../src/helper.c"
#undef main

static void reject_sources(void)
{
    alarm(10);
    g_autofree char *dir = g_dir_make_tmp("tuxflash-source-XXXXXX", NULL);
    g_autofree char *regular = g_build_filename(dir, "original", NULL);
    g_assert_true(g_file_set_contents(regular, "data", 4, NULL));
    const char *names[] = {"link.img", "link.gz", "link.xz", "link.bz2", "link.zst", "link.zstd"};
    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        g_autofree char *link = g_build_filename(dir, names[i], NULL);
        g_assert_cmpint(symlink(regular, link), ==, 0);
        uint64_t total = 0;
        g_assert_cmpint(open_source(link, &total), ==, -1);
        g_assert_cmpint(errno, ==, ELOOP);
        unlink(link);
    }
    g_autofree char *fifo = g_build_filename(dir, "pipe.gz", NULL);
    g_assert_cmpint(mkfifo(fifo, 0600), ==, 0);
    uint64_t total;
    g_assert_cmpint(open_source(fifo, &total), ==, -1);
    g_assert_cmpint(open_source(dir, &total), ==, -1);
    g_assert_cmpint(truncate(regular, 0), ==, 0);
    g_assert_cmpint(open_source(regular, &total), ==, -1);
    unlink(fifo); unlink(regular); rmdir(dir);
    alarm(0);
}

typedef struct { const char *tool, *suffix; } Codec;
static const Codec codecs[] = {{"gzip", "gz"}, {"xz", "xz"}, {"bzip2", "bz2"}, {"zstd", "zst"}};

static void pinned_decoder(gconstpointer data)
{
    const Codec *codec = data;
    alarm(15);
    g_autofree char *dir = g_dir_make_tmp("tuxflash-pinned-XXXXXX", NULL);
    g_autofree char *image = g_strdup_printf("%s/source.%s", dir, codec->suffix);
    g_autofree char *moved = g_build_filename(dir, "original-moved", NULL);
    g_autofree char *marker = g_build_filename(dir, "ready", NULL);
    g_autofree char *gate = g_build_filename(dir, "gate", NULL);
    g_autofree char *tool = g_find_program_in_path(codec->tool);
    g_assert_nonnull(tool);
    const char payload[] = "the selected image stays pinned\n";
    GSubprocess *encoder = g_subprocess_new(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                          NULL, tool, "-c", NULL);
    g_assert_nonnull(encoder);
    GBytes *input = g_bytes_new_static(payload, sizeof(payload) - 1), *encoded = NULL;
    g_assert_true(g_subprocess_communicate(encoder, input, NULL, &encoded, NULL, NULL));
    g_assert_true(g_subprocess_get_successful(encoder));
    gsize length; const char *bytes = g_bytes_get_data(encoded, &length);
    g_assert_true(g_file_set_contents(image, bytes, length, NULL));
    g_bytes_unref(input); g_bytes_unref(encoded); g_object_unref(encoder);
    g_autofree char *wrapper = g_build_filename(dir, codec->tool, NULL);
    g_autofree char *quoted_tool = g_shell_quote(tool);
    g_autofree char *quoted_gate = g_shell_quote(gate);
    g_autofree char *quoted_marker = g_shell_quote(marker);
    g_autofree char *script = g_strdup_printf("#!/bin/sh\nprintf ready > %s\nread -r release < %s\nexec %s \"$@\"\n",
                                             quoted_marker, quoted_gate, quoted_tool);
    g_assert_true(g_file_set_contents(wrapper, script, -1, NULL));
    g_assert_cmpint(chmod(wrapper, 0700), ==, 0);
    g_assert_cmpint(mkfifo(gate, 0600), ==, 0);
    g_autofree char *old_path = g_strdup(g_getenv("PATH"));
    g_autofree char *new_path = g_strdup_printf("%s:%s", dir, old_path);
    g_setenv("PATH", new_path, TRUE);
    cancelled = 0;
    uint64_t total;
    int stream = open_source(image, &total);
    g_setenv("PATH", old_path, TRUE);
    g_assert_cmpint(stream, >=, 0);
    gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (access(marker, F_OK) != 0 && g_get_monotonic_time() < deadline) g_usleep(1000);
    g_assert_cmpint(access(marker, F_OK), ==, 0);
    g_assert_cmpint(rename(image, moved), ==, 0);
    g_assert_true(g_file_set_contents(image, "not the selected image", -1, NULL));
    int release = open(gate, O_WRONLY | O_CLOEXEC);
    g_assert_cmpint(release, >=, 0);
    g_assert_cmpint(write(release, "go\n", 3), ==, 3); close(release);
    GString *actual = g_string_new(NULL); char buffer[128]; ssize_t count;
    while ((count = read(stream, buffer, sizeof(buffer))) > 0) g_string_append_len(actual, buffer, count);
    g_assert_cmpint(count, ==, 0); close(stream);
    g_assert_cmpint(wait_for_decompressor(), ==, 0);
    g_assert_cmpstr(actual->str, ==, payload); g_string_free(actual, TRUE);
    unlink(image); unlink(moved); unlink(marker); unlink(gate); unlink(wrapper); rmdir(dir);
    alarm(0);
}

static void cancelled_write(void)
{
    alarm(5);
    int channel[2]; g_assert_cmpint(pipe(channel), ==, 0);
    int flags = fcntl(channel[1], F_GETFL);
    g_assert_cmpint(fcntl(channel[1], F_SETFL, flags | O_NONBLOCK), ==, 0);
    unsigned char buffer[4096] = {0};
    while (write(channel[1], buffer, sizeof(buffer)) > 0) {}
    g_assert_cmpint(errno, ==, EAGAIN);
    g_assert_cmpint(fcntl(channel[1], F_SETFL, flags), ==, 0);
    struct sigaction previous_int, previous_term;
    sigaction(SIGINT, NULL, &previous_int); sigaction(SIGTERM, NULL, &previous_term);
    g_assert_cmpint(install_signal_handlers(), ==, 0); cancelled = 0;
    pid_t child = fork(); g_assert_cmpint(child, >=, 0);
    if (!child) { usleep(30000); kill(getppid(), SIGTERM); _exit(0); }
    int result = write_all(channel[1], buffer, sizeof(buffer)), saved = errno, status;
    g_assert_cmpint(wait_child(child, &status), ==, child);
    sigaction(SIGINT, &previous_int, NULL); sigaction(SIGTERM, &previous_term, NULL);
    close(channel[0]); close(channel[1]); cancelled = 0;
    g_assert_cmpint(result, ==, -1); g_assert_cmpint(saved, ==, ECANCELED);
    alarm(0);
}

static void capture_descriptor(gconstpointer data)
{
    const char *format = data;
    alarm(20);
    g_autofree char *tool = g_find_program_in_path("qemu-img");
    if (!tool) { g_test_skip("qemu-img not installed"); alarm(0); return; }
    g_autofree char *dir = g_dir_make_tmp("tuxflash-capture-XXXXXX", NULL);
    g_autofree char *source = g_build_filename(dir, "source", NULL);
    g_autofree char *output = g_build_filename(dir, "capture", NULL);
    g_autofree char *moved = g_build_filename(dir, "reserved-inode", NULL);
    g_autofree char *victim = g_build_filename(dir, "existing-file", NULL);
    g_assert_true(g_file_set_contents(victim, "keep me", -1, NULL));
    int input = open(source, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    int target = open(output, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    g_assert_cmpint(input, >=, 0); g_assert_cmpint(target, >=, 0);
    const uint64_t capacity = 4U * 1024U * 1024U;
    g_assert_cmpint(ftruncate(input, capacity), ==, 0);
    g_assert_cmpint(pwrite(input, "payload", 7, capacity - 512), ==, 7);
    g_assert_cmpint(capture_output_matches(output, target), ==, 0);
    g_assert_cmpint(rename(output, moved), ==, 0);
    g_assert_cmpint(symlink(victim, output), ==, 0);
    cancelled = 0;
    g_assert_cmpint(capture_virtual_image(input, target, capacity, format), ==, 0);
    g_assert_cmpint(capture_output_matches(output, target), ==, -1);
    g_assert_cmpint(errno, ==, ESTALE);
    g_autofree char *unchanged = NULL;
    g_assert_true(g_file_get_contents(victim, &unchanged, NULL, NULL));
    g_assert_cmpstr(unchanged, ==, "keep me");
    const char *kind = strcmp(format, "vhd") == 0 ? "vpc" : "vhdx";
    GSubprocess *compare = g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, NULL, tool, "compare",
                                          "-f", kind, "-F", "raw", moved, source, NULL);
    g_assert_nonnull(compare); g_assert_true(g_subprocess_wait_check(compare, NULL, NULL));
    g_object_unref(compare);
    close(input); close(target); unlink(source); unlink(output); unlink(moved); unlink(victim); rmdir(dir);
    alarm(0);
}

static void control_inner(gboolean cross_uid)
{
    alarm(5);
    int control[2], blocked[2];
    if (pipe(control) != 0 || pipe(blocked) != 0 || install_signal_handlers() != 0) _exit(10);
    pid_t operation = getpid(), sender = fork();
    if (sender < 0) _exit(11);
    if (!sender) {
        close(control[0]); close(blocked[0]); close(blocked[1]);
        if (cross_uid && (setgid(1000) != 0 || setuid(1000) != 0)) _exit(12);
        usleep(60000);
        if (cross_uid && (kill(operation, SIGTERM) != -1 || errno != EPERM)) _exit(13);
        close(control[1]); _exit(0);
    }
    close(control[1]); cancelled = 0;
    if (start_cancellation_channel(control[0]) != 0) _exit(14);
    close(control[0]);
    char byte;
    ssize_t count = read(blocked[0], &byte, 1);
    int saved = errno, status = 0;
    if (wait_child(sender, &status) != sender) _exit(15);
    _exit(count == -1 && saved == EINTR && cancelled && WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 16);
}

static void control_case(gconstpointer data)
{
    gboolean cross_uid = GPOINTER_TO_INT(data);
    if (cross_uid && geteuid() != 0) { g_test_skip("Cross-UID case runs inside the isolated root VM"); return; }
    pid_t child = fork(); g_assert_cmpint(child, >=, 0);
    if (!child) control_inner(cross_uid);
    int status; g_assert_cmpint(wait_child(child, &status), ==, child);
    g_assert_true(WIFEXITED(status)); g_assert_cmpint(WEXITSTATUS(status), ==, 0);
}

static void control_argument_parser(void)
{
    pid_t child = fork(); g_assert_cmpint(child, >=, 0);
    if (!child) {
        alarm(3);
        int sink = open("/dev/null", O_WRONLY);
        if (sink >= 0) { dup2(sink, STDOUT_FILENO); dup2(sink, STDERR_FILENO); close(sink); }
        char *argv[] = {"tuxflash-helper", "write", "--cancel-stdin", NULL};
        _exit(tuxflash_helper_entry(3, argv) == 64 ? 0 : 1);
    }
    int status; g_assert_cmpint(wait_child(child, &status), ==, child);
    g_assert_true(WIFEXITED(status)); g_assert_cmpint(WEXITSTATUS(status), ==, 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/io/source-rejections", reject_sources);
    g_test_add_func("/io/cancel-blocked-write", cancelled_write);
    for (guint i = 0; i < G_N_ELEMENTS(codecs); i++) {
        g_autofree char *name = g_strdup_printf("/io/pinned-decoder/%s", codecs[i].suffix);
        g_test_add_data_func(name, &codecs[i], pinned_decoder);
    }
    g_test_add_data_func("/io/capture-pinned-output/vhd", "vhd", capture_descriptor);
    g_test_add_data_func("/io/capture-pinned-output/vhdx", "vhdx", capture_descriptor);
    g_test_add_data_func("/io/cancel-control-eof", GINT_TO_POINTER(FALSE), control_case);
    g_test_add_data_func("/io/cancel-control-unprivileged-sender", GINT_TO_POINTER(TRUE), control_case);
    g_test_add_func("/io/cancel-control-argument-parser", control_argument_parser);
    return g_test_run();
}
