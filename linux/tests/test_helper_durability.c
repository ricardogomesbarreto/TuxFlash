/*
 * Focused tests for the privileged helper's durable copy accounting.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Keep the test fast while preserving the production ratio: progress may
 * advance only after two complete buffers have been committed. */
#define COPY_BUFFER_SIZE 4096U
#define COPY_COMMIT_INTERVAL 8192ULL
#define main lufus_helper_entry_point
#include "../src/helper.c"
#undef main

static void test_periodic_and_tail_commits(void)
{
    static const gsize size = 5U * COPY_BUFFER_SIZE;
    GError *error = NULL;
    char *directory = g_dir_make_tmp("lufus-durable-copy-XXXXXX", &error);
    char *source;
    char *destination;
    unsigned char *contents;
    CopyProgress progress = {.total = size};

    g_assert_no_error(error);
    g_assert_nonnull(directory);
    source = g_build_filename(directory, "source.bin", NULL);
    destination = g_build_filename(directory, "destination.bin", NULL);
    contents = g_malloc0(size);
    g_assert_true(g_file_set_contents(source, (const char *)contents, size, &error));
    g_assert_no_error(error);

    cancelled = 0;
    g_assert_cmpint(copy_regular_file(source, destination, 0600, &progress), ==, 0);
    g_assert_true(progress.submitted == size);
    /* 16 KiB is durable; the final 4 KiB is intentionally still pending. */
    g_assert_true(progress.completed == 4U * COPY_BUFFER_SIZE);

    g_assert_cmpint(copy_progress_commit_path(&progress, directory), ==, 0);
    g_assert_true(progress.completed == size);
    g_assert_true(progress.submitted == progress.completed);

    g_free(contents);
    g_assert_cmpint(unlink(destination), ==, 0);
    g_assert_cmpint(unlink(source), ==, 0);
    g_assert_cmpint(rmdir(directory), ==, 0);
    g_free(destination);
    g_free(source);
    g_free(directory);
}

static void test_empty_commit_is_a_noop(void)
{
    char *directory = g_dir_make_tmp("lufus-durable-empty-XXXXXX", NULL);
    CopyProgress progress = {0};

    g_assert_nonnull(directory);
    g_assert_cmpint(copy_progress_commit_path(&progress, directory), ==, 0);
    g_assert_true(progress.completed == 0);
    g_assert_true(progress.submitted == 0);
    g_assert_cmpint(rmdir(directory), ==, 0);
    g_free(directory);
}

static void test_validation_counts_and_hashes_target_bytes(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("lufus-validation-XXXXXX", &error);
    char *payload;
    char *original_loader;
    char *validation_loader;
    char *old_manifest;
    GString *manifest = g_string_new(NULL);
    unsigned char *buffer = g_malloc(COPY_BUFFER_SIZE);
    uint64_t total = 0;
    uint64_t checked = 0;
    unsigned int last_thousandths = UINT_MAX;

    g_assert_no_error(error);
    g_assert_nonnull(directory);
    payload = g_build_filename(directory, "payload.bin", NULL);
    original_loader = g_build_filename(directory, "bootx64_original.efi", NULL);
    validation_loader = g_build_filename(directory, "bootx64.efi", NULL);
    old_manifest = g_build_filename(directory, "md5sum.txt", NULL);
    g_assert_true(g_file_set_contents(payload, "payload", 7, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(original_loader, "original", 8, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(validation_loader, "validator", 9, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(old_manifest, "old", 3, &error));
    g_assert_no_error(error);

    cancelled = 0;
    g_assert_cmpint(validation_tree_size(directory, "", &total), ==, 0);
    g_assert_true(total == 15);
    g_assert_cmpint(md5_tree(directory, "", manifest, total, &checked,
                            &last_thousandths, buffer, COPY_BUFFER_SIZE), ==, 0);
    g_assert_true(checked == total);
    g_assert_nonnull(strstr(manifest->str, "  ./payload.bin\n"));
    g_assert_nonnull(strstr(manifest->str, "  ./bootx64_original.efi\n"));
    g_assert_null(strstr(manifest->str, "  ./bootx64.efi\n"));
    g_assert_null(strstr(manifest->str, "  ./md5sum.txt\n"));

    g_assert_cmpint(unlink(old_manifest), ==, 0);
    g_assert_cmpint(unlink(validation_loader), ==, 0);
    g_assert_cmpint(unlink(original_loader), ==, 0);
    g_assert_cmpint(unlink(payload), ==, 0);
    g_assert_cmpint(rmdir(directory), ==, 0);
    g_free(old_manifest);
    g_free(validation_loader);
    g_free(original_loader);
    g_free(payload);
    g_free(directory);
    g_free(buffer);
    g_string_free(manifest, TRUE);
}

typedef struct {
    const char *name;
    uint64_t total;
    uint64_t capacity;
    gsize source_size;
    int expected_errno;
} ImageCopyCase;

static const ImageCopyCase image_copy_cases[] = {
    {"known-exact", 8193, 8193, 8193, 0},
    {"stream-exact", 0, 8193, 8193, 0},
    {"source-shortened", 8193, 16384, 4096, EIO},
    {"source-grown", 4096, 16384, 8193, EIO},
    {"stream-over-capacity", 0, 8192, 8193, ENOSPC},
    {"known-over-capacity", 8193, 8192, 8193, ENOSPC},
    {"empty-stream", 0, 8192, 0, ENODATA},
};

static void test_image_copy_length(gconstpointer data)
{
    const ImageCopyCase *test = data;
    char *directory = g_dir_make_tmp("lufus-image-copy-XXXXXX", NULL);
    char *source = g_build_filename(directory, "source.img", NULL);
    char *output = g_build_filename(directory, "target.img", NULL);
    unsigned char *contents = g_malloc(MAX(test->source_size, (gsize)1));
    for (gsize i = 0; i < test->source_size; i++)
        contents[i] = (unsigned char)(i * 17U + 29U);
    g_assert_true(g_file_set_contents(source, (const char *)contents,
                                      test->source_size, NULL));
    int input_fd = open(source, O_RDONLY | O_CLOEXEC);
    int output_fd = open(output, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    g_assert_cmpint(input_fd, >=, 0);
    g_assert_cmpint(output_fd, >=, 0);
    uint64_t written = UINT64_MAX;
    cancelled = 0;
    errno = 0;
    int result = copy_image(input_fd, output_fd, test->total,
                            test->capacity, &written);
    int saved_errno = errno;
    struct stat details;
    g_assert_cmpint(fstat(output_fd, &details), ==, 0);
    g_assert_true((uint64_t)details.st_size <= test->capacity);
    if (test->expected_errno != 0) {
        g_assert_cmpint(result, ==, -1);
        g_assert_cmpint(saved_errno, ==, test->expected_errno);
        g_assert_true(written == UINT64_MAX);
    } else {
        g_assert_cmpint(result, ==, 0);
        g_assert_true(written == test->source_size);
        char *actual = NULL;
        gsize actual_size = 0;
        g_assert_true(g_file_get_contents(output, &actual, &actual_size, NULL));
        g_assert_cmpmem(actual, actual_size, contents, test->source_size);
        g_free(actual);
    }
    g_assert_cmpint(close(input_fd), ==, 0);
    g_assert_cmpint(close(output_fd), ==, 0);
    g_assert_cmpint(unlink(source), ==, 0);
    g_assert_cmpint(unlink(output), ==, 0);
    g_assert_cmpint(rmdir(directory), ==, 0);
    g_free(contents);
    g_free(source);
    g_free(output);
    g_free(directory);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/helper-durability/periodic-and-tail-commits",
                    test_periodic_and_tail_commits);
    g_test_add_func("/helper-durability/empty-commit", test_empty_commit_is_a_noop);
    g_test_add_func("/helper-durability/validation-target-bytes",
                    test_validation_counts_and_hashes_target_bytes);
    for (guint i = 0; i < G_N_ELEMENTS(image_copy_cases); i++) {
        char *name = g_strdup_printf("/helper-image-copy/%s", image_copy_cases[i].name);
        g_test_add_data_func(name, &image_copy_cases[i], test_image_copy_length);
        g_free(name);
    }
    return g_test_run();
}
