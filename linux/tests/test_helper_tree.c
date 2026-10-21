/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main lufus_helper_entry_point
#include "../src/helper.c"
#undef main

static void cleanup(const char *path)
{
    struct stat details;
    g_assert_cmpint(lstat(path, &details), ==, 0);
    if (S_ISDIR(details.st_mode)) {
        GDir *dir = g_dir_open(path, 0, NULL);
        const char *name;
        g_assert_nonnull(dir);
        while ((name = g_dir_read_name(dir)) != NULL) {
            g_autofree char *child = g_build_filename(path, name, NULL);
            cleanup(child);
        }
        g_dir_close(dir);
        g_assert_cmpint(rmdir(path), ==, 0);
    } else {
        g_assert_cmpint(unlink(path), ==, 0);
    }
}

static void test_zorin_aliases(void)
{
    g_autofree char *base = g_dir_make_tmp("tuxflash-tree-XXXXXX", NULL);
    g_autofree char *source = g_build_filename(base, "iso", NULL);
    g_autofree char *noble = g_build_filename(source, "dists", "noble", NULL);
    g_autofree char *destination = g_build_filename(base, "usb", NULL);
    g_assert_cmpint(g_mkdir_with_parents(noble, 0700), ==, 0);
    g_autofree char *release = g_build_filename(noble, "Release", NULL);
    g_assert_true(g_file_set_contents(release, "release\n", -1, NULL));
    const char *names[] = {"stable", "unstable"};
    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        g_autofree char *alias = g_build_filename(source, "dists", names[i], NULL);
        g_assert_cmpint(symlink("noble", alias), ==, 0);
    }
    g_autofree char *file_alias = g_build_filename(source, "release-alias", NULL);
    g_assert_cmpint(symlink("dists/noble/Release", file_alias), ==, 0);
    uint64_t total = 0;
    g_assert_cmpint(copy_tree_size(source, false, &total, NULL), ==, 0);
    g_assert_true(total == 4 * strlen("release\n"));
    CopyProgress progress = {.total = total};
    g_assert_cmpint(copy_tree(source, destination, NULL, NULL, &progress), ==, 0);
    g_assert_true(progress.submitted == total);
    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        g_autofree char *alias = g_build_filename(destination, "dists", names[i], NULL);
        g_autofree char *file = g_build_filename(alias, "Release", NULL);
        struct stat st;
        g_assert_cmpint(lstat(alias, &st), ==, 0);
        g_assert_true(S_ISDIR(st.st_mode));
        g_autofree char *data = NULL;
        g_assert_true(g_file_get_contents(file, &data, NULL, NULL));
        g_assert_cmpstr(data, ==, "release\n");
    }
    cleanup(base);
}

static void reject_link(const char *target, int expected_errno)
{
    g_autofree char *base = g_dir_make_tmp("tuxflash-tree-guard-XXXXXX", NULL);
    g_autofree char *source = g_build_filename(base, "iso", NULL);
    g_autofree char *destination = g_build_filename(base, "usb", NULL);
    g_autofree char *outside = g_build_filename(base, "iso-outside", NULL);
    g_autofree char *alias = g_build_filename(source, "alias", NULL);
    g_assert_cmpint(mkdir(source, 0700), ==, 0);
    g_assert_true(g_file_set_contents(outside, "private", -1, NULL));
    g_assert_cmpint(symlink(target, alias), ==, 0);
    uint64_t total = 0;
    errno = 0;
    g_assert_cmpint(copy_tree_size(source, false, &total, NULL), ==, -1);
    g_assert_cmpint(errno, ==, expected_errno);
    g_assert_cmpint(copy_tree(source, destination, NULL, NULL, NULL), ==, -1);
    g_autofree char *leak = g_build_filename(destination, "alias", NULL);
    g_assert_false(g_file_test(leak, G_FILE_TEST_EXISTS));
    cleanup(base);
}
static void test_escape(void) { reject_link("../iso-outside", EPERM); }
static void test_absolute_escape(void) { reject_link("/etc/passwd", EPERM); }
static void test_cycle(void) { reject_link(".", ELOOP); }
static void test_dangling(void) { reject_link("missing", ENOENT); }

static void test_special_file(void)
{
    g_autofree char *source = g_dir_make_tmp("tuxflash-tree-fifo-XXXXXX", NULL);
    g_autofree char *fifo = g_build_filename(source, "pipe", NULL);
    g_assert_cmpint(mkfifo(fifo, 0600), ==, 0);
    uint64_t total = 0;
    g_assert_cmpint(copy_tree_size(source, false, &total, NULL), ==, -1);
    g_assert_cmpint(errno, ==, EINVAL);
    cleanup(source);
}

static void test_split_accounting(void)
{
    g_autofree char *base = g_dir_make_tmp("tuxflash-tree-split-XXXXXX", NULL);
    g_autofree char *source = g_build_filename(base, "iso", NULL);
    g_autofree char *destination = g_build_filename(base, "usb", NULL);
    g_assert_cmpint(mkdir(source, 0700), ==, 0);
    g_autofree char *image = g_build_filename(source, "install.wim", NULL);
    int fd = open(image, O_CREAT | O_EXCL | O_WRONLY, 0600);
    g_assert_cmpint(fd, >=, 0);
    g_assert_cmpint(ftruncate(fd, 4000000001LL), ==, 0);
    close(fd);
    uint64_t total = 0, skipped = 0;
    g_assert_cmpint(copy_tree_size(source, true, &total, &skipped), ==, 0);
    g_assert_true(total == 0 && skipped == 4000000001ULL);
    char *split_source = NULL, *split_destination = NULL;
    g_assert_cmpint(copy_tree(source, destination, &split_source, &split_destination, NULL), ==, 0);
    g_assert_cmpstr(split_source, ==, image);
    g_assert_true(g_str_has_suffix(split_destination, "/install.swm"));
    g_free(split_source); g_free(split_destination);
    cleanup(base);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/helper-tree/zorin-internal-aliases", test_zorin_aliases);
    g_test_add_func("/helper-tree/reject-sibling-prefix-escape", test_escape);
    g_test_add_func("/helper-tree/reject-absolute-escape", test_absolute_escape);
    g_test_add_func("/helper-tree/reject-directory-cycle", test_cycle);
    g_test_add_func("/helper-tree/reject-dangling-link", test_dangling);
    g_test_add_func("/helper-tree/reject-special-file", test_special_file);
    g_test_add_func("/helper-tree/windows-split-accounting", test_split_accounting);
    return g_test_run();
}
