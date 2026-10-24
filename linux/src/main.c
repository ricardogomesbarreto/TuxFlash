/*
 * Lufus GTK frontend
 * Copyright (C) 2026 Open Research Tools contributors
 * Copyright (C) 2026 Ricardo Barreto
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "checksums.h"
#include "cli.h"
#include "devices.h"
#include "downloads.h"
#include "image_probe.h"
#include "job.h"
#include "licenses.h"
#include "release_notes.h"
#include "volume_label.h"
#include "language.h"

#include <adwaita.h>
#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "status_translations.h"

#ifndef LUFUS_VERSION
#define LUFUS_VERSION "0.3.3"
#endif

#ifndef LUFUS_HELPER_PATH
#define LUFUS_HELPER_PATH "/usr/libexec/tuxflash-helper"
#endif

#define APP_ID "org.tuxflash.TuxFlash"

typedef enum {
    IMAGE_OPTION_RAW,
    IMAGE_OPTION_WINDOWS_UEFI_FAT32,
    IMAGE_OPTION_WINDOWS_UEFI_NTFS,
    IMAGE_OPTION_WINDOWS_BIOS_NTFS,
    IMAGE_OPTION_WINDOWS_BIOS_UEFI_NTFS,
    IMAGE_OPTION_WINDOWS_TO_GO,
    IMAGE_OPTION_LINUX_PERSISTENT,
} ImageOption;

typedef struct {
    AdwApplicationWindow *window;
    AdwToastOverlay *toast_overlay;
    GtkDropDown *device_dropdown;
    GtkDropDown *boot_dropdown;
    GtkDropDown *image_option;
    GtkWidget *windows_options_box;
    GtkWidget *persistence_options_box;
    GtkCheckButton *bypass_requirements;
    GtkCheckButton *no_online_account;
    GtkCheckButton *no_data_collection;
    GtkCheckButton *create_local_account;
    GtkCheckButton *disable_bitlocker;
    GtkCheckButton *offline_internal_drives;
    GtkCheckButton *silent_install;
    GtkEntry *local_account;
    GtkEntry *volume_label;
    GtkDropDown *windows_architecture;
    GtkSpinButton *windows_index;
    GtkSpinButton *silent_install_index;
    GtkWidget *silent_install_row;
    GtkWidget *windows_to_go_index_row;
    GtkSpinButton *persistence_size;
    GtkDropDown *target_system;
    GtkDropDown *partition_scheme;
    GtkDropDown *filesystem;
    GtkCheckButton *runtime_validation;
    GtkCheckButton *bad_blocks;
    GtkDropDown *test_passes;
    GtkCheckButton *list_fixed;
    GtkButton *hash_button;
    GtkButton *download_button;
    GtkButton *capture_button;
    GtkButton *start_button;
    GtkLabel *status_label;
    GtkProgressBar *progress;
    GPtrArray *devices;
    char *image_path;
    uint64_t image_size;
    LufusImageReport image_report;
    ImageOption image_options[7];
    guint image_option_count;
    gboolean hashing;
    GSubprocess *writer;
    GDataInputStream *writer_output;
    GCancellable *download_cancellable;
    gboolean write_completed;
    gboolean writer_exited;
    gboolean writer_succeeded;
    gboolean writer_eof;
    gboolean writer_read_failed;
    GString *log;
    guint refresh_source;
    guint progress_pulse_source;
    double progress_stage_start;
    double progress_stage_end;
    double progress_stage_last_relative;
    double progress_last_fraction;
    char *progress_stage_message;
    gboolean progress_stage_percent;
    gboolean progress_stage_text;
} AppState;

typedef struct {
    AppState *state;
    char *path;
    char *display_name;
    char *major_minor;
    uint64_t device_size;
    uint64_t disk_sequence;
    LufusJobType type;
    gboolean uefi_ntfs;
    gboolean runtime_validation;
    LufusWindowsTarget windows_target;
    guint64 persistence_size;
    guint windows_index;
    guint test_passes;
    char *output_path;
    LufusCaptureFormat capture_format;
    LufusWindowsOptions windows;
} PendingWrite;

static void refresh_devices(AppState *state, gboolean show_error);
static void update_start_sensitivity(AppState *state);
static void read_writer_line(AppState *state);
static void finish_writer_if_ready(AppState *state);
static void on_save_device(GtkButton *button, gpointer user_data);
static void on_download_clicked(GtkButton *button, gpointer user_data);

static void show_toast(AppState *state, const char *message)
{
    AdwToast *toast = adw_toast_new(_(message));
    adw_toast_set_timeout(toast, 5);
    adw_toast_overlay_add_toast(state->toast_overlay, toast);
}

static void append_log(AppState *state, const char *message)
{
    if (message == NULL || message[0] == '\0')
        return;
    if (state->log->len > 0)
        g_string_append_c(state->log, '\n');
    g_string_append(state->log, message);
}

static gboolean pulse_progress(gpointer user_data)
{
    AppState *state = user_data;
    gtk_progress_bar_pulse(state->progress);
    return G_SOURCE_CONTINUE;
}

static void stop_progress_pulse(AppState *state)
{
    if (state->progress_pulse_source != 0) {
        g_source_remove(state->progress_pulse_source);
        state->progress_pulse_source = 0;
    }
}

static void start_progress_pulse(AppState *state)
{
    if (state->progress_pulse_source == 0)
        state->progress_pulse_source = g_timeout_add(120, pulse_progress, state);
}

static void set_progress_fraction(AppState *state, double relative_fraction)
{
    double relative = CLAMP(relative_fraction, 0.0, 1.0);
    double fraction = state->progress_stage_start +
                      relative *
                      (state->progress_stage_end - state->progress_stage_start);
    char text[32];
    relative = MAX(relative, state->progress_stage_last_relative);
    state->progress_stage_last_relative = relative;
    fraction = state->progress_stage_start +
               relative * (state->progress_stage_end - state->progress_stage_start);
    stop_progress_pulse(state);
    fraction = CLAMP(fraction, 0.0, 1.0);
    fraction = MAX(fraction, state->progress_last_fraction);
    state->progress_last_fraction = fraction;
    gtk_progress_bar_set_fraction(state->progress, fraction);
    if (state->progress_stage_percent && state->progress_stage_message != NULL) {
        char *status = g_strdup_printf(_("%s: %.1f%%"), state->progress_stage_message,
                                       relative * 100.0);
        gtk_progress_bar_set_text(state->progress, status);
        g_free(status);
    } else if (state->progress_stage_text && state->progress_stage_message != NULL) {
        gtk_progress_bar_set_text(state->progress, state->progress_stage_message);
    } else {
        g_snprintf(text, sizeof(text), "%.0f%%", fraction * 100.0);
        gtk_progress_bar_set_text(state->progress, text);
    }
    gtk_progress_bar_set_show_text(state->progress, TRUE);
}

static void reset_progress_range(AppState *state)
{
    state->progress_stage_start = 0.0;
    state->progress_stage_end = 1.0;
    state->progress_stage_last_relative = 0.0;
    state->progress_last_fraction = 0.0;
    g_clear_pointer(&state->progress_stage_message, g_free);
    state->progress_stage_percent = FALSE;
    state->progress_stage_text = FALSE;
}

static GtkWidget *new_dropdown(const char *const *values)
{
    GtkStringList *model = gtk_string_list_new(values);
    GtkWidget *dropdown = gtk_drop_down_new(G_LIST_MODEL(model), NULL);
    /* gtk_drop_down_new() takes full ownership of both constructor arguments. */
    gtk_widget_set_hexpand(dropdown, TRUE);
    return dropdown;
}

static void set_dropdown_strings(GtkDropDown *dropdown, const char *const *values)
{
    GtkStringList *model = gtk_string_list_new(values);
    gtk_drop_down_set_model(dropdown, G_LIST_MODEL(model));
    g_object_unref(model);
}

static GtkWidget *new_label(const char *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "dim-label");
    return label;
}

static GtkWidget *new_field(const char *title, GtkWidget *control)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_box_append(GTK_BOX(box), new_label(title));
    gtk_box_append(GTK_BOX(box), control);
    return box;
}

static void append_heading(GtkBox *box, const char *title)
{
    GtkWidget *label = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_widget_set_margin_top(label, 7);
    gtk_box_append(box, label);
    gtk_box_append(box, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
}

static LufusDevice *selected_device(AppState *state)
{
    guint selected = gtk_drop_down_get_selected(state->device_dropdown);
    if (selected == GTK_INVALID_LIST_POSITION || state->devices == NULL || selected >= state->devices->len)
        return NULL;
    return g_ptr_array_index(state->devices, selected);
}

static void update_start_sensitivity(AppState *state)
{
    gboolean has_device = selected_device(state) != NULL;
    gtk_widget_set_sensitive(GTK_WIDGET(state->start_button),
                             state->writer != NULL ||
                             (state->image_path != NULL && has_device));
    if (state->capture_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(state->capture_button),
                                 state->writer == NULL && has_device);
}

static gboolean devices_equal(GPtrArray *left, GPtrArray *right)
{
    guint index;
    if (left == NULL || right == NULL || left->len != right->len)
        return FALSE;
    for (index = 0; index < left->len; index++) {
        LufusDevice *a = g_ptr_array_index(left, index);
        LufusDevice *b = g_ptr_array_index(right, index);
        if (g_strcmp0(a->path, b->path) != 0 || a->size != b->size ||
            g_strcmp0(a->major_minor, b->major_minor) != 0)
            return FALSE;
    }
    return TRUE;
}

static void refresh_devices(AppState *state, gboolean show_error)
{
    GError *error = NULL;
    GPtrArray *devices;
    GPtrArray *labels;
    char *selected_path = NULL;
    LufusDevice *current = selected_device(state);
    guint index;
    guint selection = 0;
    if (state->writer != NULL)
        return;
    if (current != NULL)
        selected_path = g_strdup(current->path);
    devices = lufus_devices_enumerate(gtk_check_button_get_active(state->list_fixed), &error);
    if (devices == NULL) {
        if (show_error)
            show_toast(state, error->message);
        g_clear_error(&error);
        g_free(selected_path);
        return;
    }
    if (devices_equal(state->devices, devices)) {
        g_ptr_array_unref(devices);
        g_free(selected_path);
        return;
    }
    labels = g_ptr_array_new_with_free_func(g_free);
    for (index = 0; index < devices->len; index++) {
        LufusDevice *device = g_ptr_array_index(devices, index);
        g_ptr_array_add(labels, lufus_device_display_name(device));
        if (selected_path != NULL && strcmp(selected_path, device->path) == 0)
            selection = index;
    }
    if (labels->len == 0)
        g_ptr_array_add(labels, g_strdup(_("No compatible devices found")));
    g_ptr_array_add(labels, NULL);
    set_dropdown_strings(state->device_dropdown, (const char *const *)labels->pdata);
    gtk_drop_down_set_selected(state->device_dropdown, selection);
    g_ptr_array_unref(labels);
    if (state->devices != NULL)
        g_ptr_array_unref(state->devices);
    state->devices = devices;
    g_free(selected_path);
    update_start_sensitivity(state);
}

static gboolean device_refresh_timer(gpointer user_data)
{
    refresh_devices(user_data, FALSE);
    return G_SOURCE_CONTINUE;
}

static void on_refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    refresh_devices(user_data, TRUE);
}

static void on_list_fixed_toggled(GtkCheckButton *button, gpointer user_data)
{
    (void)button;
    refresh_devices(user_data, TRUE);
}

static void on_device_selected(GObject *object, GParamSpec *property, gpointer user_data)
{
    (void)object;
    (void)property;
    update_start_sensitivity(user_data);
}

static ImageOption selected_image_option(AppState *state)
{
    guint selected = gtk_drop_down_get_selected(state->image_option);
    if (selected == GTK_INVALID_LIST_POSITION ||
        selected >= state->image_option_count)
        return IMAGE_OPTION_RAW;
    return state->image_options[selected];
}

static void add_image_option(AppState *state, GPtrArray *labels,
                             ImageOption option, const char *label)
{
    g_return_if_fail(state->image_option_count < G_N_ELEMENTS(state->image_options));
    state->image_options[state->image_option_count++] = option;
    g_ptr_array_add(labels, (gpointer)label);
}

static void configure_image_options(AppState *state)
{
    GPtrArray *labels = g_ptr_array_new();

    state->image_option_count = 0;
    if (state->image_report.supports_windows_installer) {
        if (state->image_report.has_efi) {
            add_image_option(state, labels, IMAGE_OPTION_WINDOWS_UEFI_FAT32,
                             _("Standard Windows installation (UEFI/FAT32)"));
            add_image_option(state, labels, IMAGE_OPTION_WINDOWS_UEFI_NTFS,
                             _("Standard Windows installation (UEFI:NTFS)"));
        }
        if (state->image_report.has_bootmgr || state->image_report.has_winpe)
            add_image_option(state, labels, IMAGE_OPTION_WINDOWS_BIOS_NTFS,
                             _("Standard Windows installation (BIOS/NTFS)"));
        if (state->image_report.has_efi &&
            (state->image_report.has_bootmgr || state->image_report.has_winpe))
            add_image_option(state, labels, IMAGE_OPTION_WINDOWS_BIOS_UEFI_NTFS,
                             _("Standard Windows installation (BIOS + UEFI/NTFS)"));
        if (state->image_report.supports_windows_to_go)
            add_image_option(state, labels, IMAGE_OPTION_WINDOWS_TO_GO,
                             _("Windows To Go"));
    }

    /* Rufus always retains DD mode for an ISOHybrid image. It is the safe
     * default for non-Windows media because it preserves the source image's
     * partition table and boot records exactly. */
    add_image_option(state, labels, IMAGE_OPTION_RAW,
                     _("Write in DD Image mode"));
    if (state->image_report.supports_persistence)
        add_image_option(state, labels, IMAGE_OPTION_LINUX_PERSISTENT,
                         _("Persistent Linux live media"));
    g_ptr_array_add(labels, NULL);
    set_dropdown_strings(state->image_option,
                         (const char *const *)labels->pdata);
    gtk_drop_down_set_selected(state->image_option, 0);
    g_ptr_array_unref(labels);
}

static void on_image_option_selected(GObject *object, GParamSpec *property, gpointer user_data)
{
    AppState *state = user_data;
    ImageOption selected = selected_image_option(state);
    gboolean windows_installer =
        selected == IMAGE_OPTION_WINDOWS_UEFI_FAT32 ||
        selected == IMAGE_OPTION_WINDOWS_UEFI_NTFS ||
        selected == IMAGE_OPTION_WINDOWS_BIOS_NTFS ||
        selected == IMAGE_OPTION_WINDOWS_BIOS_UEFI_NTFS;
    gboolean windows_to_go = selected == IMAGE_OPTION_WINDOWS_TO_GO;
    gboolean linux_persistent = selected == IMAGE_OPTION_LINUX_PERSISTENT;
    gboolean validation_supported = windows_installer || linux_persistent ||
                                    selected == IMAGE_OPTION_RAW;
    (void)object;
    (void)property;
    if (state->volume_label != NULL) {
        char *label = windows_installer || windows_to_go ?
            lufus_volume_label_for_image(state->image_path,
                selected == IMAGE_OPTION_WINDOWS_UEFI_FAT32) :
            g_strdup(linux_persistent ? "LUFUS-LIVE" : "");
        gtk_editable_set_text(GTK_EDITABLE(state->volume_label), label);
        g_free(label);
    }
    gtk_widget_set_visible(state->windows_options_box,
                           windows_installer || windows_to_go);
    gtk_widget_set_visible(state->persistence_options_box,
                           linux_persistent);
    gtk_widget_set_visible(state->silent_install_row, windows_installer);
    gtk_widget_set_visible(state->windows_to_go_index_row, windows_to_go);
    if (!windows_installer)
        gtk_check_button_set_active(state->silent_install, FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(state->runtime_validation),
                             validation_supported && state->image_path != NULL);
    if (selected == IMAGE_OPTION_WINDOWS_BIOS_NTFS)
        gtk_drop_down_set_selected(state->target_system, 0);
    else if (selected == IMAGE_OPTION_WINDOWS_BIOS_UEFI_NTFS)
        gtk_drop_down_set_selected(state->target_system, 2);
    else if (windows_installer || windows_to_go || linux_persistent)
        gtk_drop_down_set_selected(state->target_system, 1);
    else
        gtk_drop_down_set_selected(state->target_system, 3);
    if (state->partition_scheme != NULL)
        gtk_drop_down_set_selected(state->partition_scheme,
            selected == IMAGE_OPTION_RAW ? 2 :
            (selected == IMAGE_OPTION_WINDOWS_BIOS_NTFS ||
             selected == IMAGE_OPTION_WINDOWS_BIOS_UEFI_NTFS ? 0 : 1));
    if (state->filesystem != NULL)
        gtk_drop_down_set_selected(state->filesystem,
            selected == IMAGE_OPTION_RAW ? 6 :
            selected == IMAGE_OPTION_WINDOWS_UEFI_FAT32 ? 0 : 1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->runtime_validation),
        selected == IMAGE_OPTION_RAW ?
        _("Read the USB back and compare it with the image after writing.") :
        _("Verify the completed media and install the UEFI validation manifest."));
}

static void on_silent_install_toggled(GtkCheckButton *button, gpointer user_data)
{
    AppState *state = user_data;
    gboolean active = gtk_check_button_get_active(button);

    gtk_widget_set_sensitive(GTK_WIDGET(state->silent_install_index), active);
    if (active) {
        /* Rufus only exposes silent installation when these unattended OOBE
         * choices are active.  Keep the resulting medium self-consistent. */
        gtk_check_button_set_active(state->no_online_account, TRUE);
        gtk_check_button_set_active(state->no_data_collection, TRUE);
        gtk_check_button_set_active(state->create_local_account, TRUE);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(state->no_online_account), !active);
    gtk_widget_set_sensitive(GTK_WIDGET(state->no_data_collection), !active);
    gtk_widget_set_sensitive(GTK_WIDGET(state->create_local_account), !active);
}

static gboolean select_image_path(AppState *state, const char *path, GError **error)
{
    GStatBuf details;
    LufusImageReport report = {0};
    char *size;
    char *description;
    char *basename;
    const char *extension;
    const char *strings[2];
    gboolean iso_extension;
    if (path == NULL || g_stat(path, &details) != 0 || !S_ISREG(details.st_mode)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            _("Only local regular image files are supported"));
        return FALSE;
    }
    extension = strrchr(path, '.');
    iso_extension = extension != NULL &&
                    g_ascii_strcasecmp(extension, ".iso") == 0;
    if (iso_extension && !lufus_image_probe_iso(path, &report, error))
        return FALSE;
    g_free(state->image_path);
    state->image_path = g_strdup(path);
    state->image_size = (uint64_t)details.st_size;
    state->image_report = report;
    size = lufus_format_size(state->image_size);
    basename = g_path_get_basename(path);
    description = g_strdup_printf(_("%s — %s"), basename, size);
    strings[0] = description;
    strings[1] = NULL;
    set_dropdown_strings(state->boot_dropdown, strings);
    gtk_drop_down_set_selected(state->boot_dropdown, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(state->hash_button), TRUE);
    configure_image_options(state);
    on_image_option_selected(G_OBJECT(state->image_option), NULL, state);
    gtk_label_set_text(state->status_label, _("READY"));
    append_log(state, basename);
    if (state->image_report.supports_windows_installer)
        append_log(state, _("Detected Windows installation image"));
    else if (state->image_report.supports_persistence)
        append_log(state, _("Detected persistent-capable Linux live image"));
    else if (state->image_report.is_iso)
        append_log(state, _("Detected non-Windows ISO; DD image mode selected"));
    g_free(basename);
    g_free(description);
    g_free(size);
    update_start_sensitivity(state);
    return TRUE;
}

static void image_dialog_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AppState *state = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    char *path;
    if (file == NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
            !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
            show_toast(state, error->message);
        g_clear_error(&error);
        return;
    }
    path = g_file_get_path(file);
    g_object_unref(file);
    if (!select_image_path(state, path, &error)) {
        show_toast(state, error->message);
        g_clear_error(&error);
    }
    g_free(path);
}

static void on_select_image(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    GtkFileFilter *images = gtk_file_filter_new();
    GtkFileFilter *all_files = gtk_file_filter_new();
    const char *patterns[] = {
        "*.iso", "*.img", "*.raw", "*.dd", "*.vhd", "*.vhdx",
        "*.gz", "*.xz", "*.bz2", "*.zst", "*.zstd", NULL
    };
    guint index;
    (void)button;
    gtk_file_dialog_set_title(dialog, _("Select a bootable disk image"));
    gtk_file_dialog_set_modal(dialog, TRUE);
    gtk_file_filter_set_name(images, _("Disk images"));
    for (index = 0; patterns[index] != NULL; index++)
        gtk_file_filter_add_pattern(images, patterns[index]);
    gtk_file_filter_set_name(all_files, _("All files"));
    gtk_file_filter_add_pattern(all_files, "*");
    g_list_store_append(filters, images);
    g_list_store_append(filters, all_files);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_open(dialog, GTK_WINDOW(state->window), NULL, image_dialog_finished, state);
    g_object_unref(images);
    g_object_unref(all_files);
    g_object_unref(filters);
    g_object_unref(dialog);
}

typedef struct {
    AppState *state;
    char *release;
    char *language;
    char *architecture;
    char *output;
} DownloadTask;

typedef struct {
    AppState *state;
    double fraction;
} DownloadProgressUpdate;

static void download_task_free(DownloadTask *task)
{
    g_free(task->release);
    g_free(task->language);
    g_free(task->architecture);
    g_free(task->output);
    g_free(task);
}

static gboolean update_download_progress(gpointer user_data)
{
    DownloadProgressUpdate *update = user_data;
    if (update->state->download_cancellable != NULL) {
        if (update->fraction > 0.0)
            set_progress_fraction(update->state, update->fraction);
        else
            start_progress_pulse(update->state);
    }
    g_free(update);
    return G_SOURCE_REMOVE;
}

static void download_progress(guint64 completed, guint64 total, gpointer user_data)
{
    DownloadTask *task = user_data;
    DownloadProgressUpdate *update = g_new0(DownloadProgressUpdate, 1);
    update->state = task->state;
    update->fraction = total > 0 ? MIN(1.0, (double)completed / (double)total) : 0.0;
    g_main_context_invoke(NULL, update_download_progress, update);
}

static void download_windows_iso(GTask *task, gpointer source_object,
                                 gpointer task_data, GCancellable *cancellable)
{
    DownloadTask *request = task_data;
    GError *error = NULL;
    LufusWindowsCatalog *catalog;
    const LufusWindowsLanguage *language;
    char *url;
    (void)source_object;
    catalog = lufus_windows_catalog_discover_for_architecture(
        request->release, "en-US", request->architecture, cancellable, &error);
    if (catalog == NULL) {
        g_task_return_error(task, error);
        return;
    }
    language = lufus_windows_catalog_find_language(catalog, request->language);
    if (language == NULL) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                "Microsoft is not currently offering %s", request->language);
        lufus_windows_catalog_free(catalog);
        return;
    }
    url = lufus_windows_catalog_resolve(catalog, language, request->architecture,
                                        cancellable, &error);
    if (url == NULL || !lufus_download_url(url, request->output, download_progress,
                                           request, cancellable, &error)) {
        g_free(url);
        lufus_windows_catalog_free(catalog);
        g_task_return_error(task, error);
        return;
    }
    g_free(url);
    lufus_windows_catalog_free(catalog);
    g_task_return_pointer(task, g_strdup(request->output), g_free);
}

static void microsoft_download_error_response(AdwMessageDialog *dialog,
                                               const char *response,
                                               gpointer user_data)
{
    AppState *state = user_data;
    if (g_strcmp0(response, "website") == 0) {
        const char *page = g_object_get_data(G_OBJECT(dialog),
                                             "lufus-microsoft-page");
        if (page != NULL) {
            GtkUriLauncher *launcher = gtk_uri_launcher_new(page);
            gtk_uri_launcher_launch(launcher, GTK_WINDOW(state->window),
                                    NULL, NULL, NULL);
            g_object_unref(launcher);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_microsoft_download_error(AppState *state, const char *release,
                                          const char *message)
{
    const char *page = lufus_windows_release_download_page(release);
    AdwMessageDialog *dialog = ADW_MESSAGE_DIALOG(adw_message_dialog_new(
        GTK_WINDOW(state->window), _("Windows ISO download failed"), message));
    adw_message_dialog_add_response(dialog, "close", _("Close"));
    if (page != NULL) {
        adw_message_dialog_add_response(dialog, "website",
                                        _("Microsoft Website"));
        adw_message_dialog_set_response_appearance(
            dialog, "website", ADW_RESPONSE_SUGGESTED);
        g_object_set_data_full(G_OBJECT(dialog), "lufus-microsoft-page",
                               g_strdup(page), g_free);
    }
    adw_message_dialog_set_default_response(dialog, "close");
    adw_message_dialog_set_close_response(dialog, "close");
    g_signal_connect(dialog, "response",
                     G_CALLBACK(microsoft_download_error_response), state);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void windows_download_finished(GObject *source, GAsyncResult *result,
                                      gpointer user_data)
{
    AppState *state = user_data;
    DownloadTask *request = g_task_get_task_data(G_TASK(result));
    GError *error = NULL;
    char *path = g_task_propagate_pointer(G_TASK(result), &error);
    (void)source;
    stop_progress_pulse(state);
    g_clear_object(&state->download_cancellable);
    gtk_button_set_icon_name(state->download_button, "folder-download-symbolic");
    gtk_widget_set_sensitive(GTK_WIDGET(state->download_button), TRUE);
    gtk_progress_bar_set_fraction(state->progress, path != NULL ? 1.0 : 0.0);
    gtk_progress_bar_set_text(state->progress, path != NULL ? "100%" : NULL);
    gtk_progress_bar_set_show_text(state->progress, path != NULL);
    gtk_label_set_text(state->status_label, path != NULL ? _("READY") : _("ERROR"));
    if (path == NULL) {
        append_log(state, error->message);
        show_microsoft_download_error(state, request->release, error->message);
        g_clear_error(&error);
        return;
    }
    if (!select_image_path(state, path, &error)) {
        show_toast(state, error->message);
        g_clear_error(&error);
    } else {
        show_toast(state, _("Official Microsoft ISO downloaded and selected"));
    }
    g_free(path);
}

static void download_save_finished(GObject *source, GAsyncResult *result,
                                   gpointer user_data)
{
    DownloadTask *request = user_data;
    AppState *state = request->state;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);
    GTask *task;
    if (file == NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
            !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
            show_toast(state, error->message);
        g_clear_error(&error);
        download_task_free(request);
        return;
    }
    request->output = g_file_get_path(file);
    g_object_unref(file);
    if (request->output == NULL || g_file_test(request->output, G_FILE_TEST_EXISTS)) {
        show_toast(state, _("Choose a new local output file"));
        download_task_free(request);
        return;
    }
    state->download_cancellable = g_cancellable_new();
    gtk_button_set_icon_name(state->download_button, "process-stop-symbolic");
    gtk_label_set_text(state->status_label, _("DOWNLOADING WINDOWS ISO"));
    reset_progress_range(state);
    gtk_progress_bar_set_fraction(state->progress, 0.0);
    gtk_progress_bar_set_text(state->progress, _("Contacting Microsoft…"));
    gtk_progress_bar_set_show_text(state->progress, TRUE);
    start_progress_pulse(state);
    task = g_task_new(NULL, state->download_cancellable, windows_download_finished, state);
    g_task_set_task_data(task, request, (GDestroyNotify)download_task_free);
    g_task_run_in_thread(task, download_windows_iso);
    g_object_unref(task);
}

typedef struct {
    AppState *state;
    GtkDropDown *release;
    GtkDropDown *language;
    GtkDropDown *architecture;
} DownloadDialog;

static const char *dropdown_text(GtkDropDown *dropdown)
{
    GtkStringObject *item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dropdown));
    return item != NULL ? gtk_string_object_get_string(item) : "";
}

static void download_dialog_response(AdwMessageDialog *dialog, const char *response,
                                     gpointer user_data)
{
    DownloadDialog *selection = user_data;
    if (g_strcmp0(response, "download") == 0) {
        DownloadTask *request = g_new0(DownloadTask, 1);
        GtkFileDialog *save = gtk_file_dialog_new();
        request->state = selection->state;
        request->release = g_strdup(gtk_drop_down_get_selected(selection->release) == 0 ?
                                    "windows11" : "windows10");
        request->language = g_strdup(dropdown_text(selection->language));
        request->architecture = g_ascii_strdown(dropdown_text(selection->architecture), -1);
        gtk_file_dialog_set_title(save, _("Save official Windows ISO"));
        gtk_file_dialog_set_initial_name(save,
            gtk_drop_down_get_selected(selection->release) == 0 ?
            "Windows11.iso" : "Windows10.iso");
        gtk_file_dialog_save(save, GTK_WINDOW(selection->state->window), NULL,
                             download_save_finished, request);
        g_object_unref(save);
    } else if (g_strcmp0(response, "website") == 0) {
        const char *release = gtk_drop_down_get_selected(selection->release) == 0 ?
                              "windows11" : "windows10";
        GtkUriLauncher *launcher = gtk_uri_launcher_new(
            lufus_windows_release_download_page(release));
        gtk_uri_launcher_launch(launcher, GTK_WINDOW(selection->state->window),
                                NULL, NULL, NULL);
        g_object_unref(launcher);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(selection);
}

static void on_download_clicked(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    static const char *releases[] = {"Windows 11 25H2", "Windows 10 22H2", NULL};
    static const char *languages[] = {
        "English International", "English", "Arabic", "Brazilian Portuguese",
        "Bulgarian", "Chinese (Simplified)", "Chinese (Traditional)", "Croatian",
        "Czech", "Danish", "Dutch", "Estonian", "Finnish", "French",
        "French Canadian", "German", "Greek", "Hebrew", "Hungarian", "Italian",
        "Japanese", "Korean", "Latvian", "Lithuanian", "Norwegian", "Polish",
        "Portuguese", "Romanian", "Russian", "Serbian Latin", "Slovak",
        "Slovenian", "Spanish", "Spanish (Mexico)", "Swedish", "Thai",
        "Turkish", "Ukrainian", NULL
    };
    static const char *architectures[] = {"x64", "ARM64", "x86", NULL};
    AdwMessageDialog *dialog;
    GtkWidget *grid;
    DownloadDialog *selection;
    (void)button;
    if (state->download_cancellable != NULL) {
        g_cancellable_cancel(state->download_cancellable);
        gtk_widget_set_sensitive(GTK_WIDGET(state->download_button), FALSE);
        return;
    }
    dialog = ADW_MESSAGE_DIALOG(adw_message_dialog_new(GTK_WINDOW(state->window),
        _("Download an official Windows ISO"),
        _("TuxFlash obtains a temporary link from Microsoft and downloads the ISO directly from Microsoft.")));
    selection = g_new0(DownloadDialog, 1);
    selection->state = state;
    selection->release = GTK_DROP_DOWN(new_dropdown(releases));
    selection->language = GTK_DROP_DOWN(new_dropdown(languages));
    selection->architecture = GTK_DROP_DOWN(new_dropdown(architectures));
    grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(grid), new_field(_("Release"), GTK_WIDGET(selection->release)));
    gtk_box_append(GTK_BOX(grid), new_field(_("Language"), GTK_WIDGET(selection->language)));
    gtk_box_append(GTK_BOX(grid), new_field(_("Architecture"), GTK_WIDGET(selection->architecture)));
    adw_message_dialog_set_extra_child(dialog, grid);
    adw_message_dialog_add_responses(dialog, "cancel", _("Cancel"),
                                     "website", _("Microsoft Website"),
                                     "download", _("Download"), NULL);
    adw_message_dialog_set_default_response(dialog, "download");
    adw_message_dialog_set_response_appearance(dialog, "download", ADW_RESPONSE_SUGGESTED);
    g_signal_connect(dialog, "response", G_CALLBACK(download_dialog_response), selection);
    gtk_window_present(GTK_WINDOW(dialog));
}

typedef struct {
    char *path;
} HashTask;

static void hash_task_free(HashTask *task)
{
    g_free(task->path);
    g_free(task);
}

static void calculate_hashes(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    HashTask *data = task_data;
    GError *error = NULL;
    char *output;
    (void)source_object;
    output = lufus_checksums_file(data->path, cancellable, &error);
    if (output == NULL)
        g_task_return_error(task, error);
    else
        g_task_return_pointer(task, output, g_free);
}

static void hashes_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AppState *state = user_data;
    GError *error = NULL;
    char *body = g_task_propagate_pointer(G_TASK(result), &error);
    (void)source;
    state->hashing = FALSE;
    gtk_widget_set_sensitive(GTK_WIDGET(state->hash_button), TRUE);
    gtk_label_set_text(state->status_label, _("READY"));
    if (body == NULL) {
        show_toast(state, error->message);
        g_clear_error(&error);
        return;
    }
    {
        AdwMessageDialog *dialog = ADW_MESSAGE_DIALOG(
            adw_message_dialog_new(GTK_WINDOW(state->window), _("Checksums"), body));
        adw_message_dialog_add_response(dialog, "close", _("Close"));
        gtk_window_present(GTK_WINDOW(dialog));
    }
    g_free(body);
}

static void on_hash_clicked(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    HashTask *data;
    GTask *task;
    (void)button;
    if (state->image_path == NULL)
        return;
    state->hashing = TRUE;
    gtk_widget_set_sensitive(GTK_WIDGET(state->hash_button), FALSE);
    gtk_label_set_text(state->status_label, _("CALCULATING CHECKSUMS"));
    data = g_new0(HashTask, 1);
    data->path = g_strdup(state->image_path);
    task = g_task_new(NULL, NULL, hashes_finished, state);
    g_task_set_task_data(task, data, (GDestroyNotify)hash_task_free);
    g_task_run_in_thread(task, calculate_hashes);
    g_object_unref(task);
}

static char *helper_path(void)
{
    char *executable = g_file_read_link("/proc/self/exe", NULL);
    if (executable != NULL) {
        char *bin = g_path_get_dirname(executable);
        char *prefix = g_path_get_dirname(bin);
        char *candidate = g_build_filename(prefix, "libexec", "tuxflash-helper", NULL);
        g_free(executable);
        g_free(bin);
        g_free(prefix);
        return candidate;
    }
    return g_strdup(LUFUS_HELPER_PATH);
}

static void writer_line_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AppState *state = user_data;
    GError *error = NULL;
    gsize length = 0;
    char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source), result, &length, &error);
    if (line == NULL) {
        if (error != NULL) {
            append_log(state, error->message);
            state->writer_read_failed = TRUE;
            g_clear_error(&error);
        }
        state->writer_eof = TRUE;
        finish_writer_if_ready(state);
        return;
    }
    if (g_str_has_prefix(line, "STAGE ") ||
        g_str_has_prefix(line, "STAGE_PERCENT ") ||
        g_str_has_prefix(line, "STAGE_TEXT ")) {
        gboolean stage_percent = g_str_has_prefix(line, "STAGE_PERCENT ");
        gboolean stage_text = g_str_has_prefix(line, "STAGE_TEXT ");
        const char *values = line + (stage_percent ? 14 : stage_text ? 11 : 6);
        unsigned int start = 0;
        unsigned int end = 0;
        int message_offset = 0;
        if (sscanf(values, "%u %u %n", &start, &end, &message_offset) == 2 &&
            start <= end && end <= 1000) {
            state->progress_stage_start = start / 1000.0;
            state->progress_stage_end = end / 1000.0;
            state->progress_stage_last_relative = 0.0;
            state->progress_stage_percent = stage_percent;
            state->progress_stage_text = stage_text;
            g_free(state->progress_stage_message);
            state->progress_stage_message = g_strdup(_(values + message_offset));
            set_progress_fraction(state, 0.0);
            if (values[message_offset] != '\0') {
                gtk_label_set_text(state->status_label, _(values + message_offset));
                append_log(state, values + message_offset);
            }
        }
    } else if (g_str_has_prefix(line, "PROGRESS_PERCENT ")) {
        unsigned int thousandths = 0;
        if (sscanf(line + 17, "%u", &thousandths) == 1 && thousandths <= 1000)
            set_progress_fraction(state, thousandths / 1000.0);
    } else if (g_str_has_prefix(line, "PROGRESS ") || g_str_has_prefix(line, "VERIFY_PROGRESS ")) {
        unsigned long long written = 0;
        unsigned long long total = 0;
        const char *values = g_str_has_prefix(line, "PROGRESS ") ? line + 9 : line + 16;
        if (sscanf(values, "%llu %llu", &written, &total) == 2) {
            if (total > 0) {
                double fraction = MIN(1.0, (double)written / (double)total);
                set_progress_fraction(state, fraction);
            } else {
                start_progress_pulse(state);
            }
        }
    } else if (g_str_has_prefix(line, "LOG ")) {
        append_log(state, line + 4);
    } else if (g_str_has_prefix(line, "STARTED ")) {
        gtk_label_set_text(state->status_label, _(line + 8));
        append_log(state, line + 8);
    } else if (g_str_has_prefix(line, "VERIFYING ")) {
        gtk_label_set_text(state->status_label, _(line + 10));
        append_log(state, line + 10);
    } else if (g_str_has_prefix(line, "COMPLETE ")) {
        state->write_completed = TRUE;
    } else if (g_str_has_prefix(line, "ERROR ")) {
        stop_progress_pulse(state);
        append_log(state, line + 6);
        gtk_label_set_text(state->status_label,
                           strstr(line + 6, "cancel") != NULL ? _("CANCELLED") : _("ERROR"));
        show_toast(state, line + 6);
    } else {
        append_log(state, line);
    }
    g_free(line);
    read_writer_line(state);
}

static void read_writer_line(AppState *state)
{
    if (state->writer_output != NULL)
        g_data_input_stream_read_line_async(state->writer_output, G_PRIORITY_DEFAULT, NULL,
                                            writer_line_finished, state);
}

static void finish_writer_if_ready(AppState *state)
{
    if (!state->writer || !state->writer_exited || !state->writer_eof) return;
    stop_progress_pulse(state);
    if (state->writer_succeeded && state->write_completed && !state->writer_read_failed) {
        gtk_progress_bar_set_fraction(state->progress, 1.0);
        gtk_progress_bar_set_text(state->progress, _("READY"));
        gtk_label_set_text(state->status_label, _("READY"));
        show_toast(state, _("Operation completed successfully"));
    } else if (g_strcmp0(gtk_label_get_text(state->status_label), _("CANCELLED")) != 0) {
        gtk_label_set_text(state->status_label, _("ERROR"));
        show_toast(state, _("Write failed or authorization was cancelled"));
    }
    g_clear_object(&state->writer_output);
    g_clear_object(&state->writer);
    gtk_button_set_label(state->start_button, _("START"));
    gtk_widget_remove_css_class(GTK_WIDGET(state->start_button), "destructive-action");
    gtk_widget_add_css_class(GTK_WIDGET(state->start_button), "suggested-action");
    refresh_devices(state, FALSE);
    update_start_sensitivity(state);
}

static void writer_wait_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AppState *state = user_data;
    GError *error = NULL;
    gboolean waited = g_subprocess_wait_finish(G_SUBPROCESS(source), result, &error);
    state->writer_succeeded = waited && g_subprocess_get_successful(G_SUBPROCESS(source));
    state->writer_exited = TRUE;
    if (error) { append_log(state, error->message); g_clear_error(&error); }
    finish_writer_if_ready(state);
}

static void pending_write_free(PendingWrite *pending)
{
    g_free(pending->path);
    g_free(pending->display_name);
    g_free(pending->major_minor);
    g_free(pending->output_path);
    lufus_windows_options_clear(&pending->windows);
    g_free(pending);
}

static void begin_write(PendingWrite *pending)
{
    AppState *state = pending->state;
    GError *error = NULL;
    char *helper = helper_path();
    LufusJob *job;
    if (pending->type == LUFUS_JOB_WINDOWS_INSTALLER)
        job = lufus_job_new_windows_installer(state->image_path, pending->path,
                                              pending->uefi_ntfs,
                                              pending->windows_target,
                                              pending->runtime_validation,
                                              &pending->windows, &error);
    else if (pending->type == LUFUS_JOB_WINDOWS_TO_GO)
        job = lufus_job_new_windows_to_go(state->image_path, pending->path,
                                          pending->windows_index,
                                          &pending->windows, &error);
    else if (pending->type == LUFUS_JOB_LINUX_PERSISTENT)
        job = lufus_job_new_linux_persistent(state->image_path, pending->path,
                                              pending->persistence_size,
                                              pending->runtime_validation, &error);
    else if (pending->type == LUFUS_JOB_CAPTURE)
        job = lufus_job_new_capture(pending->path, pending->output_path,
                                    pending->capture_format, &error);
    else
        job = lufus_job_new_raw(state->image_path, pending->path,
                                pending->runtime_validation, &error);
    if (job != NULL && (g_strcmp0(job->major_minor, pending->major_minor) != 0 ||
        job->device_size != pending->device_size ||
        (pending->disk_sequence && job->disk_sequence != pending->disk_sequence))) {
        lufus_job_free(job);
        job = NULL;
        g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("The selected device changed after confirmation"));
    }
    if (job != NULL && pending->test_passes > 0 &&
        !lufus_job_set_pretest(job, pending->test_passes, &error)) {
        lufus_job_free(job);
        job = NULL;
    }
    if (job == NULL) {
        show_toast(state, error->message);
        g_clear_error(&error);
        g_free(helper);
        pending_write_free(pending);
        return;
    }
    state->write_completed = FALSE;
    state->writer_exited = FALSE;
    state->writer_succeeded = FALSE;
    state->writer_eof = FALSE;
    state->writer_read_failed = FALSE;
    state->writer = lufus_job_spawn(job, helper, geteuid() != 0, &error);
    lufus_job_free(job);
    if (state->writer == NULL) {
        show_toast(state, error->message);
        g_clear_error(&error);
        g_free(helper);
        pending_write_free(pending);
        return;
    }
    state->writer_output = g_data_input_stream_new(g_subprocess_get_stdout_pipe(state->writer));
    gtk_button_set_label(state->start_button, _("CANCEL"));
    gtk_widget_remove_css_class(GTK_WIDGET(state->start_button), "suggested-action");
    gtk_widget_add_css_class(GTK_WIDGET(state->start_button), "destructive-action");
    gtk_label_set_text(state->status_label, _("REQUESTING AUTHORIZATION"));
    reset_progress_range(state);
    gtk_progress_bar_set_fraction(state->progress, 0.0);
    gtk_progress_bar_set_text(state->progress, _("Authorizing…"));
    gtk_progress_bar_set_show_text(state->progress, TRUE);
    start_progress_pulse(state);
    read_writer_line(state);
    g_subprocess_wait_async(state->writer, NULL, writer_wait_finished, state);
    g_free(helper);
    pending_write_free(pending);
}

static void write_confirmation_response(AdwMessageDialog *dialog, const char *response, gpointer user_data)
{
    PendingWrite *pending = user_data;
    (void)dialog;
    if (strcmp(response, "write") == 0)
        begin_write(pending);
    else
        pending_write_free(pending);
}

static void update_silent_confirmation(GtkCheckButton *button, gpointer user_data)
{
    AdwMessageDialog *dialog = ADW_MESSAGE_DIALOG(user_data);
    GtkCheckButton *first = g_object_get_data(G_OBJECT(dialog), "silent-confirm-first");
    GtkCheckButton *second = g_object_get_data(G_OBJECT(dialog), "silent-confirm-second");
    GtkCheckButton *third = g_object_get_data(G_OBJECT(dialog), "silent-confirm-third");
    gboolean accepted;
    (void)button;

    accepted = gtk_check_button_get_active(first) &&
               gtk_check_button_get_active(second) &&
               gtk_check_button_get_active(third);
    adw_message_dialog_set_response_enabled(dialog, "write", accepted);
}

static void capture_dialog_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AppState *state = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);
    LufusDevice *device;
    PendingWrite *pending;
    char *path;
    char *heading;
    char *body;
    AdwMessageDialog *dialog;
    if (file == NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
            !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
            show_toast(state, error->message);
        g_clear_error(&error);
        return;
    }
    path = g_file_get_path(file);
    g_object_unref(file);
    device = selected_device(state);
    if (path == NULL || device == NULL || g_file_test(path, G_FILE_TEST_EXISTS)) {
        show_toast(state, _("Choose a new local output file and a source device"));
        g_free(path);
        return;
    }
    pending = g_new0(PendingWrite, 1);
    pending->state = state;
    pending->type = LUFUS_JOB_CAPTURE;
    pending->path = g_strdup(device->path);
    pending->major_minor = g_strdup(device->major_minor);
    pending->device_size = device->size;
    pending->disk_sequence = device->disk_sequence;
    pending->display_name = lufus_device_display_name(device);
    pending->output_path = path;
    pending->capture_format = g_str_has_suffix(path, ".vhdx") ? LUFUS_CAPTURE_VHDX :
                              g_str_has_suffix(path, ".vhd") ? LUFUS_CAPTURE_VHD :
                                                               LUFUS_CAPTURE_RAW;
    heading = g_strdup_printf(_("Save %s as an image?"), device->path);
    body = g_strdup_printf(_("TuxFlash will unmount %s for a consistent capture.\n\nOutput: %s"),
                           pending->display_name, path);
    dialog = ADW_MESSAGE_DIALOG(adw_message_dialog_new(GTK_WINDOW(state->window), heading, body));
    adw_message_dialog_add_responses(dialog, "cancel", _("Cancel"), "write", _("SAVE"), NULL);
    adw_message_dialog_set_default_response(dialog, "cancel");
    adw_message_dialog_set_close_response(dialog, "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(write_confirmation_response), pending);
    gtk_window_present(GTK_WINDOW(dialog));
    g_free(body);
    g_free(heading);
}

static void on_save_device(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    (void)button;
    gtk_file_dialog_set_title(dialog, _("Save device image (raw, .vhd, or .vhdx)"));
    gtk_file_dialog_set_initial_name(dialog, "tuxflash-device.img");
    gtk_file_dialog_save(dialog, GTK_WINDOW(state->window), NULL,
                         capture_dialog_finished, state);
    g_object_unref(dialog);
}

static void on_start_clicked(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    LufusDevice *device;
    PendingWrite *pending;
    char *image_size;
    char *body;
    char *heading;
    char *image_name;
    AdwMessageDialog *dialog;
    (void)button;
    if (state->writer != NULL) {
        gtk_label_set_text(state->status_label, _("CANCELLING"));
        g_subprocess_send_signal(state->writer, SIGTERM);
        return;
    }
    device = selected_device(state);
    if (state->image_path == NULL || device == NULL)
        return;
    if (device->read_only) {
        show_toast(state, _("The selected device is read-only"));
        return;
    }
    pending = g_new0(PendingWrite, 1);
    pending->state = state;
    pending->path = g_strdup(device->path);
    pending->major_minor = g_strdup(device->major_minor);
    pending->device_size = device->size;
    pending->disk_sequence = device->disk_sequence;
    pending->display_name = lufus_device_display_name(device);
    if (gtk_check_button_get_active(state->bad_blocks))
        pending->test_passes = gtk_drop_down_get_selected(state->test_passes) + 1;
    switch (selected_image_option(state)) {
    case IMAGE_OPTION_WINDOWS_UEFI_FAT32:
        pending->type = LUFUS_JOB_WINDOWS_INSTALLER;
        pending->windows_target = LUFUS_WINDOWS_TARGET_UEFI;
        break;
    case IMAGE_OPTION_WINDOWS_UEFI_NTFS:
        pending->type = LUFUS_JOB_WINDOWS_INSTALLER;
        pending->uefi_ntfs = TRUE;
        pending->windows_target = LUFUS_WINDOWS_TARGET_UEFI;
        break;
    case IMAGE_OPTION_WINDOWS_BIOS_NTFS:
        pending->type = LUFUS_JOB_WINDOWS_INSTALLER;
        pending->windows_target = LUFUS_WINDOWS_TARGET_BIOS;
        break;
    case IMAGE_OPTION_WINDOWS_BIOS_UEFI_NTFS:
        pending->type = LUFUS_JOB_WINDOWS_INSTALLER;
        pending->uefi_ntfs = TRUE;
        pending->windows_target = LUFUS_WINDOWS_TARGET_BIOS_UEFI;
        break;
    case IMAGE_OPTION_WINDOWS_TO_GO:
        pending->type = LUFUS_JOB_WINDOWS_TO_GO;
        pending->windows_target = LUFUS_WINDOWS_TARGET_UEFI;
        break;
    case IMAGE_OPTION_LINUX_PERSISTENT:
        pending->type = LUFUS_JOB_LINUX_PERSISTENT;
        pending->persistence_size =
            (guint64)gtk_spin_button_get_value_as_int(state->persistence_size) *
            1024U * 1024U;
        break;
    case IMAGE_OPTION_RAW:
    default:
        pending->type = LUFUS_JOB_RAW_WRITE;
        break;
    }
    pending->runtime_validation =
        gtk_check_button_get_active(state->runtime_validation) &&
        (pending->type == LUFUS_JOB_WINDOWS_INSTALLER ||
         pending->type == LUFUS_JOB_LINUX_PERSISTENT ||
         pending->type == LUFUS_JOB_RAW_WRITE);
    if (pending->type == LUFUS_JOB_WINDOWS_INSTALLER ||
        pending->type == LUFUS_JOB_WINDOWS_TO_GO) {
        pending->windows.flags =
            (gtk_check_button_get_active(state->bypass_requirements) ?
                LUFUS_WINDOWS_BYPASS_REQUIREMENTS : 0) |
            (gtk_check_button_get_active(state->no_online_account) ?
                LUFUS_WINDOWS_NO_ONLINE_ACCOUNT : 0) |
            (gtk_check_button_get_active(state->no_data_collection) ?
                LUFUS_WINDOWS_NO_DATA_COLLECTION : 0) |
            (gtk_check_button_get_active(state->create_local_account) ?
                LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT : 0) |
            (gtk_check_button_get_active(state->disable_bitlocker) ?
                LUFUS_WINDOWS_DISABLE_BITLOCKER : 0) |
            (gtk_check_button_get_active(state->offline_internal_drives) ?
                LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES : 0) |
            (gtk_check_button_get_active(state->silent_install) ?
                LUFUS_WINDOWS_SILENT_INSTALL : 0);
        {
            static const char *architectures[] = {"amd64", "x86", "arm64", "arm"};
            guint architecture = gtk_drop_down_get_selected(state->windows_architecture);
            pending->windows.architecture = g_strdup(
                architecture < G_N_ELEMENTS(architectures) ? architectures[architecture] : "amd64");
        }
        pending->windows_index = (guint)gtk_spin_button_get_value_as_int(state->windows_index);
        pending->windows.image_index =
            (guint)gtk_spin_button_get_value_as_int(state->silent_install_index);
        if (gtk_check_button_get_active(state->create_local_account))
            pending->windows.local_account = g_strdup(
                gtk_editable_get_text(GTK_EDITABLE(state->local_account)));
    }
    image_size = lufus_format_size(state->image_size);
    image_name = g_path_get_basename(state->image_path);
    if ((pending->windows.flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
        heading = g_strdup(_("Create media that silently erases a PC disk?"));
        body = g_strdup_printf(
            _("ALL DATA on the USB %s will be destroyed now.\n\n"
            "When this media is booted, Windows Setup will UNCONDITIONALLY ERASE "
            "the first detected disk (Disk 0) and install image index %u without prompting.\n\n"
            "Image: %s\nSize: %s"),
            pending->display_name, pending->windows.image_index, image_name, image_size);
    } else {
        heading = g_strdup_printf(_("Erase %s?"), device->path);
        body = g_strdup_printf(_("ALL DATA on %s will be destroyed.\n\nImage: %s\nSize: %s"),
                               pending->display_name, image_name, image_size);
    }
    dialog = ADW_MESSAGE_DIALOG(adw_message_dialog_new(GTK_WINDOW(state->window), heading, body));
    adw_message_dialog_add_responses(
        dialog, "cancel", _("Cancel"), "write",
        (pending->windows.flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0 ?
            _("CREATE DANGEROUS MEDIA") : _("WRITE"), NULL);
    adw_message_dialog_set_default_response(dialog, "cancel");
    adw_message_dialog_set_close_response(dialog, "cancel");
    adw_message_dialog_set_response_appearance(dialog, "write", ADW_RESPONSE_DESTRUCTIVE);
    if ((pending->windows.flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
        GtkWidget *acknowledgements = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkCheckButton *first = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
            _("I WILL disconnect every disk except the Windows installation target.")));
        GtkCheckButton *second = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
            _("I WILL NOT leave this media connected to a PC I do not plan to erase.")));
        GtkCheckButton *third = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
            _("I ACCEPT responsibility for any irreversible data loss.")));
        gtk_box_append(GTK_BOX(acknowledgements), GTK_WIDGET(first));
        gtk_box_append(GTK_BOX(acknowledgements), GTK_WIDGET(second));
        gtk_box_append(GTK_BOX(acknowledgements), GTK_WIDGET(third));
        adw_message_dialog_set_extra_child(dialog, acknowledgements);
        g_object_set_data(G_OBJECT(dialog), "silent-confirm-first", first);
        g_object_set_data(G_OBJECT(dialog), "silent-confirm-second", second);
        g_object_set_data(G_OBJECT(dialog), "silent-confirm-third", third);
        g_signal_connect(first, "toggled", G_CALLBACK(update_silent_confirmation), dialog);
        g_signal_connect(second, "toggled", G_CALLBACK(update_silent_confirmation), dialog);
        g_signal_connect(third, "toggled", G_CALLBACK(update_silent_confirmation), dialog);
        adw_message_dialog_set_response_enabled(dialog, "write", FALSE);
    }
    g_signal_connect(dialog, "response", G_CALLBACK(write_confirmation_response), pending);
    gtk_window_present(GTK_WINDOW(dialog));
    g_free(body);
    g_free(heading);
    g_free(image_name);
    g_free(image_size);
}

static void on_show_log(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    GtkWidget *window = adw_window_new();
    AdwToolbarView *toolbar = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
    GtkWidget *close = gtk_button_new_with_label(_("Close"));
    GtkWidget *scroller = gtk_scrolled_window_new();
    GtkWidget *view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    (void)button;
    gtk_window_set_title(GTK_WINDOW(window), _("TuxFlash Log"));
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 420);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_buffer_set_text(buffer, state->log->len == 0 ? _("No log entries yet.") : state->log->str, -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), view);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), window);
    adw_header_bar_pack_end(header, close);
    adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(header));
    adw_toolbar_view_set_content(toolbar, scroller);
    adw_window_set_content(ADW_WINDOW(window), GTK_WIDGET(toolbar));
    gtk_window_present(GTK_WINDOW(window));
}

static void on_credit_link_clicked(GtkButton *button, gpointer user_data)
{
    const char *uri = user_data;
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(button));

    if (GTK_IS_WINDOW(root)) {
        GtkUriLauncher *launcher = gtk_uri_launcher_new(uri);
        gtk_uri_launcher_launch(launcher, GTK_WINDOW(root), NULL, NULL, NULL);
        g_object_unref(launcher);
    }
}

static AdwActionRow *find_credit_row(GtkWidget *widget, const char *title)
{
    GtkWidget *child;

    if (ADW_IS_ACTION_ROW(widget) &&
        g_strcmp0(adw_preferences_row_get_title(ADW_PREFERENCES_ROW(widget)),
                  title) == 0)
        return ADW_ACTION_ROW(widget);

    for (child = gtk_widget_get_first_child(widget); child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        AdwActionRow *row = find_credit_row(child, title);
        if (row != NULL)
            return row;
    }
    return NULL;
}

static void make_credit_clickable(AdwAboutWindow *about, const char *title,
                                  const char *uri, const char *accessible_label)
{
    AdwActionRow *row = find_credit_row(GTK_WIDGET(about), title);
    GtkWidget *link;

    if (row == NULL) {
        g_warning("Could not find About credit row: %s", title);
        return;
    }

    link = gtk_button_new_from_icon_name("adw-external-link-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(link), FALSE);
    gtk_widget_set_valign(link, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(link, accessible_label);
    gtk_accessible_update_property(GTK_ACCESSIBLE(link),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   accessible_label, -1);
    g_signal_connect(link, "clicked", G_CALLBACK(on_credit_link_clicked),
                     (gpointer)uri);
    adw_action_row_add_suffix(row, link);
    adw_action_row_set_activatable_widget(row, link);
    gtk_widget_set_cursor_from_name(GTK_WIDGET(row), "pointer");
}

static void on_show_about(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    AdwAboutWindow *about = ADW_ABOUT_WINDOW(adw_about_window_new());
    const char *developers[] = {
        _("Ricardo Barreto — TuxFlash development"),
        _("Pete Batard and Rufus contributors — upstream"),
        _("Open Research Tools contributors — Linux implementation"),
        _("Lufus — base project for the Linux version"),
        NULL,
    };
    const LufusLegalComponent *components;
    size_t component_count = 0;
    size_t index;
    (void)button;
    gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(state->window));
    adw_about_window_set_application_name(about, "TuxFlash");
    adw_about_window_set_application_icon(about, APP_ID);
    adw_about_window_set_version(about, LUFUS_VERSION);
    adw_about_window_set_website(about, "https://github.com/ricardogomesbarreto");
    adw_about_window_set_developer_name(about,
                                        _("Native bootable USB creator for Linux"));
    adw_about_window_set_developers(about, developers);
    make_credit_clickable(
        about, developers[0],
        "https://github.com/ricardogomesbarreto",
        _("Open Ricardo Barreto’s GitHub profile"));
    make_credit_clickable(
        about, developers[1],
        "https://github.com/pbatard/rufus/graphs/contributors",
        _("Open Rufus contributors page"));
    make_credit_clickable(
        about, developers[2],
        "https://github.com/openresearchtools/lufus/graphs/contributors",
        _("Open Lufus contributors page"));
    make_credit_clickable(
        about, developers[3],
        "https://github.com/openresearchtools/lufus",
        _("Open the Lufus repository"));
    adw_about_window_set_comments(about,
        _("Turn ISO images into bootable USB drives simply and safely."));
    adw_about_window_set_copyright(about,
        _("Copyright © 2026 Ricardo Barreto\n"
        "Copyright © Pete Batard and Rufus contributors\n"
        "Copyright © 2026 Open Research Tools contributors"));
    adw_about_window_set_license_type(about, GTK_LICENSE_CUSTOM);
    adw_about_window_set_license(about, _(lufus_project_license_notice()));
    adw_about_window_add_link(about, _("Read the GNU GPL v3 license"),
                              "https://www.gnu.org/licenses/gpl-3.0.html");
    g_autofree char *notes = tuxflash_release_notes();
    adw_about_window_set_release_notes_version(about, LUFUS_VERSION);
    adw_about_window_set_release_notes(about, notes);
    components = lufus_legal_components(&component_count);
    for (index = 0; index < component_count; index++) {
        g_autofree char *copyright_markup = g_markup_escape_text(_(components[index].copyright), -1);
        g_autofree char *license_markup = g_markup_escape_text(_(components[index].license), -1);
        adw_about_window_add_legal_section(about,
                                           _(components[index].title),
                                           copyright_markup,
                                           GTK_LICENSE_CUSTOM,
                                           license_markup);
    }
    gtk_window_present(GTK_WINDOW(about));
}

static gboolean on_close_request(GtkWindow *window, gpointer user_data)
{
    AppState *state = user_data;
    (void)window;
    if (state->hashing) {
        show_toast(state, _("Wait for checksums to finish before closing TuxFlash"));
        return TRUE;
    }
    if (state->writer != NULL) {
        show_toast(state, _("Cancel the active write before closing TuxFlash"));
        return TRUE;
    }
    if (state->download_cancellable != NULL) {
        show_toast(state, _("Cancel the active download before closing TuxFlash"));
        return TRUE;
    }
    return FALSE;
}

static void state_free(AppState *state)
{
    if (state->refresh_source != 0)
        g_source_remove(state->refresh_source);
    stop_progress_pulse(state);
    g_clear_object(&state->writer_output);
    g_clear_object(&state->writer);
    g_clear_object(&state->download_cancellable);
    if (state->devices != NULL)
        g_ptr_array_unref(state->devices);
    g_free(state->image_path);
    g_free(state->progress_stage_message);
    g_string_free(state->log, TRUE);
    g_free(state);
}

static GtkWidget *build_drive_properties(AppState *state)
{
    const char *boot_values[] = {_("Disk or ISO image (Please select)"), NULL};
    const char *image_values[] = {_("Write in DD Image mode"), NULL};
    const char *partition_values[] = {"MBR", "GPT", _("From image"), NULL};
    const char *target_values[] = {_("BIOS (or UEFI-CSM)"), _("UEFI (non CSM)"),
                                    _("BIOS or UEFI"), _("From image"), NULL};
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *device_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *boot_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *scheme_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    GtkWidget *refresh = gtk_button_new_from_icon_name("view-refresh-symbolic");
    state->capture_button = GTK_BUTTON(gtk_button_new_with_label(_("SAVE")));
    GtkWidget *select = gtk_button_new_with_label(_("Select"));
    GtkWidget *partition = new_dropdown(partition_values);
    state->partition_scheme = GTK_DROP_DOWN(partition);
    GtkWidget *advanced_toggle = gtk_check_button_new_with_label(_("Show advanced drive properties"));
    GtkWidget *revealer = gtk_revealer_new();
    GtkWidget *advanced = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *account_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *windows_details = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *silent_index_field;
    const char *no_devices[] = {_("No compatible devices found"), NULL};

    append_heading(GTK_BOX(box), _("Drive Properties"));
    state->device_dropdown = GTK_DROP_DOWN(new_dropdown(no_devices));
    gtk_box_append(GTK_BOX(device_row), GTK_WIDGET(state->device_dropdown));
    gtk_widget_set_tooltip_text(refresh, _("Refresh devices"));
    g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh_clicked), state);
    gtk_box_append(GTK_BOX(device_row), refresh);
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->capture_button),
                                _("Save this device as raw, VHD, or VHDX"));
    gtk_widget_set_sensitive(GTK_WIDGET(state->capture_button), FALSE);
    g_signal_connect(state->capture_button, "clicked", G_CALLBACK(on_save_device), state);
    gtk_box_append(GTK_BOX(device_row), GTK_WIDGET(state->capture_button));
    gtk_box_append(GTK_BOX(box), new_field(_("Device"), device_row));

    state->boot_dropdown = GTK_DROP_DOWN(new_dropdown(boot_values));
    gtk_box_append(GTK_BOX(boot_row), GTK_WIDGET(state->boot_dropdown));
    state->hash_button = GTK_BUTTON(gtk_button_new_from_icon_name("document-properties-symbolic"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->hash_button), _("Calculate image checksums"));
    gtk_widget_set_sensitive(GTK_WIDGET(state->hash_button), FALSE);
    g_signal_connect(state->hash_button, "clicked", G_CALLBACK(on_hash_clicked), state);
    gtk_box_append(GTK_BOX(boot_row), GTK_WIDGET(state->hash_button));
    gtk_widget_add_css_class(select, "suggested-action");
    g_signal_connect(select, "clicked", G_CALLBACK(on_select_image), state);
    state->download_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("folder-download-symbolic"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->download_button),
                                _("Download an official Windows ISO from Microsoft"));
    g_signal_connect(state->download_button, "clicked",
                     G_CALLBACK(on_download_clicked), state);
    gtk_box_append(GTK_BOX(boot_row), GTK_WIDGET(state->download_button));
    gtk_box_append(GTK_BOX(boot_row), select);
    gtk_box_append(GTK_BOX(box), new_field(_("Boot selection"), boot_row));

    state->image_option = GTK_DROP_DOWN(new_dropdown(image_values));
    gtk_box_append(GTK_BOX(box), new_field(_("Image Option"), GTK_WIDGET(state->image_option)));
    state->windows_options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    state->bypass_requirements = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("Remove requirement for 4GB+ RAM, Secure Boot and TPM 2.0")));
    state->no_online_account = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("Remove requirement for an online Microsoft account")));
    state->no_data_collection = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("Disable data collection (skip privacy questions)")));
    state->create_local_account = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("Create a local account with username")));
    state->local_account = GTK_ENTRY(gtk_entry_new());
    state->disable_bitlocker = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("Disable BitLocker automatic device encryption")));
    state->offline_internal_drives = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("Prevent Windows To Go from accessing internal disks")));
    state->silent_install = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
        _("⚠SILENTLY⚠ erase disk and install:")));
    gtk_widget_set_tooltip_text(
        GTK_WIDGET(state->silent_install),
        _("Creates media that does not prompt during Windows installation and "
          "unconditionally erases the first detected disk on the target PC. "
          "Disconnect every other disk before booting it."));
    gtk_widget_add_css_class(GTK_WIDGET(state->silent_install), "error");
    {
        static const char *architectures[] = {"x64 (amd64)", "x86", "ARM64", "ARM", NULL};
        state->windows_architecture = GTK_DROP_DOWN(new_dropdown(architectures));
    }
    state->windows_index = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 99, 1));
    gtk_spin_button_set_value(state->windows_index, 1);
    state->silent_install_index = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 99, 1));
    gtk_spin_button_set_value(state->silent_install_index, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(state->silent_install_index), FALSE);
    gtk_check_button_set_active(state->bypass_requirements, TRUE);
    gtk_check_button_set_active(state->no_online_account, TRUE);
    gtk_editable_set_text(GTK_EDITABLE(state->local_account), "User");
    gtk_widget_set_sensitive(GTK_WIDGET(state->local_account), FALSE);
    g_object_bind_property(state->create_local_account, "active", state->local_account,
                           "sensitive", G_BINDING_SYNC_CREATE);
    gtk_box_append(GTK_BOX(state->windows_options_box),
                   GTK_WIDGET(state->bypass_requirements));
    gtk_box_append(GTK_BOX(state->windows_options_box),
                   GTK_WIDGET(state->no_online_account));
    gtk_box_append(GTK_BOX(state->windows_options_box),
                   GTK_WIDGET(state->no_data_collection));
    state->silent_install_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(state->silent_install_row),
                   GTK_WIDGET(state->silent_install));
    silent_index_field = new_field(_("Windows image index"),
                                   GTK_WIDGET(state->silent_install_index));
    gtk_box_append(GTK_BOX(state->silent_install_row), silent_index_field);
    gtk_widget_set_visible(state->silent_install_row, FALSE);
    gtk_box_append(GTK_BOX(state->windows_options_box), state->silent_install_row);
    gtk_box_append(GTK_BOX(account_row), GTK_WIDGET(state->create_local_account));
    gtk_box_append(GTK_BOX(account_row), GTK_WIDGET(state->local_account));
    gtk_box_append(GTK_BOX(state->windows_options_box), account_row);
    gtk_box_append(GTK_BOX(state->windows_options_box),
                   GTK_WIDGET(state->disable_bitlocker));
    gtk_box_append(GTK_BOX(state->windows_options_box),
                   GTK_WIDGET(state->offline_internal_drives));
    gtk_box_append(GTK_BOX(windows_details),
                   new_field(_("Windows architecture"), GTK_WIDGET(state->windows_architecture)));
    state->windows_to_go_index_row =
        new_field(_("Windows To Go image index"), GTK_WIDGET(state->windows_index));
    gtk_widget_set_visible(state->windows_to_go_index_row, FALSE);
    gtk_box_append(GTK_BOX(windows_details), state->windows_to_go_index_row);
    gtk_box_append(GTK_BOX(state->windows_options_box), windows_details);
    gtk_widget_set_visible(state->windows_options_box, FALSE);
    gtk_box_append(GTK_BOX(box), state->windows_options_box);
    g_signal_connect(state->silent_install, "toggled",
                     G_CALLBACK(on_silent_install_toggled), state);
    state->persistence_options_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->persistence_size = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(256, 1048576, 256));
    gtk_spin_button_set_value(state->persistence_size, 4096);
    gtk_box_append(GTK_BOX(state->persistence_options_box),
                   new_field(_("Persistent partition size (MiB)"),
                             GTK_WIDGET(state->persistence_size)));
    gtk_widget_set_visible(state->persistence_options_box, FALSE);
    gtk_box_append(GTK_BOX(box), state->persistence_options_box);
    g_signal_connect(state->image_option, "notify::selected",
                     G_CALLBACK(on_image_option_selected), state);
    gtk_widget_set_sensitive(partition, FALSE);
    state->target_system = GTK_DROP_DOWN(new_dropdown(target_values));
    gtk_widget_set_sensitive(GTK_WIDGET(state->target_system), FALSE);
    gtk_widget_set_tooltip_text(partition, _("Determined by the selected image option"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->target_system),
                                _("Selected automatically from the image option"));
    gtk_box_append(GTK_BOX(scheme_row), new_field(_("Partition scheme"), partition));
    gtk_box_append(GTK_BOX(scheme_row), new_field(_("Target system"),
                                                  GTK_WIDGET(state->target_system)));
    gtk_box_append(GTK_BOX(box), scheme_row);

    gtk_box_append(GTK_BOX(box), advanced_toggle);
    state->list_fixed = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(_("List USB Hard Drives")));
    g_signal_connect(state->list_fixed, "toggled", G_CALLBACK(on_list_fixed_toggled), state);
    gtk_box_append(GTK_BOX(advanced), GTK_WIDGET(state->list_fixed));
    GtkWidget *bios_fixes = gtk_check_button_new_with_label(_("Add fixes for old BIOSes (extra partition, align, etc.)"));
    gtk_widget_set_sensitive(bios_fixes, FALSE);
    gtk_widget_set_tooltip_text(bios_fixes, _("Not available in this Linux version"));
    gtk_box_append(GTK_BOX(advanced), bios_fixes);
    state->runtime_validation = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Verify written media (recommended)")));
    gtk_check_button_set_active(
        state->runtime_validation,
        lufus_job_runtime_validation_default(LUFUS_JOB_WINDOWS_INSTALLER));
    gtk_widget_set_sensitive(GTK_WIDGET(state->runtime_validation), FALSE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->runtime_validation),
        _("Reads the completed filesystem and creates a signed UEFI validation manifest. "
          "Turn this off to skip the verification pass."));
    gtk_box_append(GTK_BOX(advanced), GTK_WIDGET(state->runtime_validation));
    gtk_revealer_set_child(GTK_REVEALER(revealer), advanced);
    g_object_bind_property(advanced_toggle, "active", revealer, "reveal-child", G_BINDING_SYNC_CREATE);
    gtk_box_append(GTK_BOX(box), revealer);
    g_signal_connect(state->device_dropdown, "notify::selected", G_CALLBACK(on_device_selected), state);
    return box;
}

static GtkWidget *build_format_options(AppState *state)
{
    const char *filesystems[] = {"FAT32", "NTFS", "exFAT", "UDF", "ext2", "ext3", _("From image"), NULL};
    const char *clusters[] = {_("Default"), _("4096 bytes"), _("8192 bytes"), _("16 kilobytes"), _("32 kilobytes"), NULL};
    const char *passes[] = {_("1 pass"), _("2 passes"), _("3 passes"), _("4 passes"), NULL};
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *label = gtk_entry_new();
    GtkWidget *file_system = new_dropdown(filesystems);
    state->filesystem = GTK_DROP_DOWN(file_system);
    GtkWidget *cluster = new_dropdown(clusters);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    GtkWidget *advanced_toggle = gtk_check_button_new_with_label(_("Show advanced format options"));
    GtkWidget *revealer = gtk_revealer_new();
    GtkWidget *advanced = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *quick = gtk_check_button_new_with_label(_("Quick format"));
    GtkWidget *extended = gtk_check_button_new_with_label(_("Create extended label and icon files"));
    GtkWidget *bad_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->bad_blocks = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Check device for bad blocks")));
    state->test_passes = GTK_DROP_DOWN(new_dropdown(passes));
    append_heading(GTK_BOX(box), _("Format Options"));
    state->volume_label = GTK_ENTRY(label);
    gtk_widget_set_sensitive(label, FALSE);
    gtk_widget_set_sensitive(file_system, FALSE);
    gtk_widget_set_sensitive(cluster, FALSE);
    gtk_widget_set_tooltip_text(label, _("Raw images preserve their embedded format"));
    gtk_widget_set_tooltip_text(file_system, _("Raw images preserve their embedded format"));
    gtk_widget_set_tooltip_text(cluster, _("Raw images preserve their embedded format"));
    gtk_box_append(GTK_BOX(box), new_field(_("Volume label"), label));
    gtk_box_append(GTK_BOX(row), new_field(_("File system"), file_system));
    gtk_box_append(GTK_BOX(row), new_field(_("Cluster size"), cluster));
    gtk_box_append(GTK_BOX(box), row);
    gtk_box_append(GTK_BOX(box), advanced_toggle);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(quick), TRUE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(extended), TRUE);
    gtk_widget_set_sensitive(quick, FALSE);
    gtk_widget_set_sensitive(extended, FALSE);
    gtk_box_append(GTK_BOX(advanced), quick);
    gtk_box_append(GTK_BOX(advanced), extended);
    gtk_box_append(GTK_BOX(bad_row), GTK_WIDGET(state->bad_blocks));
    gtk_box_append(GTK_BOX(bad_row), GTK_WIDGET(state->test_passes));
    g_object_bind_property(state->bad_blocks, "active", state->test_passes,
                           "sensitive", G_BINDING_SYNC_CREATE);
    gtk_box_append(GTK_BOX(advanced), bad_row);
    gtk_revealer_set_child(GTK_REVEALER(revealer), advanced);
    g_object_bind_property(advanced_toggle, "active", revealer, "reveal-child", G_BINDING_SYNC_CREATE);
    gtk_box_append(GTK_BOX(box), revealer);
    return box;
}

static GtkWidget *build_status(AppState *state)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *close = gtk_button_new_with_label(_("CLOSE"));
    append_heading(GTK_BOX(box), _("Status"));
    state->status_label = GTK_LABEL(gtk_label_new(_("READY")));
    gtk_widget_add_css_class(GTK_WIDGET(state->status_label), "caption");
    gtk_label_set_ellipsize(state->status_label, PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(state->status_label, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->status_label));
    state->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->progress));
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    state->start_button = GTK_BUTTON(gtk_button_new_with_label(_("START")));
    gtk_widget_add_css_class(GTK_WIDGET(state->start_button), "suggested-action");
    gtk_widget_set_sensitive(GTK_WIDGET(state->start_button), FALSE);
    g_signal_connect(state->start_button, "clicked", G_CALLBACK(on_start_clicked), state);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_close), state->window);
    gtk_box_append(GTK_BOX(buttons), GTK_WIDGET(state->start_button));
    gtk_box_append(GTK_BOX(buttons), close);
    gtk_box_append(GTK_BOX(box), buttons);
    return box;
}

static gboolean language_change_busy(AppState *state)
{
    return state->writer != NULL || state->download_cancellable != NULL || state->hashing;
}

static void language_response(AdwMessageDialog *dialog, const char *response, gpointer user_data)
{
    AppState *state = user_data;
    if (g_strcmp0(response, "save") != 0)
        return;
    if (language_change_busy(state)) {
        show_toast(state, _("Finish the active operation before changing the language."));
        return;
    }
    GtkDropDown *choice = g_object_get_data(G_OBJECT(dialog), "tuxflash-language-choice");
    GError *error = NULL;
    if (!tuxflash_language_save(gtk_drop_down_get_selected(choice), &error)) {
        show_toast(state, error->message);
        g_clear_error(&error);
        return;
    }
    show_toast(state, _("Language saved. Close and reopen TuxFlash to apply it."));
}

static void on_language_clicked(GtkButton *button, gpointer user_data)
{
    AppState *state = user_data;
    (void)button;
    if (language_change_busy(state)) {
        show_toast(state, _("Finish the active operation before changing the language."));
        return;
    }
    AdwMessageDialog *dialog = ADW_MESSAGE_DIALOG(adw_message_dialog_new(
        GTK_WINDOW(state->window), _("Application language"),
        _("Choose a language. The change takes effect the next time you open TuxFlash.")));
    GtkWidget *choice = new_dropdown(tuxflash_language_names());
    gtk_drop_down_set_selected(GTK_DROP_DOWN(choice), tuxflash_language_selected());
    adw_message_dialog_set_extra_child(dialog, choice);
    g_object_set_data(G_OBJECT(dialog), "tuxflash-language-choice", choice);
    adw_message_dialog_add_responses(dialog, "cancel", _("Cancel"), "save", _("Save"), NULL);
    adw_message_dialog_set_default_response(dialog, "save");
    adw_message_dialog_set_close_response(dialog, "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(language_response), state);
    gtk_window_present(GTK_WINDOW(dialog));
}

static AppState *create_window(AdwApplication *application)
{
    AppState *state = g_new0(AppState, 1);
    AdwToolbarView *toolbar = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
    GtkWidget *title = adw_window_title_new("TuxFlash", _("Bootable USB Creator for Linux"));
    GtkWidget *log_button = gtk_button_new_from_icon_name("utilities-terminal-symbolic");
    GtkWidget *about_button = gtk_button_new_from_icon_name("help-about-symbolic");
    GtkWidget *language_button = gtk_button_new_from_icon_name("preferences-desktop-locale-symbolic");
    GtkWidget *scroller = gtk_scrolled_window_new();
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    state->log = g_string_new(NULL);
    state->window = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(application)));
    gtk_icon_theme_add_resource_path(gtk_icon_theme_get_for_display(gdk_display_get_default()),
                                     "/org/tuxflash/TuxFlash/icons");
    gtk_window_set_default_icon_name(APP_ID);
    gtk_window_set_icon_name(GTK_WINDOW(state->window), APP_ID);
    gtk_window_set_title(GTK_WINDOW(state->window), "TuxFlash " LUFUS_VERSION);
    /* Use the available horizontal space to keep the collapsed form compact.
     * Smaller desktops can still scroll the form; destructive actions do not. */
    gtk_window_set_default_size(GTK_WINDOW(state->window), 920, 690);
    gtk_widget_set_size_request(GTK_WIDGET(state->window), 600, 520);
    state->toast_overlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
    adw_application_window_set_content(state->window, GTK_WIDGET(state->toast_overlay));
    adw_toast_overlay_set_child(state->toast_overlay, GTK_WIDGET(toolbar));
    adw_header_bar_set_title_widget(header, title);
    gtk_widget_set_tooltip_text(log_button, _("View log"));
    gtk_widget_set_tooltip_text(about_button, _("About TuxFlash"));
    g_signal_connect(log_button, "clicked", G_CALLBACK(on_show_log), state);
    g_signal_connect(about_button, "clicked", G_CALLBACK(on_show_about), state);
    GtkWidget *brand_icon = gtk_image_new_from_icon_name(APP_ID);
    gtk_image_set_pixel_size(GTK_IMAGE(brand_icon), 34);
    adw_header_bar_pack_start(header, brand_icon);
    adw_header_bar_pack_start(header, log_button);
    gtk_widget_set_tooltip_text(language_button, _("Application language"));
    g_signal_connect(language_button, "clicked", G_CALLBACK(on_language_clicked), state);
    adw_header_bar_pack_end(header, about_button);
    adw_header_bar_pack_end(header, language_button);
    adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(header));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), content);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    adw_toolbar_view_set_content(toolbar, scroller);
    gtk_widget_set_margin_top(content, 8);
    gtk_widget_set_margin_bottom(content, 8);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    GtkWidget *intro = gtk_label_new(_("Turn ISO images into bootable USB drives simply and safely."));
    gtk_label_set_wrap(GTK_LABEL(intro), TRUE);
    gtk_label_set_xalign(GTK_LABEL(intro), 0.0);
    gtk_widget_add_css_class(intro, "dim-label");
    gtk_widget_set_margin_bottom(intro, 6);
    gtk_box_append(GTK_BOX(content), intro);
    /* GtkFlowBox wraps the two form groups into one column only when needed.
     * At desktop width it avoids unnecessary vertical scrolling. */
    GtkWidget *groups = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(groups), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(groups), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(groups), 2);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(groups), 16);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(groups), 8);
    gtk_widget_set_hexpand(groups, TRUE);
    gtk_flow_box_insert(GTK_FLOW_BOX(groups), build_drive_properties(state), -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(groups), build_format_options(state), -1);
    gtk_box_append(GTK_BOX(content), groups);
    /* The status, progress and action buttons must remain visible even if an
     * expanded advanced section or an unusually small screen requires scroll. */
    GtkWidget *footer = build_status(state);
    gtk_widget_set_margin_top(footer, 6);
    gtk_widget_set_margin_bottom(footer, 8);
    gtk_widget_set_margin_start(footer, 12);
    gtk_widget_set_margin_end(footer, 12);
    adw_toolbar_view_add_bottom_bar(toolbar, footer);
    on_image_option_selected(G_OBJECT(state->image_option), NULL, state);
    g_signal_connect(state->window, "close-request", G_CALLBACK(on_close_request), state);
    g_object_set_data_full(G_OBJECT(state->window), "lufus-state", state, (GDestroyNotify)state_free);
    refresh_devices(state, FALSE);
    state->refresh_source = g_timeout_add_seconds(3, device_refresh_timer, state);
    return state;
}

static void on_activate(GApplication *application, gpointer user_data)
{
    GtkWindow *active = gtk_application_get_active_window(GTK_APPLICATION(application));
    (void)user_data;
    if (active == NULL)
        active = GTK_WINDOW(create_window(ADW_APPLICATION(application))->window);
    gtk_window_present(active);
}

int main(int argc, char **argv)
{
    char *helper;
    setlocale(LC_ALL, "");
    gtk_disable_setlocale();
    char *executable = g_file_read_link("/proc/self/exe", NULL);
    char *bin = executable != NULL ? g_path_get_dirname(executable) : NULL;
    char *prefix = bin != NULL ? g_path_get_dirname(bin) : NULL;
    g_autofree char *locale_directory = prefix != NULL ?
        g_build_filename(prefix, "share", "locale", NULL) : g_strdup("/usr/share/locale");
    tuxflash_language_init(locale_directory);
    g_free(prefix);
    g_free(bin);
    g_free(executable);

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        g_print("TuxFlash %s\n", LUFUS_VERSION);
        return 0;
    }
    if (argc > 1 &&
        (strcmp(argv[1], "doctor") == 0 || strcmp(argv[1], "list") == 0 || strcmp(argv[1], "checksum") == 0 || strcmp(argv[1], "probe") == 0 ||
         strcmp(argv[1], "write") == 0 || strcmp(argv[1], "test-device") == 0 ||
         strcmp(argv[1], "capture") == 0 || strcmp(argv[1], "download") == 0 ||
         strcmp(argv[1], "help") == 0 ||
         strcmp(argv[1], "--help") == 0)) {
        int cli_status;
        helper = helper_path();
        cli_status = lufus_cli_main(argc, argv, helper);
        g_free(helper);
        return cli_status;
    }
    adw_init();
    tuxflash_language_bind_catalogs(locale_directory);
    AdwApplication *application = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    int status;
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    return status;
}
