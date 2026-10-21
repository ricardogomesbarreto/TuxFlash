#define main lufus_original_main
#include "main.c"
#undef main

int main(int argc, char **argv)
{
    GError *error = NULL;
    g_assert_cmpint(argc, ==, 2);
    adw_init();
    AdwApplication *application = adw_application_new(APP_ID, G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(application), NULL, &error));
    g_assert_no_error(error);
    AppState *state = create_window(application);
    g_assert_true(select_image_path(state, argv[1], &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(state->volume_label)), ==, "WINDOWS 10");
    g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(state->local_account)), ==, "User");
    gboolean found = FALSE;
    for (guint i = 0; i < state->image_option_count; i++) {
        if (state->image_options[i] == IMAGE_OPTION_WINDOWS_UEFI_NTFS) {
            gtk_drop_down_set_selected(state->image_option, i);
            on_image_option_selected(G_OBJECT(state->image_option), NULL, state);
            found = TRUE;
            break;
        }
    }
    g_assert_true(found);
    g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(state->volume_label)), ==, "Windows 10 PRO 2021");
    g_assert_null(strstr(state->log->str, "/home/"));
    g_print("GUI PASS: FAT32/NTFS labels match ISO name; generic account; log excludes home path\n");
    gtk_window_destroy(GTK_WINDOW(state->window));
    g_object_unref(application);
    return 0;
}
