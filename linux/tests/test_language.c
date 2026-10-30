/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <libintl.h>
#include <locale.h>
#include <sys/stat.h>
#include <stdio.h>
#include "language.h"
#include "release_notes.h"

static char *settings_file(void)
{ return g_build_filename(g_get_user_config_dir(), "tuxflash", "settings.ini", NULL); }
static void test_restart_translation(void)
{
    if (g_test_subprocess()) {
        tuxflash_language_init(TUXFLASH_TEST_CATALOGS);
        /* Compare catalog UTF-8 bytes, independent of terminal charset. */
        fputs(gettext("READY"), stdout);
        return;
    }
    const char *expected[] = {"PRONTO", "READY", "就绪", "LISTO"};
    for (guint i=0;i<4;i++) {
        GError *error=NULL;
        g_assert_true(tuxflash_language_save(i,&error));g_assert_no_error(error);
        g_setenv("LANGUAGE","de_DE",TRUE); /* Saved preference wins. */
        g_test_trap_subprocess(NULL,10*G_USEC_PER_SEC,0);
        g_test_trap_assert_passed();g_test_trap_assert_stdout(expected[i]);
    }
}
static void test_translated_release_notes(void)
{
    if (g_test_subprocess()) {
        tuxflash_language_init(TUXFLASH_TEST_CATALOGS);
        g_autofree char *notes = tuxflash_release_notes();
        g_autofree char *document = g_strconcat("<root>", notes, "</root>", NULL);
        GMarkupParser parser = {0};
        g_autoptr(GMarkupParseContext) context = g_markup_parse_context_new(&parser, 0, NULL, NULL);
        GError *error = NULL;
        g_assert_true(g_markup_parse_context_parse(context, document, -1, &error));
        g_assert_true(g_markup_parse_context_end_parse(context, &error));
        g_assert_no_error(error);
        const char *title = gettext("Updated in 0.3.8");
        g_assert_nonnull(strstr(notes, title));
        fputs(title, stdout);
        return;
    }
    const char *expected[] = {"Atualizado na versão 0.3.8", "Updated in 0.3.8", "0.3.8 版更新", "Actualizado en la versión 0.3.8"};
    for (guint i = 0; i < 4; i++) {
        g_assert_true(tuxflash_language_save(i, NULL));
        g_test_trap_subprocess(NULL, 10 * G_USEC_PER_SEC, 0);
        g_test_trap_assert_passed();
        g_test_trap_assert_stdout(expected[i]);
    }
}
static void test_preserve_settings(void)
{
    g_autofree char *path=settings_file();
    g_assert_true(tuxflash_language_save(0,NULL));
    g_file_set_contents(path,"# preserved\n[Other]\nValue=keep\n",-1,NULL);
    GError *error=NULL;
    g_assert_true(tuxflash_language_save(2,&error));g_assert_no_error(error);
    g_autoptr(GKeyFile) key=g_key_file_new();
    g_assert_true(g_key_file_load_from_file(key,path,G_KEY_FILE_NONE,&error));
    g_autofree char *value=g_key_file_get_string(key,"Other","Value",NULL);
    g_assert_cmpstr(value,==,"keep");
    g_autofree char *language=g_key_file_get_string(key,"Interface","Language",NULL);
    g_assert_cmpstr(language,==,"zh_CN");
    g_assert_cmpuint(tuxflash_language_selected(),==,2);
    struct stat sb;g_assert_cmpint(g_stat(path,&sb),==,0);g_assert_cmpuint(sb.st_mode&0777,==,0600);
}
static void test_invalid_and_corrupt(void)
{
    g_autofree char *path=settings_file();
    g_file_set_contents(path,"not a key file\n",-1,NULL);
    GError *error=NULL;g_assert_false(tuxflash_language_save(0,&error));g_assert_nonnull(error);g_clear_error(&error);
    g_autofree char *data=NULL;g_file_get_contents(path,&data,NULL,NULL);g_assert_cmpstr(data,==,"not a key file\n");
    g_assert_false(tuxflash_language_save(99,&error));g_assert_error(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT);g_clear_error(&error);
    g_unlink(path);
}
int main(int argc,char **argv)
{
    gboolean subprocess=FALSE;
    for (int i=1;i<argc;i++) if (g_str_has_prefix(argv[i],"--GTestSubprocess")) subprocess=TRUE;
    if (!subprocess) {
        char *dir=g_dir_make_tmp("tuxflash-language-test-XXXXXX",NULL);
        g_assert_nonnull(dir);g_setenv("XDG_CONFIG_HOME",dir,TRUE);g_free(dir);
    }
    g_test_init(&argc,&argv,NULL);
    /* The -p subprocess flag is parsed only by g_test_init(). */
    if (!g_test_subprocess()) {
        g_autofree char *path=settings_file();g_autofree char *dir=g_path_get_dirname(path);g_mkdir_with_parents(dir,0700);
    }
    g_test_add_func("/language/1-invalid-corrupt",test_invalid_and_corrupt);
    g_test_add_func("/language/2-preserve-private-settings",test_preserve_settings);
    g_test_add_func("/language/3-preference-survives-restart",test_restart_translation);
    g_test_add_func("/language/4-translated-release-notes", test_translated_release_notes);
    return g_test_run();
}
