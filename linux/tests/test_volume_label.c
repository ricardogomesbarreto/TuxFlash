/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "volume_label.h"

static void check(const char *path, gboolean fat32, const char *expected)
{
    char *label = lufus_volume_label_for_image(path, fat32);
    g_assert_cmpstr(label, ==, expected);
    g_free(label);
}

static void names_and_limits(void)
{
    check("/images/Windows 10 PRO 2021.iso", TRUE, "WINDOWS 10");
    check("/images/Windows 10 PRO 2021.iso", FALSE, "Windows 10 PRO 2021");
    check("/images/abcdefghijklmnopqrstuvwxyz0123456789.ISO", TRUE, "ABCDEFGHIJK");
    check("/images/abcdefghijklmnopqrstuvwxyz0123456789.ISO", FALSE,
          "abcdefghijklmnopqrstuvwxyz012345");
    check("windows.v2.iso", TRUE, "WINDOWS V2");
}

static void unsafe_and_empty_names(void)
{
    check("/images/   .iso", TRUE, "INSTALL");
    check(".iso", FALSE, "INSTALL");
    check(NULL, TRUE, "INSTALL");
    check("/images/Windows\nname=BAD,\"x\".iso", FALSE, "Windows name BAD x");
    check("/images/Edição 10.iso", TRUE, "EDICAO 10");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/volume-label/names-and-limits", names_and_limits);
    g_test_add_func("/volume-label/unsafe-and-empty", unsafe_and_empty_names);
    return g_test_run();
}
