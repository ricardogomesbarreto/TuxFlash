/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main lufus_helper_entry_point
#include "../src/helper.c"
#undef main

static void test_root_capacity(void)
{
    g_assert_true(virtual_disk_size_from_json(
        "{\"children\":[{\"info\":{\"virtual-size\":75497472}}],"
        "\"virtual-size\":67108864,\"format\":\"vhdx\"}") == 67108864);
    g_assert_true(virtual_disk_size_from_json(
        "{\"virtual-size\":8589934592}") == UINT64_C(8589934592));
}

static void test_reject_invalid_capacity(void)
{
    const char *invalid[] = {NULL, "", "not json", "null", "[]", "{}",
        "{\"children\":[{\"virtual-size\":123}]}",
        "{\"virtual-size\":0}", "{\"virtual-size\":-1}",
        "{\"virtual-size\":true}", "{\"virtual-size\":\"67108864\"}",
        "{\"virtual-size\":1.5}", "{\"virtual-size\":null}"};
    for (guint i = 0; i < G_N_ELEMENTS(invalid); i++)
        g_assert_true(virtual_disk_size_from_json(invalid[i]) == 0);
}

static void qemu_run(char **argv)
{
    GError *error = NULL;
    int status = -1;
    g_assert_true(g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                              NULL, NULL, NULL, NULL, &status, &error));
    g_assert_no_error(error);
    g_assert_true(g_spawn_check_wait_status(status, &error));
    g_assert_no_error(error);
}

static void test_real_containers(void)
{
    char *qemu = g_find_program_in_path("qemu-img");
    if (qemu == NULL) {
        g_test_skip("qemu-img not available; real container checks require it");
        return;
    }
    char *dir = g_dir_make_tmp("lufus-helper-images-XXXXXX", NULL);
    g_assert_nonnull(dir);
    for (guint i = 0; i < 2; i++) {
        char *image = g_build_filename(dir, i == 0 ? "disk.vhd" : "disk.vhdx", NULL);
        char *create[] = {qemu, "create", "-f", i == 0 ? "vpc" : "vhdx", "-o",
            i == 0 ? "subformat=fixed,force_size=on" : "subformat=dynamic",
            image, "67108864", NULL};
        qemu_run(create);
        g_assert_true(virtual_disk_size(image) == 67108864);
        struct stat details;
        g_assert_cmpint(stat(image, &details), ==, 0);
        /* Physical file length deliberately differs from virtual capacity. */
        g_assert_true((uint64_t)details.st_size != 67108864);
        g_assert_cmpint(unlink(image), ==, 0);
        g_free(image);
    }
    char *fake = g_build_filename(dir, "fake.vhd", NULL);
    g_assert_true(g_file_set_contents(fake, "not a virtual disk\n", -1, NULL));
    g_assert_true(virtual_disk_size(fake) == 0);
    g_assert_cmpint(unlink(fake), ==, 0);
    g_assert_cmpint(rmdir(dir), ==, 0);
    g_free(fake);
    g_free(dir);
    g_free(qemu);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/helper-images/root-capacity", test_root_capacity);
    g_test_add_func("/helper-images/invalid-capacity", test_reject_invalid_capacity);
    g_test_add_func("/helper-images/real-containers", test_real_containers);
    return g_test_run();
}
