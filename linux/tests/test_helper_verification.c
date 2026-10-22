/* SPDX-License-Identifier: GPL-3.0-or-later */
#define COPY_BUFFER_SIZE 4096U
#define COPY_COMMIT_INTERVAL (32ULL * 1024ULL)
#define main tuxflash_helper_entry
#include "../src/helper.c"
#undef main

/* No block devices are opened by these tests. Firmware/VM tests exercise the
 * production wrapper, including target validation and cache invalidation. */
static unsigned char source_buffer[COPY_BUFFER_SIZE];
static unsigned char target_buffer[COPY_BUFFER_SIZE];

typedef struct {
    const char *name;
    gsize source_size, target_size;
    uint64_t expected;
    int error;
    bool corrupt_target;
} RawCase;

static void raw_case(gconstpointer data)
{
    const RawCase *test = data;
    g_autofree char *dir = g_dir_make_tmp("tuxflash-verify-XXXXXX", NULL);
    g_autofree char *source = g_build_filename(dir, "source.img", NULL);
    g_autofree char *target = g_build_filename(dir, "target.img", NULL);
    g_autofree char *bytes = g_malloc0(MAX((gsize)1, MAX(test->source_size, test->target_size)));
    g_assert_true(g_file_set_contents(source, bytes, test->source_size, NULL));
    if (test->corrupt_target) bytes[test->expected - 1] = 42;
    g_assert_true(g_file_set_contents(target, bytes, test->target_size, NULL));
    int input = open(source, O_RDONLY | O_CLOEXEC);
    int output = open(target, O_RDONLY | O_CLOEXEC);
    g_assert_cmpint(input, >=, 0); g_assert_cmpint(output, >=, 0);
    cancelled = 0;
    int result = verify_stream(input, output, test->expected, source_buffer, target_buffer);
    int saved = errno;
    g_assert_cmpint(result, ==, test->error ? -1 : 0);
    if (test->error) g_assert_cmpint(saved, ==, test->error);
    close(input); close(output); unlink(source); unlink(target); rmdir(dir);
}

static const RawCase raw_cases[] = {
    {"exact", 8193, 8193, 8193, 0, false},
    {"target-padding", 8193, 16384, 8193, 0, false},
    {"source-short", 8192, 8193, 8193, EIO, false},
    {"source-grown", 8194, 16384, 8193, EIO, false},
    {"target-short", 8193, 8192, 8193, EIO, false},
    {"last-byte-corrupt", 8193, 8193, 8193, EILSEQ, true},
    {"empty", 0, 0, 0, ENODATA, false},
};

typedef struct { const char *tool, *extension; } Codec;
static const Codec codecs[] = {{"gzip","gz"}, {"xz","xz"}, {"bzip2","bz2"}, {"zstd","zst"}};

static void compressed_case(gconstpointer data)
{
    const Codec *codec = data;
    /* Bound a regression where a grown stream blocks waiting for its child. */
    alarm(10);
    g_autofree char *dir = g_dir_make_tmp("tuxflash-codec-XXXXXX", NULL);
    g_autofree char *raw = g_build_filename(dir, "source.img", NULL);
    g_autofree char *compressed = g_strdup_printf("%s/source.img.%s", dir, codec->extension);
    const gsize size = 2U * 1024U * 1024U + 17;
    g_autofree char *bytes = g_malloc(size);
    for (gsize i = 0; i < size; i++) bytes[i] = (char)(i * 17U + i / 53U);
    g_assert_true(g_file_set_contents(raw, bytes, size, NULL));
    char *args[] = {(char *)codec->tool, "-c", "--", raw, NULL};
    GSubprocess *process = g_subprocess_newv((const gchar *const *)args,
                                            G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL);
    g_assert_nonnull(process);
    GBytes *encoded = NULL;
    g_assert_true(g_subprocess_communicate(process, NULL, NULL, &encoded, NULL, NULL));
    g_assert_true(g_subprocess_get_successful(process)); g_object_unref(process);
    gsize encoded_size;
    const char *encoded_data = g_bytes_get_data(encoded, &encoded_size);
    g_assert_true(g_file_set_contents(compressed, encoded_data, encoded_size, NULL));
    for (unsigned int mode = 0; mode < 4; mode++) {
        /* Exact, stream grew substantially, stream shorter than committed,
         * then a damaged/truncated compressed trailer. */
        uint64_t expected = mode == 1 ? 4096 : mode == 2 ? size + 1 : size;
        if (mode == 3)
            g_assert_cmpint(truncate(compressed, encoded_size - 8), ==, 0);
        int target = open(raw, O_RDONLY | O_CLOEXEC);
        uint64_t ignored;
        cancelled = 0;
        int input = open_source(compressed, &ignored);
        g_assert_cmpint(input, >=, 0); g_assert_cmpint(target, >=, 0);
        int result = verify_stream(input, target, expected, source_buffer, target_buffer);
        if (result != 0) kill(decompressor_pid, SIGTERM);
        close(input); close(target);
        int decoder_result = wait_for_decompressor();
        if (mode == 0) {
            g_assert_cmpint(result, ==, 0); g_assert_cmpint(decoder_result, ==, 0);
        } else g_assert_true(result != 0 || decoder_result != 0);
        g_assert_cmpint(decompressor_pid, ==, -1);
    }
    g_bytes_unref(encoded); unlink(raw); unlink(compressed); rmdir(dir);
    alarm(0);
}

/* A compressed file must be opened once: not through a symlink or an
 * untrusted pathname reopened later by a privileged decompressor. */
static void compressed_path_safety(void)
{
    alarm(10);
    g_autofree char *dir = g_dir_make_tmp("tuxflash-input-XXXXXX", NULL);
    g_autofree char *raw = g_build_filename(dir, "raw.img", NULL);
    g_autofree char *source = g_build_filename(dir, "original.img.gz", NULL);
    g_autofree char *swapped = g_build_filename(dir, "replacement.img.gz", NULL);
    g_autofree char *saved = g_build_filename(dir, "saved.img.gz", NULL);
    g_autofree char *link = g_build_filename(dir, "link.img.gz", NULL);
    const char expected[] = "TuxFlash source anchored to file descriptor";
    g_assert_true(g_file_set_contents(raw, expected, sizeof(expected), NULL));
    char *args[] = {"gzip", "-c", "--", raw, NULL};
    GSubprocess *process = g_subprocess_newv((const gchar *const *)args,
                                             G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL);
    g_assert_nonnull(process);
    GBytes *encoded = NULL;
    g_assert_true(g_subprocess_communicate(process, NULL, NULL, &encoded, NULL, NULL));
    g_assert_true(g_subprocess_get_successful(process));
    g_object_unref(process);
    gsize length = 0;
    gconstpointer payload = g_bytes_get_data(encoded, &length);
    g_assert_true(g_file_set_contents(source, payload, length, NULL));
    g_assert_true(g_file_set_contents(swapped, "not compressed", -1, NULL));
    uint64_t total = 0;
    g_assert_cmpint(symlink(source, link), ==, 0);
    errno = 0;
    g_assert_cmpint(open_source(link, &total), ==, -1);
    g_assert_cmpint(errno, ==, ELOOP);
    int stream = open_source(source, &total);
    g_assert_cmpint(stream, >=, 0);
    g_assert_cmpuint(total, ==, 0);
    g_assert_cmpint(rename(source, saved), ==, 0);
    g_assert_cmpint(rename(swapped, source), ==, 0);
    char actual[sizeof(expected) + 64] = {0};
    size_t offset = 0;
    ssize_t amount;
    while ((amount = read(stream, actual + offset, sizeof(actual) - offset)) > 0)
        offset += (size_t)amount;
    g_assert_cmpint(amount, ==, 0);
    g_assert_cmpuint(offset, ==, sizeof(expected));
    g_assert_cmpmem(actual, offset, expected, sizeof(expected));
    close(stream);
    g_assert_cmpint(wait_for_decompressor(), ==, 0);
    g_assert_cmpint(decompressor_pid, ==, -1);
    g_bytes_unref(encoded);
    unlink(link); unlink(source); unlink(saved); unlink(raw); rmdir(dir);
    alarm(0);
}

static void capture_output_exclusive(void)
{
    g_autofree char *dir = g_dir_make_tmp("tuxflash-capture-output-XXXXXX", NULL);
    g_autofree char *existing = g_build_filename(dir, "existing.vhdx", NULL);
    g_autofree char *link = g_build_filename(dir, "linked.vhdx", NULL);
    g_autofree char *fresh = g_build_filename(dir, "fresh.vhdx", NULL);
    const char sentinel[] = "DO NOT OVERWRITE";
    g_assert_true(g_file_set_contents(existing, sentinel, sizeof(sentinel), NULL));
    int out = reserve_capture_output(existing);
    g_assert_cmpint(out, ==, -1);
    g_assert_cmpint(errno, ==, EEXIST);
    g_autofree char *after = NULL;
    gsize length = 0;
    g_assert_true(g_file_get_contents(existing, &after, &length, NULL));
    g_assert_cmpmem(after, length, sentinel, sizeof(sentinel));
    g_assert_cmpint(symlink(existing, link), ==, 0);
    g_assert_cmpint(reserve_capture_output(link), ==, -1);
    out = reserve_capture_output(fresh);
    g_assert_cmpint(out, >=, 0);
    struct stat details;
    g_assert_cmpint(fstat(out, &details), ==, 0);
    g_assert_cmpint(details.st_mode & 0777, ==, 0600);
    close(out);
    unlink(fresh); unlink(link); unlink(existing); rmdir(dir);
}

static void interrupted_read(gconstpointer data)
{
    int mode = GPOINTER_TO_INT(data);
    alarm(5);
    int stream[2], target[2];
    g_assert_cmpint(pipe(stream), ==, 0); g_assert_cmpint(pipe(target), ==, 0);
    struct sigaction previous_int, previous_term;
    g_assert_cmpint(sigaction(SIGINT, NULL, &previous_int), ==, 0);
    g_assert_cmpint(sigaction(SIGTERM, NULL, &previous_term), ==, 0);
    g_assert_cmpint(install_signal_handlers(), ==, 0);
    cancelled = 0;
    if (mode >= 1) g_assert_cmpint(write(stream[1], "x", 1), ==, 1);
    if (mode == 2) g_assert_cmpint(write(target[1], "x", 1), ==, 1);
    pid_t child = fork(); g_assert_cmpint(child, >=, 0);
    if (!child) {
        close(stream[0]); usleep(30000); kill(getppid(), SIGTERM);
        usleep(100000); _exit(0);
    }
    if (mode == 1) close(stream[1]);
    int result = verify_stream(stream[0], target[0], 1, source_buffer, target_buffer);
    int saved = errno;
    g_assert_cmpint(result, ==, -1); g_assert_cmpint(saved, ==, ECANCELED);
    int status; g_assert_cmpint(wait_child(child, &status), ==, child);
    g_assert_cmpint(sigaction(SIGINT, &previous_int, NULL), ==, 0);
    g_assert_cmpint(sigaction(SIGTERM, &previous_term, NULL), ==, 0);
    close(stream[0]); if (mode != 1) close(stream[1]);
    close(target[0]); close(target[1]); cancelled = 0;
    alarm(0);
}

static void progress_volume(void)
{
    g_autofree char *source = NULL, *log = NULL;
    int fd = g_file_open_tmp("tuxflash-progress-source-XXXXXX", &source, NULL);
    int log_fd = g_file_open_tmp("tuxflash-progress-log-XXXXXX", &log, NULL);
    g_assert_cmpint(fd, >=, 0); g_assert_cmpint(log_fd, >=, 0);
    g_assert_cmpint(ftruncate(fd, 1024 * 1024), ==, 0);
    int other = open(source, O_RDONLY | O_CLOEXEC);
    fflush(stdout); int saved = dup(STDOUT_FILENO);
    g_assert_cmpint(dup2(log_fd, STDOUT_FILENO), >=, 0);
    cancelled = 0;
    int result = verify_stream(fd, other, 1024 * 1024, source_buffer, target_buffer);
    fflush(stdout); g_assert_cmpint(dup2(saved, STDOUT_FILENO), >=, 0); close(saved);
    g_assert_cmpint(result, ==, 0);
    g_autofree char *events = NULL;
    g_assert_true(g_file_get_contents(log, &events, NULL, NULL));
    unsigned int count = 0; uint64_t previous = 0;
    char **lines = g_strsplit(events, "\n", -1);
    for (guint i = 0; lines[i][0]; i++) {
        unsigned long long checked, total;
        g_assert_cmpint(sscanf(lines[i], "VERIFY_PROGRESS %llu %llu", &checked, &total), ==, 2);
        g_assert_true(checked > previous && checked < total);
        previous = checked; count++;
    }
    /* 256 individual reads must not emit 256 lines. The final event belongs
     * to the wrapper, after the decompressor's exit status has been checked. */
    g_assert_cmpuint(count, >, 0); g_assert_cmpuint(count, <=, 40);
    g_strfreev(lines); close(fd); close(other); close(log_fd); unlink(source); unlink(log);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    for (guint i = 0; i < G_N_ELEMENTS(raw_cases); i++) {
        g_autofree char *name = g_strdup_printf("/verify/raw/%s", raw_cases[i].name);
        g_test_add_data_func(name, &raw_cases[i], raw_case);
    }
    for (guint i = 0; i < G_N_ELEMENTS(codecs); i++) {
        g_autofree char *name = g_strdup_printf("/verify/compressed/%s", codecs[i].extension);
        g_test_add_data_func(name, &codecs[i], compressed_case);
    }
    g_test_add_func("/verify/compressed/path-safety", compressed_path_safety);
    g_test_add_func("/capture/exclusive-output", capture_output_exclusive);
    g_test_add_data_func("/verify/cancel-source-read", GINT_TO_POINTER(0), interrupted_read);
    g_test_add_data_func("/verify/cancel-target-read", GINT_TO_POINTER(1), interrupted_read);
    g_test_add_data_func("/verify/cancel-eof-read", GINT_TO_POINTER(2), interrupted_read);
    g_test_add_func("/verify/bounded-progress", progress_volume);
    return g_test_run();
}
