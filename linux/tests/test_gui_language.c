/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main tuxflash_original_main
#include "../src/main.c"
#undef main
#include <gdk/x11/gdkx.h>

static char *opened_uri;
void __wrap_gtk_uri_launcher_launch(GtkUriLauncher *launcher,GtkWindow *parent,
 GCancellable *cancellable,GAsyncReadyCallback callback,gpointer data)
{
 (void)parent;(void)cancellable;(void)callback;(void)data;
 g_free(opened_uri);opened_uri=g_strdup(gtk_uri_launcher_get_uri(launcher));
}
GPtrArray *__wrap_lufus_devices_enumerate(gboolean fixed,GError **error)
{(void)fixed;(void)error;return g_ptr_array_new_with_free_func((GDestroyNotify)lufus_device_free);}
static gboolean has_label(GtkWidget *w,const char *text)
{
 if(GTK_IS_LABEL(w)&&g_strcmp0(gtk_label_get_text(GTK_LABEL(w)),text)==0)return TRUE;
 for(GtkWidget *c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c))if(has_label(c,text))return TRUE;
 return FALSE;
}
static GtkWidget *find_button(GtkWidget *w,const char *text)
{
 if(GTK_IS_BUTTON(w)&&g_strcmp0(gtk_button_get_label(GTK_BUTTON(w)),text)==0)return w;
 for(GtkWidget *c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c)) {GtkWidget *found=find_button(c,text);if(found)return found;}
 return NULL;
}
static GtkWindow *find_window(GType type)
{
 GListModel *list=gtk_window_get_toplevels();
 for(guint i=0;i<g_list_model_get_n_items(list);i++) {
  GtkWindow *w=g_list_model_get_item(list,i);gboolean found=g_type_is_a(G_OBJECT_TYPE(w),type)&&gtk_widget_get_visible(GTK_WIDGET(w));g_object_unref(w);if(found)return w;
 }
 return NULL;
}
static GtkWidget *find_type(GtkWidget *w,GType type)
{
 if(g_type_is_a(G_OBJECT_TYPE(w),type))return w;
 for(GtkWidget *c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c)) {GtkWidget *found=find_type(c,type);if(found)return found;}
 return NULL;
}
static gboolean has_text(GtkWidget *w,const char *text)
{
 if(GTK_IS_LABEL(w)&&strstr(gtk_label_get_text(GTK_LABEL(w)),text))return TRUE;
 if(GTK_IS_TEXT_VIEW(w)) {
  GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));GtkTextIter a,b;
  gtk_text_buffer_get_bounds(buffer,&a,&b);char *data=gtk_text_buffer_get_text(buffer,&a,&b,FALSE);
  gboolean found=strstr(data,text)!=NULL;g_free(data);if(found)return TRUE;
 }
 for(GtkWidget *c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c))if(has_text(c,text))return TRUE;
 return FALSE;
}
static void iterate(void){while(g_main_context_iteration(NULL,FALSE));}
static void check_writer_result(AppState *state, const char *script, gboolean success)
{
 GError *error = NULL;
 state->write_completed = state->writer_exited = state->writer_succeeded = FALSE;
 state->writer_eof = state->writer_read_failed = FALSE;
 gtk_label_set_text(state->status_label, "TESTING");
 state->writer = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE,
                                  &error, "/bin/sh", "-c", script, NULL);
 g_assert_no_error(error);
 state->writer_output = g_data_input_stream_new(g_subprocess_get_stdout_pipe(state->writer));
 read_writer_line(state); g_subprocess_wait_async(state->writer, NULL, writer_wait_finished, state);
 for (int i = 0; i < 500 && state->writer; i++) { iterate(); g_usleep(1000); }
 g_assert_null(state->writer);
 g_assert_cmpstr(gtk_label_get_text(state->status_label), ==, success ? _("READY") : _("ERROR"));
}
int main(int argc,char **argv)
{
 g_assert_cmpint(argc,==,4);
 setlocale(LC_ALL,"");gtk_disable_setlocale();
 g_autofree char *dir=g_build_filename(argv[1],"share","locale",NULL);
 tuxflash_language_init(dir);
 guint selected=tuxflash_language_selected();
 const char *ready[]={"PRONTO","READY","就绪","LISTO"};
 const char *device[]={"Dispositivo","Device","设备","Dispositivo"};
 g_assert_cmpstr(_("READY"),==,ready[selected]);
 adw_init();tuxflash_language_bind_catalogs(dir);
 g_assert_cmpstr(_("READY"),==,ready[selected]);
 AdwApplication *app=adw_application_new(APP_ID,G_APPLICATION_NON_UNIQUE);GError *error=NULL;
 g_assert_true(g_application_register(G_APPLICATION(app),NULL,&error));g_assert_no_error(error);
 AppState *state=create_window(app);gtk_window_present(GTK_WINDOW(state->window));iterate();
 g_assert_cmpstr(gtk_label_get_text(state->status_label),==,ready[selected]);
 g_assert_true(has_label(GTK_WIDGET(state->window),device[selected]));
 g_assert_true(has_label(GTK_WIDGET(state->window),_("Turn ISO images into bootable USB drives simply and safely.")));
 PangoLayout *layout=gtk_widget_create_pango_layout(GTK_WIDGET(state->window),"中文（简体）设备启动镜像");
 g_assert_cmpint(pango_layout_get_unknown_glyphs_count(layout),==,0);g_object_unref(layout);
 check_writer_result(state,"printf 'COMPLETE done\\n'; exit 0",TRUE);
 check_writer_result(state,"printf 'COMPLETE done\\n'; exit 7",FALSE);
 check_writer_result(state,"exit 0",FALSE);
 check_writer_result(state,"printf 'COMPLETE done\\n'; exit 0",TRUE);
 on_show_about(NULL,state);iterate();
 AdwAboutWindow *about=ADW_ABOUT_WINDOW(find_window(ADW_TYPE_ABOUT_WINDOW));g_assert_nonnull(about);
 g_assert_nonnull(strstr(adw_about_window_get_copyright(about),"Ricardo Barreto"));
 g_assert_cmpstr(adw_about_window_get_developer_name(about),==,_("Native bootable USB creator for Linux"));
 g_assert_cmpstr(adw_about_window_get_license(about),==,_(lufus_project_license_notice()));
 g_assert_cmpstr(adw_about_window_get_release_notes_version(about),==,LUFUS_VERSION);
 g_assert_nonnull(strstr(adw_about_window_get_release_notes(about),"Rufus"));
 g_assert_nonnull(strstr(adw_about_window_get_release_notes(about),"GPL-3.0-or-later"));
 g_assert_cmpstr(adw_about_window_get_website(about),==,"https://github.com/ricardogomesbarreto");
 AdwActionRow *row=find_credit_row(GTK_WIDGET(about),_("Ricardo Barreto — TuxFlash development"));g_assert_nonnull(row);
 GtkWidget *link=adw_action_row_get_activatable_widget(row);g_assert_true(GTK_IS_BUTTON(link));
 g_signal_emit_by_name(link,"clicked");g_assert_cmpstr(opened_uri,==,"https://github.com/ricardogomesbarreto");
 g_assert_nonnull(find_credit_row(GTK_WIDGET(about),_("Pete Batard and Rufus contributors — upstream")));
 g_assert_nonnull(find_credit_row(GTK_WIDGET(about),_("Open Research Tools contributors — Linux implementation")));
 row=find_credit_row(GTK_WIDGET(about),_("Lufus — base project for the Linux version"));g_assert_nonnull(row);
 link=adw_action_row_get_activatable_widget(row);g_assert_true(GTK_IS_BUTTON(link));
 g_assert_cmpstr(gtk_widget_get_tooltip_text(link),==,_("Open the Lufus repository"));
 g_signal_emit_by_name(link,"clicked");g_assert_cmpstr(opened_uri,==,"https://github.com/openresearchtools/lufus");
 gtk_window_destroy(GTK_WINDOW(about));iterate();
 on_language_clicked(NULL,state);iterate();
 AdwMessageDialog *dialog=ADW_MESSAGE_DIALOG(find_window(ADW_TYPE_MESSAGE_DIALOG));g_assert_nonnull(dialog);
 GtkDropDown *choice=g_object_get_data(G_OBJECT(dialog),"tuxflash-language-choice");
 g_assert_cmpuint(g_list_model_get_n_items(gtk_drop_down_get_model(choice)),==,4);
 g_assert_cmpuint(gtk_drop_down_get_selected(choice),==,selected);
 gtk_drop_down_set_selected(choice,(selected+1)%4);
 GtkWidget *save=find_button(GTK_WIDGET(dialog),_("Save"));g_assert_nonnull(save);
 g_signal_emit_by_name(save,"clicked");
 for(int i=0;i<50;i++){iterate();g_usleep(10000);}
 g_assert_cmpuint(tuxflash_language_selected(),==,(selected+1)%4);
 /* Saving must not change the active language or destroy an in-progress UI. */
 g_assert_cmpstr(_("READY"),==,ready[selected]);
 tuxflash_language_save(selected,NULL);
 state->hashing=TRUE;
 on_language_clicked(NULL,state);iterate();g_assert_null(find_window(ADW_TYPE_MESSAGE_DIALOG));state->hashing=FALSE;
 if(strcmp(argv[3],"language")==0){on_language_clicked(NULL,state);iterate();}
 GtkWindow *capture=strcmp(argv[3],"language")==0?find_window(ADW_TYPE_MESSAGE_DIALOG):GTK_WINDOW(state->window);
 if(strcmp(argv[3],"about")==0) {on_show_about(NULL,state);iterate();capture=find_window(ADW_TYPE_ABOUT_WINDOW);}
 if(strcmp(argv[3],"credits")==0) {
  on_show_about(NULL,state);iterate();capture=find_window(ADW_TYPE_ABOUT_WINDOW);
  GtkWidget *child=GTK_WIDGET(find_credit_row(GTK_WIDGET(capture),_("Ricardo Barreto — TuxFlash development")));
  GtkWidget *parent;
  while((parent=gtk_widget_get_parent(child))!=NULL) {
   if(GTK_IS_STACK(parent))gtk_stack_set_visible_child(GTK_STACK(parent),child);
   if(ADW_IS_LEAFLET(parent))adw_leaflet_set_visible_child(ADW_LEAFLET(parent),child);
   if(ADW_IS_NAVIGATION_VIEW(parent)&&ADW_IS_NAVIGATION_PAGE(child))adw_navigation_view_push(ADW_NAVIGATION_VIEW(parent),ADW_NAVIGATION_PAGE(child));
   child=parent;
  }
 }
 if(strcmp(argv[3],"legal")==0 || strcmp(argv[3],"notes")==0) {
  on_show_about(NULL,state);iterate();capture=find_window(ADW_TYPE_ABOUT_WINDOW);
  AdwNavigationView *nav=ADW_NAVIGATION_VIEW(find_type(GTK_WIDGET(capture),ADW_TYPE_NAVIGATION_VIEW));g_assert_nonnull(nav);
  adw_navigation_view_push_by_tag(nav,strcmp(argv[3],"legal")==0?"legal":"whatsnew");
  for(int i=0;i<70;i++){iterate();g_usleep(10000);}
  if(strcmp(argv[3],"legal")==0) {
   g_assert_true(has_text(GTK_WIDGET(capture),_(lufus_project_license_notice())));
   size_t count;const LufusLegalComponent *components=lufus_legal_components(&count);
   for(size_t i=0;i<count;i++) {
    g_assert_true(has_text(GTK_WIDGET(capture),_(components[i].title)));
    g_assert_true(has_text(GTK_WIDGET(capture),_(components[i].license)));
   }
  } else {
   g_assert_true(has_text(GTK_WIDGET(capture),_("Updated in 0.3.2")));
   g_assert_true(has_text(GTK_WIDGET(capture),_("Rufus, Lufus and TuxFlash")));
   g_assert_true(has_text(GTK_WIDGET(capture),_("Free and open source")));
   g_assert_true(has_text(GTK_WIDGET(capture),"FFU"));
  }
 }
 for(int i=0;i<120;i++){iterate();g_usleep(10000);}
 GdkSurface *surface=gtk_native_get_surface(GTK_NATIVE(capture));
 g_autofree char *json=g_strdup_printf("{\"xid\":%lu}\n",gdk_x11_surface_get_xid(surface));
 g_file_set_contents(argv[2],json,-1,&error);g_assert_no_error(error);
 g_print("PASS language=%u translated legal summaries and release notes, original attribution, GitHub target, operation guard, CJK glyphs\n",selected);fflush(stdout);
 for(int i=0;i<300;i++){iterate();g_usleep(10000);}
 GtkWindow *main_window=GTK_WINDOW(state->window);gboolean capture_is_main=capture==main_window;
 gtk_window_destroy(capture);if(!capture_is_main)gtk_window_destroy(main_window);
 g_free(opened_uri);g_object_unref(app);return 0;
}
