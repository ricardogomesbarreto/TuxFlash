/* Copyright (C) 2026 Ricardo Barreto
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "language.h"
#include <libintl.h>
#include <gio/gio.h>
#include <locale.h>
#include <glib/gstdio.h>
#include <errno.h>

static const char *codes[] = {"pt_BR", "en", "zh_CN", "es"};
static const char *names[] = {"Português (Brasil)", "English", "中文（简体）", "Español", NULL};
static guint current_language = 1;

const char *const *tuxflash_language_names(void) { return names; }
guint tuxflash_language_selected(void) { return current_language; }
static char *settings_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "tuxflash", "settings.ini", NULL);
}
static gboolean language_index(const char *code, guint *index)
{
    for (guint i=0;i<G_N_ELEMENTS(codes);i++)
        if (g_strcmp0(code,codes[i])==0) { *index=i; return TRUE; }
    return FALSE;
}
void tuxflash_language_init(const char *locale_directory)
{
    g_autofree char *path = settings_path();
    g_autoptr(GKeyFile) settings = g_key_file_new();
    g_key_file_load_from_file(settings, path, G_KEY_FILE_NONE, NULL);
    g_autofree char *saved = g_key_file_get_string(settings, "Interface", "Language", NULL);
    if (!language_index(saved, &current_language)) {
        const char *const *preferred = g_get_language_names();
        for (guint i=0;preferred[i]!=NULL;i++) {
            if (g_str_has_prefix(preferred[i],"pt")) {current_language=0;break;}
            if (g_str_has_prefix(preferred[i],"en")) {current_language=1;break;}
            if (g_str_has_prefix(preferred[i],"zh")) {current_language=2;break;}
            if (g_str_has_prefix(preferred[i],"es")) {current_language=3;break;}
        }
    }
    g_setenv("LANGUAGE",codes[current_language],TRUE);
    /* GNU gettext suppresses catalogs in C and C.UTF-8 on some libc
       versions. The portable runtime includes these real UTF-8 locales. */
    const char *locales[] = {"pt_BR.UTF-8", "en_US.UTF-8", "zh_CN.UTF-8", "es_ES.UTF-8"};
    const char *active = setlocale(LC_MESSAGES,locales[current_language]);
    if (active==NULL) {
        for (guint i=0;i<G_N_ELEMENTS(locales);i++) {
            active=setlocale(LC_MESSAGES,locales[i]);
            if (active!=NULL) break;
        }
    }
    if (active==NULL) setlocale(LC_MESSAGES,"");
    tuxflash_language_bind_catalogs(locale_directory);
}
void tuxflash_language_bind_catalogs(const char *locale_directory)
{
    const char *domains[] = {"tuxflash", "libadwaita", "gtk40", "glib20"};
    for (guint i=0;i<G_N_ELEMENTS(domains);i++) {
        bindtextdomain(domains[i],locale_directory);
        bind_textdomain_codeset(domains[i],"UTF-8");
    }
    textdomain("tuxflash");
}
gboolean tuxflash_language_save(guint selected, GError **error)
{
    if (selected>=G_N_ELEMENTS(codes)) {
        g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Invalid application language");
        return FALSE;
    }
    g_autofree char *path = settings_path();
    g_autofree char *dir = g_path_get_dirname(path);
    g_autoptr(GKeyFile) settings = g_key_file_new();
    GError *load_error = NULL;
    if (!g_key_file_load_from_file(settings,path,G_KEY_FILE_KEEP_COMMENTS,&load_error) &&
        !g_error_matches(load_error,G_FILE_ERROR,G_FILE_ERROR_NOENT)) {
        g_propagate_error(error,load_error);return FALSE;
    }
    g_clear_error(&load_error);
    if (g_mkdir_with_parents(dir,0700)!=0) {
        g_set_error(error,G_FILE_ERROR,g_file_error_from_errno(errno),"%s",g_strerror(errno));return FALSE;
    }
    g_key_file_set_string(settings,"Interface","Language",codes[selected]);
    gsize length;
    g_autofree char *data = g_key_file_to_data(settings,&length,error);
    gboolean saved = data!=NULL && g_file_set_contents_full(path,data,length,
        G_FILE_SET_CONTENTS_CONSISTENT|G_FILE_SET_CONTENTS_DURABLE,0600,error);
    if (saved) current_language = selected;
    return saved;
}
