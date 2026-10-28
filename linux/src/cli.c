/*
 * Lufus command-line interface
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "cli.h"

#include "checksums.h"
#include "image_probe.h"
#include "devices.h"
#include "downloads.h"
#include "job.h"

#include <gio/gio.h>
#include <gio/gunixoutputstream.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int command_list(int argc, char **argv)
{
    gboolean include_fixed = FALSE;
    GOptionEntry entries[] = {
        {"all", 'a', 0, G_OPTION_ARG_NONE, &include_fixed,
         "Include fixed disks (system disks remain excluded)", NULL},
        {NULL}
    };
    GOptionContext *context = g_option_context_new("list [--all]");
    GError *error = NULL;
    GPtrArray *devices;
    guint index;
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("tuxflash list: %s\n", error->message);
        g_clear_error(&error);
        status = 64;
        goto out;
    }
    devices = lufus_devices_enumerate(include_fixed, &error);
    if (devices == NULL) {
        g_printerr("tuxflash list: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    g_print("DEVICE\tSIZE\tTRANSPORT\tMODEL\tSERIAL\n");
    for (index = 0; index < devices->len; index++) {
        LufusDevice *device = g_ptr_array_index(devices, index);
        char *size = lufus_format_size(device->size);
        g_print("%s\t%s\t%s\t%s %s\t%s\n",
                device->path, size, device->transport,
                device->vendor, device->model, device->serial);
        g_free(size);
    }
    g_ptr_array_unref(devices);

out:
    g_option_context_free(context);
    return status;
}

static int command_probe(int argc, char **argv)
{
    char *image = NULL;
    GError *error = NULL;
    LufusImageReport report;
    GOptionEntry entries[] = {
        {"image", 'i', 0, G_OPTION_ARG_FILENAME, &image, "ISO image", "PATH"},
        {NULL}
    };
    GOptionContext *context = g_option_context_new("probe --image PATH");
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error) || image == NULL) {
        g_printerr("tuxflash probe: %s\n", error != NULL ? error->message : "--image is required");
        status = 64;
    } else if (!lufus_image_probe_iso(image, &report, &error)) {
        g_printerr("tuxflash probe: %s\n", error->message);
        status = 1;
    } else {
        g_print("{\"is_iso\":%s,\"has_efi\":%s,\"has_windows\":%s,"
                "\"supports_windows_installer\":%s,\"supports_windows_to_go\":%s,"
                "\"supports_persistence\":%s}\n",
                report.is_iso ? "true" : "false", report.has_efi ? "true" : "false",
                report.has_windows ? "true" : "false",
                report.supports_windows_installer ? "true" : "false",
                report.supports_windows_to_go ? "true" : "false",
                report.supports_persistence ? "true" : "false");
    }
    g_clear_error(&error);
    g_free(image);
    g_option_context_free(context);
    return status;
}

static int command_checksum(int argc, char **argv)
{
    char *image = NULL;
    GOptionEntry entries[] = {
        {"image", 'i', 0, G_OPTION_ARG_FILENAME, &image, "Image file", "PATH"},
        {NULL}
    };
    GOptionContext *context = g_option_context_new("checksum --image PATH");
    GError *error = NULL;
    char *values;
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error) || image == NULL) {
        g_printerr("tuxflash checksum: %s\n",
                   error != NULL ? error->message : "--image is required");
        g_clear_error(&error);
        status = 64;
        goto out;
    }
    values = lufus_checksums_file(image, NULL, &error);
    if (values == NULL) {
        g_printerr("tuxflash checksum: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    g_print("%s", values);
    g_free(values);

out:
    g_free(image);
    g_option_context_free(context);
    return status;
}

static gboolean sha256_matches(const char *path, const char *expected, GError **error)
{
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    GFile *file = g_file_new_for_path(path);
    GFileInputStream *stream = g_file_read(file, NULL, error);
    unsigned char buffer[1024 * 1024];
    gssize count;
    gboolean matches = FALSE;
    if (stream == NULL)
        goto out;
    while ((count = g_input_stream_read(G_INPUT_STREAM(stream), buffer,
                                        sizeof(buffer), NULL, error)) > 0)
        g_checksum_update(checksum, buffer, (gsize)count);
    if (count >= 0)
        matches = g_ascii_strcasecmp(g_checksum_get_string(checksum), expected) == 0;
out:
    g_clear_object(&stream);
    g_object_unref(file);
    g_checksum_free(checksum);
    return matches;
}

static void cli_download_progress(guint64 completed, guint64 total, gpointer user_data)
{
    int *last_percent = user_data;
    if (total > 0) {
        int percent = (int)(completed * 100U / total);
        if (percent != *last_percent) {
            g_printerr("\rDownloading: %3d%%", percent);
            *last_percent = percent;
        }
    } else if (completed / (16U * 1024U * 1024U) > (guint64)*last_percent) {
        *last_percent = (int)(completed / (16U * 1024U * 1024U));
        g_printerr("\rDownloaded: %" G_GUINT64_FORMAT " MiB", completed / 1024U / 1024U);
    }
}

static int command_download(int argc, char **argv)
{
    static const char *shell_url =
        "https://github.com/pbatard/UEFI-Shell/releases/download/26H1/UEFI-Shell-2.2-26H1-RELEASE.iso";
    static const char *shell_sha256 =
        "2802f4bbd87be88f366d3fe1556a7cfaf41102e3b167224b69a261391ada8ccf";
    char *kind = NULL;
    char *output = NULL;
    char *release = NULL;
    char *language = NULL;
    char *architecture = NULL;
    char *locale = NULL;
    gboolean list_languages = FALSE;
    gboolean url_only = FALSE;
    GOptionEntry entries[] = {
        {"kind", 'k', 0, G_OPTION_ARG_STRING, &kind, "Download type", "windows|uefi-shell"},
        {"output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "New output file", "PATH"},
        {"release", 'r', 0, G_OPTION_ARG_STRING, &release, "Windows release", "windows11|windows10"},
        {"language", 'l', 0, G_OPTION_ARG_STRING, &language, "Windows language", "NAME"},
        {"architecture", 'a', 0, G_OPTION_ARG_STRING, &architecture, "Windows architecture", "x64|arm64|x86"},
        {"locale", 0, 0, G_OPTION_ARG_STRING, &locale, "Microsoft catalog locale", "LOCALE"},
        {"list-languages", 0, 0, G_OPTION_ARG_NONE, &list_languages, "List currently offered languages", NULL},
        {"url-only", 0, 0, G_OPTION_ARG_NONE, &url_only, "Print the temporary official URL", NULL},
        {NULL}
    };
    GOptionContext *context = g_option_context_new("download --kind windows|uefi-shell [OPTIONS]");
    GError *error = NULL;
    LufusWindowsCatalog *catalog = NULL;
    char *url = NULL;
    int last_percent = -1;
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error) || kind == NULL) {
        g_printerr("tuxflash download: %s\n", error != NULL ? error->message :
                   "--kind windows or --kind uefi-shell is required");
        g_clear_error(&error);
        status = 64;
        goto out;
    }
    if (g_strcmp0(kind, "uefi-shell") == 0) {
        if (url_only && !list_languages) {
            g_print("%s\n", shell_url);
            goto out;
        }
        if (output == NULL || !g_path_is_absolute(output) ||
            g_file_test(output, G_FILE_TEST_EXISTS) || list_languages) {
            g_printerr("tuxflash download: UEFI Shell requires an absolute, new --output path\n");
            status = 64;
            goto out;
        }
        if (!lufus_download_url(shell_url, output, cli_download_progress,
                                &last_percent, NULL, &error)) {
            g_printerr("\ntuxflash download: %s\n", error->message);
            g_clear_error(&error);
            status = 1;
            goto out;
        }
        g_printerr("\n");
        if (!sha256_matches(output, shell_sha256, &error)) {
            g_printerr("tuxflash download: %s\n", error != NULL ? error->message :
                       "downloaded file failed its pinned SHA-256 check");
            g_clear_error(&error);
            g_unlink(output);
            status = 1;
            goto out;
        }
        g_print("Downloaded and verified UEFI Shell 2.2 26H1: %s\n", output);
    } else if (g_strcmp0(kind, "windows") == 0) {
        const LufusWindowsLanguage *selected;
        if (release == NULL)
            release = g_strdup("windows11");
        if (locale == NULL)
            locale = g_strdup("en-US");
        if (architecture == NULL)
            architecture = g_strdup("x64");
        catalog = lufus_windows_catalog_discover_for_architecture(
            release, locale, architecture, NULL, &error);
        if (catalog == NULL) {
            g_printerr("tuxflash download: %s\n", error->message);
            g_clear_error(&error);
            status = 1;
            goto out;
        }
        if (list_languages) {
            guint index;
            g_print("%s\n", catalog->release_name);
            for (index = 0; index < catalog->languages->len; index++) {
                const LufusWindowsLanguage *item = g_ptr_array_index(catalog->languages, index);
                g_print("%s\t%s\n", item->name, item->display_name);
            }
            goto out;
        }
        if (language == NULL)
            language = g_strdup("English International");
        selected = lufus_windows_catalog_find_language(catalog, language);
        if (selected == NULL) {
            g_printerr("tuxflash download: language '%s' is not currently offered; use --list-languages\n",
                       language);
            status = 64;
            goto out;
        }
        url = lufus_windows_catalog_resolve(catalog, selected, architecture, NULL, &error);
        if (url == NULL) {
            g_printerr("tuxflash download: %s\n", error->message);
            g_clear_error(&error);
            status = 1;
            goto out;
        }
        if (url_only) {
            g_print("%s\n", url);
            goto out;
        }
        if (output == NULL || !g_path_is_absolute(output) ||
            g_file_test(output, G_FILE_TEST_EXISTS)) {
            g_printerr("tuxflash download: Windows download requires an absolute, new --output path\n");
            status = 64;
            goto out;
        }
        g_print("Downloading %s, %s, %s directly from Microsoft\n",
                catalog->release_name, selected->display_name, architecture);
        if (!lufus_download_url(url, output, cli_download_progress,
                                &last_percent, NULL, &error)) {
            g_printerr("\ntuxflash download: %s\n", error->message);
            g_clear_error(&error);
            status = 1;
            goto out;
        }
        g_printerr("\n");
        g_print("Downloaded official Microsoft ISO: %s\n", output);
    } else {
        g_printerr("tuxflash download: unsupported kind '%s'\n", kind);
        status = 64;
    }
out:
    lufus_windows_catalog_free(catalog);
    g_free(url);
    g_free(kind);
    g_free(output);
    g_free(release);
    g_free(language);
    g_free(architecture);
    g_free(locale);
    g_option_context_free(context);
    return status;
}

static gboolean confirm_write(const LufusJob *job)
{
    char response[4096];
    char *size;
    if (!isatty(STDIN_FILENO)) {
        g_printerr("Refusing a destructive non-interactive write without --yes\n");
        return FALSE;
    }
    if ((job->windows.flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
        g_printerr("WARNING: This media will unconditionally erase Windows Setup Disk 0 "
                   "when booted. Disconnect every other disk from the target PC.\n"
                   "Type exactly 'ERASE WINDOWS DISK 0' to accept this risk: ");
        if (fgets(response, sizeof(response), stdin) == NULL)
            return FALSE;
        g_strchomp(response);
        if (strcmp(response, "ERASE WINDOWS DISK 0") != 0)
            return FALSE;
    }
    size = lufus_format_size(job->device_size);
    g_printerr("ALL DATA on %s (%s) will be destroyed.\n"
               "Type exactly 'WRITE %s' to continue: ",
               job->device_path, size, job->device_path);
    g_free(size);
    if (fgets(response, sizeof(response), stdin) == NULL)
        return FALSE;
    g_strchomp(response);
    {
        char *expected = g_strdup_printf("WRITE %s", job->device_path);
        gboolean confirmed = strcmp(response, expected) == 0;
        g_free(expected);
        return confirmed;
    }
}

static gboolean confirm_capture(const LufusJob *job)
{
    char response[4096];
    char *size;
    if (!isatty(STDIN_FILENO)) {
        g_printerr("Refusing non-interactive device access without --yes\n");
        return FALSE;
    }
    size = lufus_format_size(job->device_size);
    g_printerr("%s (%s) will be unmounted and read in full; the source is not written.\n"
               "Type exactly 'READ %s' to continue: ",
               job->device_path, size, job->device_path);
    g_free(size);
    if (fgets(response, sizeof(response), stdin) == NULL)
        return FALSE;
    g_strchomp(response);
    {
        char *expected = g_strdup_printf("READ %s", job->device_path);
        gboolean confirmed = strcmp(response, expected) == 0;
        g_free(expected);
        return confirmed;
    }
}

static volatile sig_atomic_t cli_child_pid = -1;
static volatile sig_atomic_t cli_cancel_fd = -1;
static volatile sig_atomic_t cli_uses_cancel_pipe;

static void forward_cli_signal(int signal_number)
{
    int saved_errno = errno;
    if (cli_uses_cancel_pipe) {
        int descriptor = cli_cancel_fd;
        cli_cancel_fd = -1;
        if (descriptor >= 0) close(descriptor);
    } else if (cli_child_pid > 0)
        kill((pid_t)cli_child_pid, signal_number);
    errno = saved_errno;
}

static int stream_process(GSubprocess *process)
{
    GDataInputStream *output = g_data_input_stream_new(g_subprocess_get_stdout_pipe(process));
    GError *error = NULL;
    char *line;
    gsize length;
    int status;
    int last_tenth = -1;
    unsigned long long last_indeterminate = 0;
    double stage_start = 0.0;
    double stage_end = 1.0;
    double stage_last_relative = 0.0;
    char *stage_message = NULL;
    gboolean stage_percent = FALSE;
    const char *identifier = g_subprocess_get_identifier(process);
    struct sigaction action = {0};
    struct sigaction old_interrupt;
    struct sigaction old_terminate;
    if (identifier != NULL)
        cli_child_pid = (sig_atomic_t)g_ascii_strtoll(identifier, NULL, 10);
    GOutputStream *control = g_subprocess_get_stdin_pipe(process);
    cli_uses_cancel_pipe = control != NULL;
    if (control != NULL) {
        cli_cancel_fd = fcntl(g_unix_output_stream_get_fd(G_UNIX_OUTPUT_STREAM(control)), F_DUPFD_CLOEXEC, 3);
        /* Keep the duplicate as the only writer. close(2) is signal-safe. */
        g_output_stream_close(control, NULL, NULL);
    }
    action.sa_handler = forward_cli_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, &old_interrupt);
    sigaction(SIGTERM, &action, &old_terminate);
    while ((line = g_data_input_stream_read_line(output, &length, NULL, &error)) != NULL) {
        if (g_str_has_prefix(line, "STAGE ") ||
            g_str_has_prefix(line, "STAGE_PERCENT ") ||
            g_str_has_prefix(line, "STAGE_TEXT ")) {
            gboolean reports_percent = g_str_has_prefix(line, "STAGE_PERCENT ");
            gboolean reports_text = g_str_has_prefix(line, "STAGE_TEXT ");
            const char *values = line + (reports_percent ? 14 : reports_text ? 11 : 6);
            unsigned int start = 0;
            unsigned int end = 0;
            int message_offset = 0;
            if (sscanf(values, "%u %u %n", &start, &end, &message_offset) == 2 &&
                start <= end && end <= 1000) {
                stage_start = start / 1000.0;
                stage_end = end / 1000.0;
                stage_last_relative = 0.0;
                stage_percent = reports_percent;
                g_free(stage_message);
                stage_message = g_strdup(values + message_offset);
                last_tenth = -1;
                g_printerr("\n%s\n", values + message_offset);
            }
        } else if (g_str_has_prefix(line, "PROGRESS_PERCENT ")) {
            unsigned int thousandths = 0;
            if (sscanf(line + 17, "%u", &thousandths) == 1 && thousandths <= 1000) {
                double relative = thousandths / 1000.0;
                relative = MAX(relative, stage_last_relative);
                stage_last_relative = relative;
                int tenth = (int)(1000.0 *
                                  (stage_start + relative * (stage_end - stage_start)));
                if (tenth != last_tenth) {
                    if (stage_percent && stage_message != NULL)
                        g_printerr("\r%s: %5.1f%% (overall %5.1f%%)", stage_message,
                                   relative * 100.0, tenth / 10.0);
                    else
                        g_printerr("\rProgress: %5.1f%%", tenth / 10.0);
                    last_tenth = tenth;
                }
            }
        } else if (g_str_has_prefix(line, "PROGRESS ")) {
            unsigned long long written = 0;
            unsigned long long total = 0;
            if (sscanf(line + 9, "%llu %llu", &written, &total) == 2 && total > 0) {
                double relative = (double)written / (double)total;
                relative = MAX(relative, stage_last_relative);
                stage_last_relative = relative;
                int tenth = (int)(1000.0 *
                                  (stage_start + relative * (stage_end - stage_start)));
                if (tenth != last_tenth) {
                    if (stage_percent && stage_message != NULL)
                        g_printerr("\r%s: %5.1f%% (overall %5.1f%%)", stage_message,
                                   relative * 100.0, tenth / 10.0);
                    else
                        g_printerr("\rProgress: %5.1f%%", tenth / 10.0);
                    last_tenth = tenth;
                }
            } else if (written >= last_indeterminate + 1024U * 1024U) {
                g_printerr("\rWritten: %llu bytes", written);
                last_indeterminate = written;
            }
        } else if (g_str_has_prefix(line, "VERIFY_PROGRESS ")) {
            unsigned long long checked = 0;
            unsigned long long total = 0;
            if (sscanf(line + 16, "%llu %llu", &checked, &total) == 2 && total > 0) {
                double relative = (double)checked / (double)total;
                double overall = stage_start + relative * (stage_end - stage_start);
                g_printerr("\rVerifying: %5.1f%%", 100.0 * overall);
            }
        } else {
            g_printerr("\n%s\n", line);
        }
        g_free(line);
    }
    if (error != NULL) {
        g_printerr("\ntuxflash write: %s\n", error->message);
        g_clear_error(&error);
    }
    g_printerr("\n");
    if (!g_subprocess_wait(process, NULL, &error)) {
        g_printerr("tuxflash write: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
    } else {
        status = g_subprocess_get_exit_status(process);
    }
    cli_child_pid = -1;
    sigaction(SIGINT, &old_interrupt, NULL);
    sigaction(SIGTERM, &old_terminate, NULL);
    if (cli_cancel_fd >= 0) close(cli_cancel_fd);
    cli_cancel_fd = -1;
    cli_uses_cancel_pipe = 0;
    g_free(stage_message);
    g_object_unref(output);
    return status;
}

static int command_write(int argc, char **argv, const char *helper_path)
{
    char *image = NULL;
    char *device = NULL;
    char *mode = NULL;
    char *architecture = NULL;
    char *local_account = NULL;
    gboolean assume_yes = FALSE;
    gboolean verify = FALSE;
    gboolean bypass_requirements = FALSE;
    gboolean no_online_account = FALSE;
    gboolean no_data_collection = FALSE;
    gboolean disable_bitlocker = FALSE;
    gboolean offline_internal_drives = FALSE;
    gboolean silent_install = FALSE;
    gboolean uefi_ntfs = FALSE;
    gboolean runtime_validation = FALSE;
    gboolean no_runtime_validation = FALSE;
    char *target_system = NULL;
    gint persistence_mib = 4096;
    gint windows_index = 1;
    gint test_passes = 0;
    GOptionEntry entries[] = {
        {"image", 'i', 0, G_OPTION_ARG_FILENAME, &image, "Image file", "PATH"},
        {"device", 'd', 0, G_OPTION_ARG_FILENAME, &device,
         "Whole target device; /dev/disk/by-id is recommended", "DEVICE"},
        {"mode", 'm', 0, G_OPTION_ARG_STRING, &mode,
         "Write mode: raw, windows-installer, windows-to-go, or linux-persistent", "MODE"},
        {"windows-index", 0, 0, G_OPTION_ARG_INT, &windows_index,
         "1-based Windows edition index for silent install or Windows To Go", "INDEX"},
        {"verify", 'v', 0, G_OPTION_ARG_NONE, &verify, "Verify after writing", NULL},
        {"bypass-windows-requirements", 0, 0, G_OPTION_ARG_NONE, &bypass_requirements,
         "Bypass Windows TPM, Secure Boot, and RAM checks", NULL},
        {"no-online-account", 0, 0, G_OPTION_ARG_NONE, &no_online_account,
         "Remove the Windows online-account requirement", NULL},
        {"no-data-collection", 0, 0, G_OPTION_ARG_NONE, &no_data_collection,
         "Skip Windows data-collection questions", NULL},
        {"disable-bitlocker", 0, 0, G_OPTION_ARG_NONE, &disable_bitlocker,
         "Prevent automatic Windows device encryption", NULL},
        {"offline-internal-drives", 0, 0, G_OPTION_ARG_NONE, &offline_internal_drives,
         "Set internal disks offline (primarily for Windows To Go)", NULL},
        {"silent-install", 0, 0, G_OPTION_ARG_NONE, &silent_install,
         "Create unattended media that erases Setup Disk 0", NULL},
        {"uefi-ntfs", 0, 0, G_OPTION_ARG_NONE, &uefi_ntfs,
         "Use an NTFS payload plus the signed UEFI:NTFS loader", NULL},
        {"target-system", 0, 0, G_OPTION_ARG_STRING, &target_system,
         "Windows target firmware", "uefi|bios|bios-uefi"},
        {"runtime-validation", 0, 0, G_OPTION_ARG_NONE, &runtime_validation,
         "Enable signed UEFI media validation (the default)", NULL},
        {"no-runtime-validation", 0, 0, G_OPTION_ARG_NONE, &no_runtime_validation,
         "Skip the post-write media validation pass", NULL},
        {"persistence-size", 0, 0, G_OPTION_ARG_INT, &persistence_mib,
         "Persistent Linux partition size in MiB", "MIB"},
        {"test-passes", 0, 0, G_OPTION_ARG_INT, &test_passes,
         "Destructive fake-drive/bad-block passes before writing (1-4)", "N"},
        {"local-account", 0, 0, G_OPTION_ARG_STRING, &local_account,
         "Create a local Windows account", "NAME"},
        {"architecture", 0, 0, G_OPTION_ARG_STRING, &architecture,
         "Windows image architecture", "amd64|x86|arm64|arm"},
        {"yes", 'y', 0, G_OPTION_ARG_NONE, &assume_yes,
         "Confirm destruction (required for non-interactive use)", NULL},
        {NULL}
    };
    GOptionContext *context = g_option_context_new(
        "write --image PATH --device DEVICE [--mode MODE] [OPTIONS]");
    GError *error = NULL;
    LufusJob *job = NULL;
    GSubprocess *process = NULL;
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error) ||
        image == NULL || device == NULL || test_passes < 0 || test_passes > 4) {
        g_printerr("tuxflash write: %s\n", error != NULL ? error->message :
                   "--image and --device are required");
        g_clear_error(&error);
        status = 64;
        goto out;
    }
    if (runtime_validation && no_runtime_validation) {
        g_printerr("tuxflash write: --runtime-validation and --no-runtime-validation cannot be used together\n");
        status = 64;
        goto out;
    }
    if (mode == NULL || strcmp(mode, "raw") == 0) {
        if (bypass_requirements || no_online_account || no_data_collection ||
            disable_bitlocker || offline_internal_drives || silent_install ||
            local_account != NULL || windows_index != 1 ||
            architecture != NULL || uefi_ntfs || target_system != NULL ||
            runtime_validation || no_runtime_validation) {
            g_printerr("tuxflash write: Windows options require a Windows write mode\n");
            status = 64;
            goto out;
        }
        job = lufus_job_new_raw(image, device, verify, &error);
    } else if (strcmp(mode, "windows-installer") == 0 ||
               strcmp(mode, "windows-to-go") == 0) {
        LufusWindowsOptions windows = {0};
        if (target_system != NULL && g_strcmp0(target_system, "uefi") != 0 &&
            g_strcmp0(target_system, "bios") != 0 &&
            g_strcmp0(target_system, "bios-uefi") != 0) {
            g_printerr("tuxflash write: --target-system must be uefi, bios, or bios-uefi\n");
            status = 64;
            goto out;
        }
        if (verify) {
            g_printerr("tuxflash write: --verify currently applies only to raw mode\n");
            status = 64;
            goto out;
        }
        windows.flags = (bypass_requirements ? LUFUS_WINDOWS_BYPASS_REQUIREMENTS : 0) |
                        (no_online_account ? LUFUS_WINDOWS_NO_ONLINE_ACCOUNT : 0) |
                        (no_data_collection ? LUFUS_WINDOWS_NO_DATA_COLLECTION : 0) |
                        (disable_bitlocker ? LUFUS_WINDOWS_DISABLE_BITLOCKER : 0) |
                        (offline_internal_drives ? LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES : 0) |
                        (local_account != NULL ? LUFUS_WINDOWS_CREATE_LOCAL_ACCOUNT : 0) |
                        (silent_install ? LUFUS_WINDOWS_SILENT_INSTALL : 0);
        windows.image_index = (guint)(windows_index > 0 ? windows_index : 0);
        windows.architecture = architecture != NULL ? architecture : "amd64";
        windows.local_account = local_account;
        if (windows_index < 1)
            g_set_error_literal(&error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                "--windows-index must be 1 or greater");
        else if (strcmp(mode, "windows-installer") == 0 &&
                 windows_index != 1 && !silent_install)
            g_set_error_literal(&error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                "--windows-index for installer mode requires --silent-install");
        else if (strcmp(mode, "windows-installer") == 0)
            job = lufus_job_new_windows_installer(
                                                  image, device, uefi_ntfs,
                                                  g_strcmp0(target_system, "bios") == 0 ?
                                                    LUFUS_WINDOWS_TARGET_BIOS :
                                                  g_strcmp0(target_system, "bios-uefi") == 0 ?
                                                    LUFUS_WINDOWS_TARGET_BIOS_UEFI :
                                                    LUFUS_WINDOWS_TARGET_UEFI,
                                                  (runtime_validation ||
                                                   lufus_job_runtime_validation_default(
                                                     LUFUS_JOB_WINDOWS_INSTALLER)) &&
                                                  !no_runtime_validation,
                                                  &windows, &error);
        else if (silent_install)
            g_set_error_literal(&error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                "--silent-install applies only to windows-installer mode");
        else if (runtime_validation || no_runtime_validation)
            g_set_error_literal(&error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                "runtime media validation is not available for windows-to-go mode");
        else if (uefi_ntfs)
            g_set_error_literal(&error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                "--uefi-ntfs applies only to windows-installer mode");
        else
            job = lufus_job_new_windows_to_go(image, device, (guint)windows_index,
                                              &windows, &error);
    } else if (strcmp(mode, "linux-persistent") == 0) {
        if (verify || bypass_requirements || no_online_account || no_data_collection ||
            disable_bitlocker || offline_internal_drives || silent_install ||
            local_account != NULL ||
            architecture != NULL || uefi_ntfs || target_system != NULL ||
            windows_index != 1 || persistence_mib < 256) {
            g_printerr("tuxflash write: invalid option for linux-persistent mode or persistence is below 256 MiB\n");
            status = 64;
            goto out;
        }
        job = lufus_job_new_linux_persistent(image, device,
                                              (guint64)persistence_mib * 1024U * 1024U,
                                              (runtime_validation ||
                                               lufus_job_runtime_validation_default(
                                                 LUFUS_JOB_LINUX_PERSISTENT)) &&
                                              !no_runtime_validation, &error);
    } else {
        g_printerr("tuxflash write: unknown mode '%s'\n", mode);
        status = 64;
        goto out;
    }
    if (job == NULL) {
        gboolean option_error = g_error_matches(error, G_OPTION_ERROR,
                                                 G_OPTION_ERROR_BAD_VALUE);
        g_printerr("tuxflash write: %s\n", error->message);
        g_clear_error(&error);
        status = option_error ? 64 : 1;
        goto out;
    }
    if (test_passes > 0 &&
        !lufus_job_set_pretest(job, (guint)test_passes, &error)) {
        g_printerr("tuxflash write: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    if (!assume_yes && !confirm_write(job)) {
        g_printerr("Write cancelled.\n");
        status = 2;
        goto out;
    }
    if (assume_yes && (job->windows.flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0)
        g_printerr("WARNING: unattended media will erase Windows Setup Disk 0 when booted.\n");
    process = lufus_job_spawn(job, helper_path, geteuid() != 0, &error);
    if (process == NULL) {
        g_printerr("tuxflash write: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    status = stream_process(process);

out:
    g_clear_object(&process);
    lufus_job_free(job);
    g_free(image);
    g_free(device);
    g_free(mode);
    g_free(architecture);
    g_free(local_account);
    g_free(target_system);
    g_option_context_free(context);
    return status;
}

static int command_device_test(int argc, char **argv, const char *helper_path)
{
    char *device = NULL;
    gint passes = 1;
    gboolean assume_yes = FALSE;
    GOptionEntry entries[] = {
        {"device", 'd', 0, G_OPTION_ARG_FILENAME, &device, "Whole target device", "DEVICE"},
        {"passes", 'p', 0, G_OPTION_ARG_INT, &passes, "Destructive test passes (1-4)", "N"},
        {"yes", 'y', 0, G_OPTION_ARG_NONE, &assume_yes, "Confirm destruction", NULL},
        {NULL}
    };
    GOptionContext *context = g_option_context_new("test-device --device DEVICE [--passes N]");
    GError *error = NULL;
    LufusJob *job = NULL;
    GSubprocess *process = NULL;
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error) || device == NULL ||
        passes < 1 || passes > 4) {
        g_printerr("tuxflash test-device: %s\n", error != NULL ? error->message :
                   "--device and passes from 1 through 4 are required");
        g_clear_error(&error);
        status = 64;
        goto out;
    }
    job = lufus_job_new_device_test(device, (guint)passes, &error);
    if (job == NULL) {
        g_printerr("tuxflash test-device: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    if (!assume_yes && !confirm_write(job)) {
        status = 2;
        goto out;
    }
    process = lufus_job_spawn(job, helper_path, geteuid() != 0, &error);
    if (process == NULL) {
        g_printerr("tuxflash test-device: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    status = stream_process(process);
out:
    g_clear_object(&process);
    lufus_job_free(job);
    g_free(device);
    g_option_context_free(context);
    return status;
}

static int command_capture(int argc, char **argv, const char *helper_path)
{
    char *device = NULL;
    char *output = NULL;
    char *format = NULL;
    gboolean assume_yes = FALSE;
    GOptionEntry entries[] = {
        {"device", 'd', 0, G_OPTION_ARG_FILENAME, &device, "Whole source device", "DEVICE"},
        {"output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "New image output path", "PATH"},
        {"format", 'f', 0, G_OPTION_ARG_STRING, &format, "Image format", "raw|vhd|vhdx"},
        {"yes", 'y', 0, G_OPTION_ARG_NONE, &assume_yes, "Confirm device access", NULL},
        {NULL}
    };
    GOptionContext *context = g_option_context_new("capture --device DEVICE --output PATH --format FORMAT");
    GError *error = NULL;
    LufusJob *job = NULL;
    GSubprocess *process = NULL;
    LufusCaptureFormat capture_format;
    int status = 0;
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error) || device == NULL ||
        output == NULL || format == NULL) {
        g_printerr("tuxflash capture: %s\n", error != NULL ? error->message :
                   "--device, --output and --format are required");
        g_clear_error(&error);
        status = 64;
        goto out;
    }
    if (strcmp(format, "raw") == 0)
        capture_format = LUFUS_CAPTURE_RAW;
    else if (strcmp(format, "vhd") == 0)
        capture_format = LUFUS_CAPTURE_VHD;
    else if (strcmp(format, "vhdx") == 0)
        capture_format = LUFUS_CAPTURE_VHDX;
    else {
        g_printerr("tuxflash capture: unsupported format '%s'\n", format);
        status = 64;
        goto out;
    }
    job = lufus_job_new_capture(device, output, capture_format, &error);
    if (job == NULL) {
        g_printerr("tuxflash capture: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    if (!assume_yes && !confirm_capture(job)) {
        status = 2;
        goto out;
    }
    process = lufus_job_spawn(job, helper_path, geteuid() != 0, &error);
    if (process == NULL) {
        g_printerr("tuxflash capture: %s\n", error->message);
        g_clear_error(&error);
        status = 1;
        goto out;
    }
    status = stream_process(process);
out:
    g_clear_object(&process);
    lufus_job_free(job);
    g_free(device);
    g_free(output);
    g_free(format);
    g_option_context_free(context);
    return status;
}

static int command_doctor(int argc, const char *helper_path)
{
    if (argc != 1) {
        g_printerr("Usage: tuxflash doctor\n");
        return 64;
    }
    GError *error = NULL;
    GSubprocess *process = g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, &error,
                                            helper_path, "--check-tools", NULL);
    if (process == NULL) {
        g_printerr("TuxFlash helper is unavailable: %s\n", error->message);
        g_clear_error(&error);
        return 69;
    }
    gboolean waited = g_subprocess_wait(process, NULL, &error);
    int status = waited && g_subprocess_get_if_exited(process) ?
                 g_subprocess_get_exit_status(process) : 1;
    g_clear_error(&error);
    g_object_unref(process);
    return status;
}

void lufus_cli_print_help(void)
{
    g_print(
        "TuxFlash - Bootable USB Creator for Linux\n\n"
        "Usage:\n"
        "  tuxflash                         Open the graphical interface\n"
        "  tuxflash doctor                  Check required runtime tools\n"
        "  tuxflash list [--all]            List eligible target devices\n"
        "  tuxflash checksum --image PATH   Calculate image checksums\n"
        "  tuxflash probe --image PATH      Inspect ISO capabilities without mounting\n"
        "  tuxflash download [OPTIONS]      Discover/download official Microsoft or UEFI Shell ISOs\n"
        "  tuxflash write [OPTIONS]         Write a raw image or Windows installer\n"
        "  tuxflash test-device [OPTIONS]   Destructively test real capacity and blocks\n"
        "  tuxflash capture [OPTIONS]       Save a device as raw, VHD, or VHDX\n"
        "  tuxflash --version               Show the version\n\n"
        "Run 'tuxflash COMMAND --help' for command-specific options.\n");
}

int lufus_cli_main(int argc, char **argv, const char *helper_path)
{
    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        lufus_cli_print_help();
        return 0;
    }
    argc--;
    argv++;
    if (strcmp(argv[0], "doctor") == 0)
        return command_doctor(argc, helper_path);
    if (strcmp(argv[0], "list") == 0)
        return command_list(argc, argv);
    if (strcmp(argv[0], "probe") == 0)
        return command_probe(argc, argv);
    if (strcmp(argv[0], "checksum") == 0)
        return command_checksum(argc, argv);
    if (strcmp(argv[0], "download") == 0)
        return command_download(argc, argv);
    if (strcmp(argv[0], "write") == 0)
        return command_write(argc, argv, helper_path);
    if (strcmp(argv[0], "test-device") == 0)
        return command_device_test(argc, argv, helper_path);
    if (strcmp(argv[0], "capture") == 0)
        return command_capture(argc, argv, helper_path);
    g_printerr("Unknown TuxFlash command: %s\n", argv[0]);
    lufus_cli_print_help();
    return 64;
}
