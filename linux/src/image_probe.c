/*
 * Linux-native ISO capability scanner
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "image_probe.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static gboolean basename_is(const char *path, const char *name)
{
    const char *basename = strrchr(path, '/');
    basename = basename != NULL ? basename + 1 : path;
    return strcmp(basename, name) == 0;
}

static gboolean basename_has_prefix(const char *path, const char *prefix)
{
    const char *basename = strrchr(path, '/');
    basename = basename != NULL ? basename + 1 : path;
    return g_str_has_prefix(basename, prefix);
}

static gboolean is_efi_loader(const char *path)
{
    const char *basename;
    gsize length;

    if (!g_str_has_prefix(path, "/efi/"))
        return FALSE;
    basename = strrchr(path, '/');
    basename = basename != NULL ? basename + 1 : path;
    length = strlen(basename);
    if (length <= 4 || !g_str_has_suffix(basename, ".efi"))
        return FALSE;
    return g_str_has_prefix(basename, "boot") ||
           g_str_has_prefix(basename, "grub") ||
           g_str_has_prefix(basename, "mm");
}

static void scan_path(LufusImageReport *report, const char *input,
                      guint *winpe_i386, guint *winpe_amd64,
                      guint *winpe_minint)
{
    char *lower = g_utf8_strdown(input, -1);
    char *path = lower[0] == '/' ? lower : g_strconcat("/", lower, NULL);
    guint *winpe_bits = NULL;

    if (strcmp(path, "/bootmgr") == 0)
        report->has_bootmgr = TRUE;
    else if (strcmp(path, "/bootmgr.efi") == 0) {
        report->has_bootmgr_efi = TRUE;
        report->has_efi = TRUE;
    }

    if (strcmp(path, "/sources/boot.wim") == 0)
        report->has_boot_wim = TRUE;
    if (strcmp(path, "/sources/install.wim") == 0 ||
        strcmp(path, "/sources/install.esd") == 0 ||
        strcmp(path, "/sources/install.swm") == 0)
        report->has_install_image = TRUE;

    if (is_efi_loader(path))
        report->has_efi = TRUE;
    if (g_str_has_prefix(path, "/boot/grub/") ||
        g_str_has_prefix(path, "/boot/grub2/") ||
        basename_is(path, "grldr") ||
        (g_str_has_prefix(path, "/efi/") &&
         basename_has_prefix(path, "grub") &&
         g_str_has_suffix(path, ".efi")))
        report->has_grub = TRUE;
    if (basename_is(path, "isolinux.bin") || basename_is(path, "boot.bin"))
        report->has_syslinux = TRUE;
    if (basename_is(path, "setupldr.sys") || basename_is(path, "freeldr.sys"))
        report->has_reactos = TRUE;
    if (basename_is(path, "kolibri.img"))
        report->has_kolibrios = TRUE;
    if (g_str_has_prefix(path, "/casper/"))
        report->uses_casper = TRUE;
    if (g_str_has_prefix(path, "/live/"))
        report->uses_debian_live = TRUE;

    if (g_str_has_prefix(path, "/i386/"))
        winpe_bits = winpe_i386;
    else if (g_str_has_prefix(path, "/amd64/"))
        winpe_bits = winpe_amd64;
    else if (g_str_has_prefix(path, "/minint/"))
        winpe_bits = winpe_minint;
    if (winpe_bits != NULL) {
        if (basename_is(path, "ntdetect.com"))
            *winpe_bits |= 1U;
        else if (basename_is(path, "setupldr.bin"))
            *winpe_bits |= 2U;
        else if (basename_is(path, "txtsetup.sif"))
            *winpe_bits |= 4U;
    }

    if (path != lower)
        g_free(path);
    g_free(lower);
}

void lufus_image_report_from_listing(const char *listing,
                                     LufusImageReport *report)
{
    char **lines;
    guint winpe_i386 = 0;
    guint winpe_amd64 = 0;
    guint winpe_minint = 0;
    guint index;

    g_return_if_fail(report != NULL);
    memset(report, 0, sizeof(*report));
    report->is_iso = TRUE;
    lines = g_strsplit(listing != NULL ? listing : "", "\n", -1);
    for (index = 0; lines[index] != NULL; index++) {
        char *line = g_strstrip(lines[index]);
        char *path;
        GError *unquote_error = NULL;

        if (line[0] == '\0')
            continue;
        if (g_str_has_prefix(line, "Path = "))
            path = g_strdup(line + strlen("Path = "));
        else {
            path = g_shell_unquote(line, &unquote_error);
            if (path == NULL) {
                g_clear_error(&unquote_error);
                path = g_strdup(line);
            }
        }
        scan_path(report, path, &winpe_i386, &winpe_amd64, &winpe_minint);
        g_free(path);
    }
    g_strfreev(lines);

    report->has_winpe = winpe_i386 == 7U || winpe_amd64 == 7U ||
                        winpe_minint == 7U;
    /* These mirror upstream Rufus' HAS_WINDOWS, HAS_WINTOGO, and
     * HAS_PERSISTENCE decisions, narrowed only where the Linux helper needs
     * the modern sources/boot.wim plus install-image layout it can process. */
    report->has_windows = report->has_bootmgr || report->has_bootmgr_efi ||
                          report->has_winpe;
    report->supports_windows_installer =
        report->has_windows && report->has_boot_wim &&
        report->has_install_image;
    report->supports_windows_to_go =
        (report->has_bootmgr || report->has_bootmgr_efi) &&
        report->has_efi && report->has_install_image;
    report->supports_persistence =
        (report->has_syslinux || report->has_grub) &&
        (report->uses_casper || report->uses_debian_live) &&
        !(report->has_windows || report->has_reactos ||
          report->has_kolibrios);
}

static gboolean run_listing_command(const char *const *arguments,
                                    char **stdout_text,
                                    char **failure_detail)
{
    GSubprocess *process;
    char *stderr_text = NULL;
    GError *error = NULL;
    gboolean communicated;

    *stdout_text = NULL;
    *failure_detail = NULL;
    process = g_subprocess_newv(
        arguments,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (process == NULL) {
        *failure_detail = g_strdup(error->message);
        g_clear_error(&error);
        return FALSE;
    }
    communicated = g_subprocess_communicate_utf8(
        process, NULL, NULL, stdout_text, &stderr_text, &error);
    if (!communicated) {
        *failure_detail = g_strdup(error->message);
        g_clear_error(&error);
        g_object_unref(process);
        g_clear_pointer(stdout_text, g_free);
        g_free(stderr_text);
        return FALSE;
    }
    if (!g_subprocess_get_successful(process)) {
        const char *detail = stderr_text != NULL ? g_strstrip(stderr_text) : "";
        *failure_detail = g_strdup(detail[0] != '\0' ? detail :
                                  "image listing command failed");
        g_object_unref(process);
        g_clear_pointer(stdout_text, g_free);
        g_free(stderr_text);
        return FALSE;
    }
    g_object_unref(process);
    g_free(stderr_text);
    return TRUE;
}

static gboolean has_optical_descriptor(const char *path, GError **error)
{
    struct stat details;
    unsigned char descriptor[7];
    gboolean recognized = FALSE;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        int saved = errno;
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(saved),
                    "Could not open ISO: %s", g_strerror(saved));
        return FALSE;
    }
    if (fstat(fd, &details) == 0 && S_ISREG(details.st_mode)) {
        /* ISO9660 descriptors and UDF's volume recognition sequence begin at
         * logical sector 16. A successful listing alone is not evidence of an
         * ISO: xorriso can treat arbitrary data as an empty, blank medium. */
        for (off_t sector = 16; sector < 32; sector++) {
            ssize_t count;
            if (details.st_size < sector * 2048 + (off_t)sizeof(descriptor))
                break;
            do {
                count = pread(fd, descriptor, sizeof(descriptor), sector * 2048);
            } while (count < 0 && errno == EINTR);
            if (count != (ssize_t)sizeof(descriptor))
                break;
            if (memcmp(descriptor + 1, "CD001", 5) == 0 ||
                memcmp(descriptor + 1, "NSR02", 5) == 0 ||
                memcmp(descriptor + 1, "NSR03", 5) == 0) {
                recognized = TRUE;
                break;
            }
        }
    }
    close(fd);
    if (!recognized)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "The file has no ISO9660 or UDF volume descriptor");
    return recognized;
}

gboolean lufus_image_probe_iso(const char *path,
                               LufusImageReport *report,
                               GError **error)
{
    char *seven_zip = g_find_program_in_path("7z");
    const char *seven_zip_arguments[] = {
        seven_zip != NULL ? seven_zip : "/usr/bin/7z", "l", "-ba", "-slt", "--", path, NULL
    };
    const char *xorriso_arguments[] = {
        "/usr/bin/xorriso", "-report_about", "FAILURE", "-indev", path,
        "-find", "/", "-type", "f", "-exec", "echo", "--", NULL
    };
    char *listing = NULL;
    char *seven_zip_failure = NULL;
    char *xorriso_failure = NULL;

    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(report != NULL, FALSE);
    memset(report, 0, sizeof(*report));
    if (!has_optical_descriptor(path, error)) {
        g_free(seven_zip);
        return FALSE;
    }

    /* Microsoft's retail media commonly exposes its full tree only through
     * UDF while retaining a one-file ISO9660 compatibility view. 7-Zip reads
     * that UDF tree without mounting or privileges. xorriso remains the
     * fallback for ISO9660/Rock Ridge media that 7-Zip cannot enumerate. */
    if (run_listing_command(seven_zip_arguments, &listing,
                            &seven_zip_failure)) {
        lufus_image_report_from_listing(listing, report);
        g_free(listing);
        g_free(seven_zip);
        return TRUE;
    }
    if (run_listing_command(xorriso_arguments, &listing,
                            &xorriso_failure)) {
        lufus_image_report_from_listing(listing, report);
        g_free(listing);
        g_free(seven_zip_failure);
        g_free(seven_zip);
        return TRUE;
    }
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "Could not inspect ISO contents (7-Zip: %s; xorriso: %s)",
                seven_zip_failure != NULL ? seven_zip_failure : "failed",
                xorriso_failure != NULL ? xorriso_failure : "failed");
    g_free(seven_zip_failure);
    g_free(xorriso_failure);
    g_free(seven_zip);
    return FALSE;
}
