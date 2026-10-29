/* SPDX-License-Identifier: GPL-3.0-or-later
 * VM-only integration fixture; no authentication bypass or production agent.
 * Built with the actual GUI code. Run only in the marked disposable guest. */
#define _GNU_SOURCE
#include <fcntl.h>
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>
#define main tuxflash_original_main
#include "main.c"
#undef main

typedef struct { PolkitAgentListener parent; } QaAgent;
typedef struct { PolkitAgentListenerClass parent; } QaAgentClass;
G_DEFINE_TYPE(QaAgent, qa_agent, POLKIT_AGENT_TYPE_LISTENER)
static AppState *gui;
static int mode, case_number, challenges, authentications, requests;
static const char *layout;
static gboolean cancellation_sent, root_observed;
static char *guest_secret;
static const char *work = "/var/lib/tuxflash-qa";

static void auth_completed(PolkitAgentSession *session, gboolean gained, gpointer data)
{
    GTask *task = data;
    if (gained) authentications++;
    g_task_return_boolean(task, TRUE);
    g_object_unref(task);
    g_object_unref(session);
}
static gboolean respond_later(gpointer data)
{
    PolkitAgentSession *session = data;
    polkit_agent_session_response(session, guest_secret);
    g_object_unref(session);
    return G_SOURCE_REMOVE;
}
static void auth_request(PolkitAgentSession *session, const char *prompt, gboolean echo, gpointer unused)
{
    (void)prompt; (void)echo; (void)unused;
    requests++;
    if (mode == 4 && !cancellation_sent) {
        g_signal_emit_by_name(gui->start_button, "clicked");
        cancellation_sent = TRUE;
        g_assert_true(gui->writer_cancel_requested);
        g_timeout_add(1000, respond_later, g_object_ref(session));
    } else polkit_agent_session_response(session, mode == 3 ? "intentionally-incorrect-qa-credential" : guest_secret);
}
static void initiate(PolkitAgentListener *listener, const gchar *action, const gchar *message,
    const gchar *icon, PolkitDetails *details, const gchar *cookie, GList *identities,
    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer data)
{
    (void)message; (void)icon; (void)details;
    g_assert_cmpstr(action, ==, strcmp(layout, "installed") == 0 ? "org.tuxflash.TuxFlash.write-image" : "org.freedesktop.policykit.exec");
    challenges++;
    GTask *task = g_task_new(listener, cancellable, callback, data);
    if (mode == 2) {
        g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "QA authentication dismissed");
        g_object_unref(task); return;
    }
    PolkitIdentity *identity = NULL;
    for (GList *p = identities; p; p = p->next)
        if (POLKIT_IS_UNIX_USER(p->data) && polkit_unix_user_get_uid(p->data) == (gint)getuid()) identity = p->data;
    g_assert_nonnull(identity);
    PolkitAgentSession *session = polkit_agent_session_new(identity, cookie);
    g_signal_connect(session, "request", G_CALLBACK(auth_request), NULL);
    g_signal_connect(session, "completed", G_CALLBACK(auth_completed), task);
    if (cancellable) g_signal_connect_object(cancellable, "cancelled", G_CALLBACK(polkit_agent_session_cancel), session, G_CONNECT_SWAPPED);
    polkit_agent_session_initiate(session);
}
static gboolean initiate_finish(PolkitAgentListener *listener, GAsyncResult *result, GError **error)
{ (void)listener; return g_task_propagate_boolean(G_TASK(result), error); }
static void qa_agent_class_init(QaAgentClass *klass)
{
    POLKIT_AGENT_LISTENER_CLASS(klass)->initiate_authentication = initiate;
    POLKIT_AGENT_LISTENER_CLASS(klass)->initiate_authentication_finish = initiate_finish;
}
static void qa_agent_init(QaAgent *agent) { (void)agent; }
static void wait_file(const char *name)
{
    g_autofree char *path = g_build_filename(work, name, NULL);
    gint64 deadline = g_get_monotonic_time() + 90 * G_USEC_PER_SEC;
    while (!g_file_test(path, G_FILE_TEST_EXISTS) && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {} g_usleep(10000);
    }
    g_assert_true(g_file_test(path, G_FILE_TEST_EXISTS));
}
static void marker(const char *phase)
{
    g_autofree char *path = g_strdup_printf("%s/%02d-%s", work, case_number, phase);
    g_assert_true(g_file_set_contents(path, "ready\n", -1, NULL));
}
static void inspect_root(void)
{
    const char *pid = gui->writer ? g_subprocess_get_identifier(gui->writer) : NULL;
    if (!pid) return;
    g_autofree char *path = g_strdup_printf("/proc/%s/status", pid);
    g_autofree char *status = NULL;
    if (g_file_get_contents(path, &status, NULL, NULL)) {
        const char *uids = strstr(status, "\nUid:");
        unsigned int real, effective;
        if (uids && sscanf(uids, "\nUid:\t%u\t%u", &real, &effective) == 2 && effective == 0) root_observed = TRUE;
    }
}
static void run_case(int number, const char *device)
{
    case_number = number; mode = number >= 11 ? number : (number - 1) % 5 + 1; challenges = authentications = requests = 0;
    cancellation_sent = root_observed = FALSE;
    g_string_truncate(gui->log, 0);
    g_autofree char *source = g_strdup_printf("/home/tuxqa/source-%02d.img", case_number);
    int fd = open(source, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    g_assert_cmpint(fd, >=, 0);
    unsigned char block[65536]; memset(block, 0x40 + mode, sizeof(block));
    for (int i=0;i<16;i++) g_assert_cmpint(write(fd, block, sizeof(block)), ==, sizeof(block));
    if (mode == 5 || mode >= 12) g_assert_cmpint(ftruncate(fd, 64 * 1024 * 1024), ==, 0);
    close(fd);
    g_free(gui->image_path); gui->image_path = g_strdup(source);
    g_autofree char *ack = g_strdup_printf("%02d-before.ready", case_number);
    marker("before"); wait_file(ack);
    GError *error = NULL;
    LufusJob *snapshot = lufus_job_new_raw(source, device, TRUE, &error);
    g_assert_no_error(error); g_assert_nonnull(snapshot);
    PendingWrite *pending = g_new0(PendingWrite, 1);
    pending->state = gui; pending->path = g_strdup(device); pending->major_minor = g_strdup(snapshot->major_minor);
    pending->device_size = snapshot->device_size; pending->disk_sequence = snapshot->disk_sequence;
    pending->type = LUFUS_JOB_RAW_WRITE; pending->runtime_validation = TRUE;
    lufus_job_free(snapshot);
    gui->image_size = (mode == 5 || mode >= 12) ? 64 * 1024 * 1024 : 1024 * 1024;
    gtk_widget_set_sensitive(GTK_WIDGET(gui->start_button), TRUE);
    begin_write(pending);
    g_assert_true(gtk_widget_get_sensitive(GTK_WIDGET(gui->start_button)));
    g_assert_nonnull(gui->writer);
    gint64 deadline = g_get_monotonic_time() + 90 * G_USEC_PER_SEC;
    while (gui->writer != NULL && g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        inspect_root();
        if (mode >= 12 && authentications > 0 && strstr(gui->log->str, "Writing image") != NULL) {
            g_autofree char *path = g_strdup_printf("%s/%02d-detached.json", work, case_number);
            const char *pid = g_subprocess_get_identifier(gui->writer);
            g_autofree char *data = g_strdup_printf("{\"case\":%d,\"helper_pid\":%s,\"auth_challenges\":%d,\"pam_authenticated\":%d}\n", case_number, pid, challenges, authentications);
            g_assert_true(g_file_set_contents(path, data, -1, NULL));
            /* Simulate an abrupt frontend death, not the Cancel callback. */
            _exit(0);
        }
        if (mode == 5 && !cancellation_sent && strstr(gui->log->str, "Writing image") != NULL) {
            g_signal_emit_by_name(gui->start_button, "clicked"); cancellation_sent = TRUE;
            g_assert_true(gui->writer_cancel_requested);
        }
        g_usleep(1000);
    }
    g_assert_null(gui->writer);
    const char *expected = mode == 1 ? _("READY") : (mode == 4 || mode == 5) ? _("CANCELLED") : _("ERROR");
    g_assert_cmpstr(gtk_label_get_text(gui->status_label), ==, expected);
    if (mode == 11) g_assert_cmpint(challenges, ==, 0);
    else g_assert_cmpint(challenges, >, 0);
    if (mode == 1 || mode == 4 || mode == 5) g_assert_cmpint(authentications, >, 0);
    else g_assert_cmpint(authentications, ==, 0);
    if (mode == 4 || mode == 5) g_assert_true(cancellation_sent);
    if (mode != 1) g_assert_false(gui->write_completed);
    if (mode == 2 || mode == 3 || mode == 4 || mode == 11) g_assert_null(strstr(gui->log->str, "Writing image"));
    g_autofree char *result_path = g_strdup_printf("%s/%02d-result.json", work, case_number);
    g_autofree char *result = g_strdup_printf("{\"case\":%d,\"passed\":true,\"auth_challenges\":%d,\"pam_authenticated\":%d,\"credential_requests\":%d,\"root_observed\":%s,\"cancelled\":%s,\"completed\":%s}\n", case_number, challenges, authentications, requests, root_observed?"true":"false", cancellation_sent?"true":"false", gui->write_completed?"true":"false");
    g_assert_true(g_file_set_contents(result_path, result, -1, NULL));
    if (mode != 11) {
    PolkitAuthority *authority = polkit_authority_get_sync(NULL, &error);
    g_assert_no_error(error);
    PolkitSubject *session_subject = polkit_unix_session_new_for_process_sync(getpid(), NULL, &error);
    g_assert_no_error(error);
    g_assert_true(polkit_authority_revoke_temporary_authorizations_sync(authority, session_subject, NULL, &error));
    g_object_unref(session_subject);
    g_assert_no_error(error); g_object_unref(authority);
    }
    marker("after");
    g_autofree char *after = g_strdup_printf("%02d-after.ready", case_number); wait_file(after);
    if (gui->refresh_source) { g_source_remove(gui->refresh_source); gui->refresh_source=0; }
    printf("POLKIT GUI PASS %d\n", case_number); fflush(stdout);
}
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    char *cmdline = NULL;
    if (!g_file_test("/etc/tuxflash-disposable-qa", G_FILE_TEST_IS_REGULAR) ||
        !g_file_get_contents("/proc/cmdline", &cmdline, NULL, NULL) ||
        strstr(cmdline, "tuxflash_qa=1") == NULL || getuid() != 1000) {
        g_printerr("This fixture requires the disposable guest user\n");
        g_free(cmdline); return 64;
    }
    g_free(cmdline);
    g_assert_true(g_file_get_contents("/home/tuxqa/.qa-secret", &guest_secret, NULL, NULL));
    g_strchomp(guest_secret);
    setlocale(LC_ALL, ""); gtk_disable_setlocale();
    tuxflash_language_init("/opt/tuxflash/runtime/share/locale");
    adw_init(); tuxflash_language_bind_catalogs("/opt/tuxflash/runtime/share/locale");
    AdwApplication *app = adw_application_new(APP_ID, G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
    layout = g_getenv("QA_LAYOUT"); g_assert_nonnull(layout);
    gui = create_window(app);
    if (gui->refresh_source) { g_source_remove(gui->refresh_source); gui->refresh_source=0; }
    gtk_window_present(GTK_WINDOW(gui->window));
    PolkitSubject *subject = polkit_unix_process_new_for_owner(getpid(), 0, getuid());
    PolkitAgentListener *listener = g_object_new(qa_agent_get_type(), NULL);
    GError *error = NULL;
    gpointer registration = polkit_agent_listener_register(listener, POLKIT_AGENT_REGISTER_FLAGS_NONE, subject, NULL, NULL, &error);
    g_assert_no_error(error); g_assert_nonnull(registration);
    char *device = NULL; g_assert_true(g_file_get_contents("/var/lib/tuxflash-qa/target", &device, NULL, NULL)); g_strchomp(device);
    g_assert_true(g_str_has_prefix(device, "/dev/sd"));
    int first = strcmp(layout, "installed") == 0 ? 1 : 6;
    const char *session_id = g_getenv("XDG_SESSION_ID"); g_assert_nonnull(session_id);
    g_assert_true(g_file_set_contents("/var/lib/tuxflash-qa/session-id", session_id, -1, NULL));
    for (int i=first;i<first+5;i++) run_case(i, device);
    if (first == 1) run_case(11, device);
    run_case(first == 1 ? 12 : 13, device);
    polkit_agent_listener_unregister(registration); g_object_unref(listener); g_object_unref(subject);
    gtk_window_destroy(GTK_WINDOW(gui->window)); g_object_unref(app);
    g_free(device); memset(guest_secret, 0, strlen(guest_secret)); g_free(guest_secret);
    return 0;
}
