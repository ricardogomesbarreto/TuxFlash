/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <glib.h>

#include "licenses.h"
#include "release_notes.h"
#include <string.h>

static void test_rufus_is_first(void)
{
    size_t count = 0;
    const LufusLegalComponent *components = lufus_legal_components(&count);
    g_assert_cmpuint(count, >, 0);
    g_assert_cmpstr(components[0].title, ==, "Rufus — upstream project");
    g_assert_cmpint(components[0].scope, ==, LUFUS_LEGAL_UPSTREAM);
    g_assert_nonnull(g_strstr_len(components[0].license, -1,
                                 "GPL-3.0-or-later"));
}
static void test_sections_are_complete_and_sorted(void)
{
    size_t count = 0;
    size_t index;
    const LufusLegalComponent *components = lufus_legal_components(&count);
    LufusLegalScope previous_scope = LUFUS_LEGAL_UPSTREAM;

    for (index = 0; index < count; index++) {
        size_t other;
        g_assert_nonnull(components[index].title);
        g_assert_nonnull(components[index].copyright);
        g_assert_nonnull(components[index].license);
        g_assert_cmpstr(components[index].title, !=, "");
        g_assert_cmpstr(components[index].copyright, !=, "");
        g_assert_cmpstr(components[index].license, !=, "");
        g_assert_cmpint(components[index].scope, >=, previous_scope);
        previous_scope = components[index].scope;
        for (other = index + 1; other < count; other++)
            g_assert_cmpstr(components[index].title, !=,
                            components[other].title);
    }
}

static void test_expected_legal_groups(void)
{
    size_t count = 0;
    size_t index;
    gboolean found_bcd_sys = FALSE;
    gboolean found_uefi_ntfs = FALSE;
    gboolean found_distribution = FALSE;
    const LufusLegalComponent *components = lufus_legal_components(&count);

    for (index = 0; index < count; index++) {
        found_bcd_sys |= g_strcmp0(components[index].title, "BCD-SYS") == 0;
        found_uefi_ntfs |= g_strcmp0(components[index].title, "UEFI:NTFS") == 0;
        found_distribution |= g_str_has_prefix(components[index].title,
                                               "Debian/Ubuntu");
    }
    g_assert_true(found_bcd_sys);
    g_assert_true(found_uefi_ntfs);
    g_assert_true(found_distribution);
}

static void check_element(GMarkupParseContext *context,const gchar *name,
                          const gchar **attributes,const gchar **values,
                          gpointer data,GError **error)
{
    (void)context;(void)attributes;(void)values;(void)data;
    if (strcmp(name,"notes") && strcmp(name,"p") && strcmp(name,"ul") && strcmp(name,"li"))
        g_set_error(error,G_MARKUP_ERROR,G_MARKUP_ERROR_UNKNOWN_ELEMENT,"Unexpected note element: %s",name);
}
static void test_release_notes_markup(void)
{
    g_autofree char *notes=tuxflash_release_notes();
    g_assert_nonnull(strstr(notes,"0.3.6-rc.1"));
    g_assert_nonnull(strstr(notes,"0.3.2"));
    g_assert_nonnull(strstr(notes,"Rufus"));
    g_assert_nonnull(strstr(notes,"Lufus"));
    g_assert_nonnull(strstr(notes,"TuxFlash"));
    g_assert_nonnull(strstr(notes,"GPL-3.0-or-later"));
    g_assert_nonnull(strstr(notes,"FFU"));
    g_autofree char *xml=g_strconcat("<notes>",notes,"</notes>",NULL);
    GMarkupParser parser={.start_element=check_element};
    GMarkupParseContext *ctx=g_markup_parse_context_new(&parser,0,NULL,NULL);
    GError *error=NULL;
    g_assert_true(g_markup_parse_context_parse(ctx,xml,-1,&error));g_assert_no_error(error);
    g_assert_true(g_markup_parse_context_end_parse(ctx,&error));g_assert_no_error(error);
    g_markup_parse_context_free(ctx);
}
static void test_current_license_notices(void)
{
    const char *notice=lufus_project_license_notice();
    g_assert_nonnull(strstr(notice,"GPL-3.0-or-later"));
    g_assert_nonnull(strstr(notice,"source/"));
    size_t count;const LufusLegalComponent *c=lufus_legal_components(&count);
    gboolean font=FALSE;
    for(size_t i=0;i<count;i++) {
        g_assert_null(strstr(c[i].license,"not bundled"));
        if(strcmp(c[i].title,"Noto Sans CJK")==0) {font=TRUE;g_assert_nonnull(strstr(c[i].license,"OFL-1.1"));}
    }
    g_assert_true(font);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/licenses/rufus-first", test_rufus_is_first);
    g_test_add_func("/licenses/complete-and-sorted",
                    test_sections_are_complete_and_sorted);
    g_test_add_func("/licenses/expected-groups", test_expected_legal_groups);
    g_test_add_func("/licenses/current-open-source-notices",test_current_license_notices);
    g_test_add_func("/licenses/release-notes-markup",test_release_notes_markup);
    return g_test_run();
}
