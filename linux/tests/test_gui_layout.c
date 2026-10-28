/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_original_main
#include "../src/main.c"
#undef main
#include <gdk/x11/gdkx.h>

GPtrArray *__wrap_lufus_devices_enumerate(gboolean fixed, GError **error)
{
    (void)fixed; (void)error;
    return g_ptr_array_new_with_free_func((GDestroyNotify)lufus_device_free);
}

static void settle(void)
{
    for (int i = 0; i < 50; i++) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(10000);
    }
}

static GtkWidget *find_type(GtkWidget *widget, GType type)
{
    if (g_type_is_a(G_OBJECT_TYPE(widget), type)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_type(child, type);
        if (found) return found;
    }
    return NULL;
}

static void expand_sections(GtkWidget *widget)
{
    if (GTK_IS_REVEALER(widget)) gtk_revealer_set_reveal_child(GTK_REVEALER(widget), TRUE);
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child)) expand_sections(child);
}

static void assert_footer(AppState *state)
{
    graphene_rect_t bounds;
    g_assert_true(gtk_widget_get_mapped(GTK_WIDGET(state->start_button)));
    g_assert_true(gtk_widget_compute_bounds(GTK_WIDGET(state->start_button), GTK_WIDGET(state->window), &bounds));
    g_assert_cmpfloat(bounds.origin.x, >=, 0);
    g_assert_cmpfloat(bounds.origin.y, >=, 0);
    g_assert_cmpfloat(bounds.origin.x + bounds.size.width, <=, gtk_widget_get_width(GTK_WIDGET(state->window)));
    g_assert_cmpfloat(bounds.origin.y + bounds.size.height, <=, gtk_widget_get_height(GTK_WIDGET(state->window)));
    g_assert_true(gtk_widget_is_ancestor(GTK_WIDGET(state->start_button), GTK_WIDGET(state->window)));
    GtkWidget *close = gtk_widget_get_next_sibling(GTK_WIDGET(state->start_button));
    g_assert_nonnull(close); g_assert_true(gtk_widget_get_mapped(close));
    g_assert_true(gtk_widget_compute_bounds(close, GTK_WIDGET(state->window), &bounds));
    g_assert_cmpfloat(bounds.origin.x + bounds.size.width, <=, gtk_widget_get_width(GTK_WIDGET(state->window)));
    g_assert_cmpfloat(bounds.origin.y + bounds.size.height, <=, gtk_widget_get_height(GTK_WIDGET(state->window)));
}

static void record(AppState *state, GtkScrolledWindow *scroller, const char *prefix, const char *stage)
{
    assert_footer(state);
    GtkPolicyType horizontal, vertical;
    gtk_scrolled_window_get_policy(scroller, &horizontal, &vertical);
    g_assert_cmpint(horizontal, ==, GTK_POLICY_NEVER);
    g_assert_cmpint(vertical, ==, GTK_POLICY_EXTERNAL);
    g_assert_false(gtk_widget_get_mapped(gtk_scrolled_window_get_hscrollbar(scroller)));
    g_assert_false(gtk_widget_get_mapped(gtk_scrolled_window_get_vscrollbar(scroller)));
    GtkAdjustment *h = gtk_scrolled_window_get_hadjustment(scroller);
    GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(scroller);
    g_assert_cmpfloat(gtk_adjustment_get_upper(h) - gtk_adjustment_get_page_size(h), <=, 1.0);
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(state->window));
    GListModel *monitors = gdk_display_get_monitors(gdk_surface_get_display(surface));
    GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
    GdkRectangle geometry; gdk_monitor_get_geometry(monitor, &geometry); g_object_unref(monitor);
    g_assert_cmpint(gtk_widget_get_width(GTK_WIDGET(state->window)), <=, geometry.width);
    g_assert_cmpint(gtk_widget_get_height(GTK_WIDGET(state->window)), <=, geometry.height);
    if (strcmp(stage, "basic") == 0 && geometry.height >= 768)
        g_assert_cmpfloat(gtk_adjustment_get_upper(v) - gtk_adjustment_get_page_size(v), <=, 1.0);
    g_autofree char *path = g_strdup_printf("%s-%s.json", prefix, stage);
    g_autofree char *json = g_strdup_printf(
        "{\"stage\":\"%s\",\"xid\":%lu,\"width\":%d,\"height\":%d,\"monitor_width\":%d,\"monitor_height\":%d,\"horizontal_overflow\":%.1f,\"vertical_range\":%.1f,\"footer_visible\":true,\"scrollbars_visible\":false}\n",
        stage, gdk_x11_surface_get_xid(surface), gtk_widget_get_width(GTK_WIDGET(state->window)),
        gtk_widget_get_height(GTK_WIDGET(state->window)), geometry.width, geometry.height,
        gtk_adjustment_get_upper(h) - gtk_adjustment_get_page_size(h),
        gtk_adjustment_get_upper(v) - gtk_adjustment_get_page_size(v));
    g_assert_true(g_file_set_contents(path, json, -1, NULL));
}

/* Exercise the GUI protocol and real async callbacks using pipe-only child
 * processes. Enumeration is mocked; no Polkit request or physical I/O occurs. */
static void writer_lifecycle_case(AppState *state, const char *script,
                                  gboolean request_cancel, const char *expected)
{
    g_assert_null(state->writer);
    GError *error = NULL;
    state->write_completed = FALSE;
    state->writer_exited = FALSE;
    state->writer_succeeded = FALSE;
    state->writer_eof = FALSE;
    state->writer_read_failed = FALSE;
    state->writer_cancel_requested = FALSE;
    state->writer = g_subprocess_new(G_SUBPROCESS_FLAGS_STDIN_PIPE |
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE,
        &error, "/bin/sh", "-c", script, NULL);
    g_assert_no_error(error); g_assert_nonnull(state->writer);
    state->writer_output = g_data_input_stream_new(g_subprocess_get_stdout_pipe(state->writer));
    gtk_button_set_label(state->start_button, _("CANCEL"));
    gtk_widget_set_sensitive(GTK_WIDGET(state->start_button), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(state->start_button), "destructive-action");
    gtk_label_set_text(state->status_label, _("REQUESTING AUTHORIZATION"));
    reset_progress_range(state);
    read_writer_line(state);
    g_subprocess_wait_async(state->writer, NULL, writer_wait_finished, state);
    if (request_cancel) {
        g_signal_emit_by_name(state->start_button, "clicked");
        g_assert_true(state->writer_cancel_requested);
        g_assert_false(gtk_widget_get_sensitive(GTK_WIDGET(state->start_button)));
        g_assert_cmpstr(gtk_label_get_text(state->status_label), ==, _("CANCELLING"));
        /* Repeated activation must not re-open or write a closed control pipe. */
        g_signal_emit_by_name(state->start_button, "clicked");
    }
    gint64 deadline = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    while (state->writer != NULL && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000);
    }
    g_assert_null(state->writer);
    g_assert_null(state->writer_output);
    g_assert_false(state->writer_cancel_requested);
    g_assert_cmpstr(gtk_label_get_text(state->status_label), ==, expected);
    g_assert_cmpstr(gtk_button_get_label(state->start_button), ==, _("START"));
    g_assert_false(gtk_widget_has_css_class(GTK_WIDGET(state->start_button), "destructive-action"));
    /* The fake enumeration contains no target: completion cannot enable Start. */
    g_assert_false(gtk_widget_get_sensitive(GTK_WIDGET(state->start_button)));
    if (state->refresh_source) { g_source_remove(state->refresh_source); state->refresh_source = 0; }
}

static void writer_lifecycle(AppState *state, const char *prefix)
{
    writer_lifecycle_case(state, "while IFS= read -r line; do :; done; exit 1", TRUE, _("CANCELLED"));
    writer_lifecycle_case(state, "printf 'ERROR Device changed\\n'; exit 1", FALSE, _("ERROR"));
    writer_lifecycle_case(state, "printf 'COMPLETE 64\\n'; exit 1", FALSE, _("ERROR"));
    writer_lifecycle_case(state, "exit 0", FALSE, _("ERROR"));
    writer_lifecycle_case(state, "printf 'COMPLETE 64\\n'; exit 0", FALSE, _("READY"));
    writer_lifecycle_case(state, "while IFS= read -r line; do :; done; printf 'COMPLETE 64\\n'; exit 0", TRUE, _("READY"));
    g_autofree char *path = g_strdup_printf("%s-writer.json", prefix);
    g_assert_true(g_file_set_contents(path,
        "{\"cases_passed\":6,\"physical_io\":false,\"polkit\":false}\n", -1, NULL));
}

static void wait_marker(const char *prefix, const char *name)
{
    g_autofree char *path = g_strdup_printf("%s-%s", prefix, name);
    gint64 deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
    while (!g_file_test(path, G_FILE_TEST_EXISTS) && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(10000);
    }
    g_assert_true(g_file_test(path, G_FILE_TEST_EXISTS));
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 3);
    setlocale(LC_ALL, ""); gtk_disable_setlocale();
    g_autofree char *catalogs = g_strdup(argv[1]);
    tuxflash_language_init(catalogs);
    adw_init(); tuxflash_language_bind_catalogs(catalogs);
    AdwApplication *app = adw_application_new(APP_ID, G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
    AppState *state = create_window(app);
    if (state->refresh_source) { g_source_remove(state->refresh_source); state->refresh_source = 0; }
    const char *labels[] = {"USB test device with a deliberately very long description to reproduce horizontal overflow at narrow window widths — 16 GB", NULL};
    set_dropdown_strings(state->device_dropdown, labels);
    gtk_window_present(GTK_WINDOW(state->window)); settle();
    GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW(find_type(GTK_WIDGET(state->window), GTK_TYPE_SCROLLED_WINDOW));
    g_assert_nonnull(scroller);
    record(state, scroller, argv[2], "basic"); wait_marker(argv[2], "advance");
    gtk_label_set_text(state->status_label,
        "VeryLongVerificationStatusWithoutSpacesToExerciseNarrowScreenWrappingAndKeepBothActionButtonsVisibleThroughoutTheOperation");
    expand_sections(GTK_WIDGET(scroller));
    gtk_widget_set_visible(state->windows_options_box, TRUE);
    settle();
    GtkAdjustment *v = gtk_scrolled_window_get_vadjustment(scroller);
    g_assert_cmpfloat(gtk_adjustment_get_upper(v), >, gtk_adjustment_get_page_size(v));
    gtk_adjustment_set_value(v, gtk_adjustment_get_upper(v) - gtk_adjustment_get_page_size(v));
    settle(); record(state, scroller, argv[2], "expanded"); wait_marker(argv[2], "done");
    writer_lifecycle(state, argv[2]);
    gtk_window_destroy(GTK_WINDOW(state->window)); g_object_unref(app);
    puts("PASS responsive layout, hidden scrollbars, reachable advanced fields and visible footer");
    return 0;
}
