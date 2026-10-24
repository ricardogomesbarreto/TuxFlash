/*
 * Lufus shared job model
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "job.h"
#include "device_safety.h"

#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static GQuark lufus_job_error_quark(void)
{
    return g_quark_from_static_string("lufus-job-error");
}

gboolean lufus_job_supports_runtime_validation(LufusJobType type)
{
    return type == LUFUS_JOB_WINDOWS_INSTALLER ||
           type == LUFUS_JOB_LINUX_PERSISTENT;
}

gboolean lufus_job_runtime_validation_default(LufusJobType type)
{
    return lufus_job_supports_runtime_validation(type);
}

static gboolean read_device_size(const char *device_path, dev_t device,
                                 uint64_t *device_size)
{
    int descriptor;
    int open_error;
    char *sysfs_path;
    char *contents = NULL;
    char *end = NULL;
    guint64 sectors;

    descriptor = open(device_path, O_RDONLY | O_CLOEXEC);
    open_error = errno;
    if (descriptor >= 0) {
        if (ioctl(descriptor, BLKGETSIZE64, device_size) == 0) {
            close(descriptor);
            return TRUE;
        }
        open_error = errno;
        close(descriptor);
    }

    sysfs_path = g_strdup_printf("/sys/dev/block/%u:%u/size",
                                 major(device), minor(device));
    if (!g_file_get_contents(sysfs_path, &contents, NULL, NULL)) {
        g_free(sysfs_path);
        errno = open_error;
        return FALSE;
    }
    g_free(sysfs_path);

    errno = 0;
    sectors = g_ascii_strtoull(contents, &end, 10);
    while (end != NULL && g_ascii_isspace(*end))
        end++;
    if (errno != 0 || end == contents || end == NULL || *end != '\0' ||
        sectors > G_MAXUINT64 / 512U) {
        g_free(contents);
        errno = EOVERFLOW;
        return FALSE;
    }
    g_free(contents);
    *device_size = sectors * 512U;
    return TRUE;
}

static LufusJob *job_new(LufusJobType type,
                         const char *image_path,
                         const char *device_path,
                         GError **error)
{
    char *canonical_image;
    char *canonical_device;
    struct stat image_stat;
    struct stat device_stat;
    uint64_t device_size = 0;
    LufusJob *job;

    g_return_val_if_fail(image_path != NULL, NULL);
    g_return_val_if_fail(device_path != NULL, NULL);

    canonical_image = realpath(image_path, NULL);
    if (canonical_image == NULL || g_stat(canonical_image, &image_stat) != 0 ||
        !S_ISREG(image_stat.st_mode)) {
        g_set_error(error, lufus_job_error_quark(), 1,
                    "Image is not a readable regular file: %s", image_path);
        free(canonical_image);
        return NULL;
    }

    canonical_device = realpath(device_path, NULL);
    if (canonical_device == NULL || !g_str_has_prefix(canonical_device, "/dev/") ||
        g_stat(canonical_device, &device_stat) != 0 || !S_ISBLK(device_stat.st_mode)) {
        g_set_error(error, lufus_job_error_quark(), 2,
                    "Target is not a block device below /dev: %s", device_path);
        free(canonical_image);
        free(canonical_device);
        return NULL;
    }

    if (!read_device_size(canonical_device, device_stat.st_rdev, &device_size)) {
        int saved_errno = errno;
        g_set_error(error, lufus_job_error_quark(), 3,
                    "Could not read target capacity: %s", g_strerror(saved_errno));
        free(canonical_image);
        free(canonical_device);
        return NULL;
    }
    job = g_new0(LufusJob, 1);
    job->type = type;
    job->image_path = canonical_image;
    job->device_path = canonical_device;
    job->major_minor = g_strdup_printf("%u:%u",
                                       major(device_stat.st_rdev),
                                       minor(device_stat.st_rdev));
    job->device_size = device_size;
    job->disk_sequence = tuxflash_disk_sequence(device_stat.st_rdev);
    return job;
}

static LufusJob *device_job_new(LufusJobType type, const char *device_path,
                                GError **error)
{
    char *canonical_device;
    struct stat device_stat;
    uint64_t device_size = 0;
    LufusJob *job;

    canonical_device = realpath(device_path, NULL);
    if (canonical_device == NULL || !g_str_has_prefix(canonical_device, "/dev/") ||
        g_stat(canonical_device, &device_stat) != 0 || !S_ISBLK(device_stat.st_mode)) {
        g_set_error(error, lufus_job_error_quark(), 2,
                    "Target is not a block device below /dev: %s", device_path);
        free(canonical_device);
        return NULL;
    }
    if (!read_device_size(canonical_device, device_stat.st_rdev, &device_size)) {
        g_set_error(error, lufus_job_error_quark(), 3,
                    "Could not read target capacity: %s", g_strerror(errno));
        free(canonical_device);
        return NULL;
    }
    job = g_new0(LufusJob, 1);
    job->type = type;
    job->device_path = canonical_device;
    job->major_minor = g_strdup_printf("%u:%u", major(device_stat.st_rdev),
                                       minor(device_stat.st_rdev));
    job->device_size = device_size;
    job->disk_sequence = tuxflash_disk_sequence(device_stat.st_rdev);
    return job;
}

LufusJob *lufus_job_new_raw(const char *image_path,
                            const char *device_path,
                            gboolean verify,
                            GError **error)
{
    LufusJob *job = job_new(LUFUS_JOB_RAW_WRITE, image_path, device_path, error);
    if (job != NULL)
        job->verify = verify;
    return job;
}

LufusJob *lufus_job_new_windows_installer(const char *image_path,
                                          const char *device_path,
                                          gboolean uefi_ntfs,
                                          LufusWindowsTarget target_system,
                                          gboolean runtime_validation,
                                          const LufusWindowsOptions *options,
                                          GError **error)
{
    LufusJob *job;
    g_return_val_if_fail(options != NULL, NULL);
    if (!lufus_windows_options_validate(options, error))
        return NULL;
    if ((options->flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0 &&
        target_system != LUFUS_WINDOWS_TARGET_UEFI) {
        g_set_error_literal(error, lufus_job_error_quark(), 12,
                            "Silent installation requires UEFI Windows media");
        return NULL;
    }
    job = job_new(LUFUS_JOB_WINDOWS_INSTALLER, image_path, device_path, error);
    if (job != NULL) {
        job->uefi_ntfs = uefi_ntfs;
        job->windows_target = target_system;
        job->runtime_validation = runtime_validation;
        job->windows_index = options->image_index != 0 ? options->image_index : 1;
        job->windows.flags = options->flags;
        job->windows.image_index = job->windows_index;
        job->windows.boot_media_partition = options->boot_media_partition;
        job->windows.architecture = g_strdup(options->architecture != NULL ?
                                              options->architecture : "amd64");
        job->windows.language = g_strdup(options->language);
        job->windows.local_account = g_strdup(options->local_account);
    }
    return job;
}

LufusJob *lufus_job_new_linux_persistent(const char *image_path,
                                         const char *device_path,
                                         guint64 persistence_size,
                                         gboolean runtime_validation,
                                         GError **error)
{
    LufusJob *job;
    if (persistence_size < 256ULL * 1024ULL * 1024ULL) {
        g_set_error_literal(error, lufus_job_error_quark(), 6,
                            "Persistence must be at least 256 MiB");
        return NULL;
    }
    job = job_new(LUFUS_JOB_LINUX_PERSISTENT, image_path, device_path, error);
    if (job != NULL) {
        job->persistence_size = persistence_size;
        job->runtime_validation = runtime_validation;
    }
    return job;
}

LufusJob *lufus_job_new_device_test(const char *device_path, guint passes,
                                    GError **error)
{
    LufusJob *job;
    if (passes < 1 || passes > 4) {
        g_set_error_literal(error, lufus_job_error_quark(), 7,
                            "Device-test passes must be between 1 and 4");
        return NULL;
    }
    job = device_job_new(LUFUS_JOB_DEVICE_TEST, device_path, error);
    if (job != NULL)
        job->test_passes = passes;
    return job;
}

gboolean lufus_job_set_pretest(LufusJob *job, guint passes, GError **error)
{
    if (job == NULL || job->type == LUFUS_JOB_CAPTURE || passes > 4) {
        g_set_error_literal(error, lufus_job_error_quark(), 10,
                            "Pre-write test passes must be between 0 and 4");
        return FALSE;
    }
    job->test_passes = passes;
    return TRUE;
}

LufusJob *lufus_job_new_capture(const char *device_path, const char *output_path,
                                LufusCaptureFormat format, GError **error)
{
    LufusJob *job;
    char *parent;
    char *canonical_parent;
    char *basename;
    if (output_path == NULL || !g_path_is_absolute(output_path) ||
        format > LUFUS_CAPTURE_VHDX) {
        g_set_error_literal(error, lufus_job_error_quark(), 8,
                            "Capture output must be an absolute path and use a supported format");
        return NULL;
    }
    parent = g_path_get_dirname(output_path);
    canonical_parent = realpath(parent, NULL);
    basename = g_path_get_basename(output_path);
    g_free(parent);
    if (canonical_parent == NULL || basename[0] == '\0' || strchr(basename, '/') != NULL) {
        g_set_error_literal(error, lufus_job_error_quark(), 9,
                            "Capture output directory does not exist");
        free(canonical_parent);
        g_free(basename);
        return NULL;
    }
    job = device_job_new(LUFUS_JOB_CAPTURE, device_path, error);
    if (job != NULL) {
        job->image_path = g_build_filename(canonical_parent, basename, NULL);
        job->capture_format = format;
    }
    free(canonical_parent);
    g_free(basename);
    return job;
}

LufusJob *lufus_job_new_windows_to_go(const char *image_path,
                                      const char *device_path,
                                      guint image_index,
                                      const LufusWindowsOptions *options,
                                      GError **error)
{
    LufusJob *job;
    g_return_val_if_fail(options != NULL, NULL);
    if (image_index == 0) {
        g_set_error_literal(error, lufus_job_error_quark(), 5,
                            "Windows image indexes start at 1");
        return NULL;
    }
    if ((options->flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
        g_set_error_literal(error, lufus_job_error_quark(), 11,
                            "Silent installation applies only to Windows installer media");
        return NULL;
    }
    if (!lufus_windows_options_validate(options, error))
        return NULL;
    job = job_new(LUFUS_JOB_WINDOWS_TO_GO, image_path, device_path, error);
    if (job != NULL) {
        job->windows_index = image_index;
        job->windows.flags = options->flags;
        job->windows.image_index = options->image_index;
        job->windows.boot_media_partition = options->boot_media_partition;
        job->windows.architecture = g_strdup(options->architecture != NULL ?
                                              options->architecture : "amd64");
        job->windows.language = g_strdup(options->language);
        job->windows.local_account = g_strdup(options->local_account);
    }
    return job;
}

void lufus_job_free(LufusJob *job)
{
    if (job == NULL)
        return;
    g_free(job->image_path);
    g_free(job->device_path);
    g_free(job->major_minor);
    lufus_windows_options_clear(&job->windows);
    g_free(job);
}

GSubprocess *lufus_job_spawn(const LufusJob *job,
                             const char *helper_path,
                             gboolean elevate,
                             GError **error)
{
    char *size;
    g_autofree char *sequence = NULL;
    const char *direct_arguments[48];
    char *windows_flags = NULL;
    char *windows_index = NULL;
    char *persistence_size = NULL;
    char *test_passes = NULL;
    guint argument = 0;
    g_autofree char *pkexec_path = NULL;
    GSubprocess *process;
    g_return_val_if_fail(job != NULL, NULL);
    g_return_val_if_fail(helper_path != NULL, NULL);

    if (job->type < LUFUS_JOB_RAW_WRITE || job->type > LUFUS_JOB_CAPTURE) {
        g_set_error_literal(error, lufus_job_error_quark(), 4,
                            "This job type does not have a backend yet");
        return NULL;
    }

    size = g_strdup_printf("%llu", (unsigned long long)job->device_size);
    if (elevate) {
        pkexec_path = g_find_program_in_path("pkexec");
        if (pkexec_path == NULL) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                "Polkit (pkexec) is required for USB operations. "
                                "Alternatively, run the command-line tool as administrator.");
            g_free(size);
            return NULL;
        }
        direct_arguments[argument++] = pkexec_path;
    }
    direct_arguments[argument++] = helper_path;
    /* Cancellation travels through a dedicated stdin pipe, even when pkexec
     * runs the helper under a different UID. It never requires kill(root_pid). */
    direct_arguments[argument++] = "--cancel-stdin";
    direct_arguments[argument++] = job->type == LUFUS_JOB_RAW_WRITE ? "write" :
                                   job->type == LUFUS_JOB_WINDOWS_INSTALLER ? "windows-installer" :
                                   job->type == LUFUS_JOB_WINDOWS_TO_GO ? "windows-to-go" :
                                   job->type == LUFUS_JOB_LINUX_PERSISTENT ? "linux-persistent" :
                                   job->type == LUFUS_JOB_DEVICE_TEST ? "device-test" : "capture";
    if (job->image_path != NULL) {
        direct_arguments[argument++] = job->type == LUFUS_JOB_CAPTURE ? "--output" : "--source";
        direct_arguments[argument++] = job->image_path;
    }
    direct_arguments[argument++] = "--target";
    direct_arguments[argument++] = job->device_path;
    direct_arguments[argument++] = "--major-minor";
    direct_arguments[argument++] = job->major_minor;
    direct_arguments[argument++] = "--size";
    direct_arguments[argument++] = size;
    if (job->disk_sequence != 0) {
        sequence = g_strdup_printf("%" G_GUINT64_FORMAT, job->disk_sequence);
        direct_arguments[argument++] = "--disk-seq";
        direct_arguments[argument++] = sequence;
    }
    if (job->type == LUFUS_JOB_RAW_WRITE && job->verify)
        direct_arguments[argument++] = "--verify";
    if (job->type == LUFUS_JOB_WINDOWS_INSTALLER && job->uefi_ntfs)
        direct_arguments[argument++] = "--uefi-ntfs";
    if (job->runtime_validation)
        direct_arguments[argument++] = "--runtime-validation";
    if (job->type == LUFUS_JOB_WINDOWS_INSTALLER) {
        direct_arguments[argument++] = "--target-system";
        direct_arguments[argument++] = job->windows_target == LUFUS_WINDOWS_TARGET_BIOS ? "bios" :
                                       job->windows_target == LUFUS_WINDOWS_TARGET_BIOS_UEFI ? "bios-uefi" : "uefi";
    }
    if (job->type == LUFUS_JOB_WINDOWS_INSTALLER || job->type == LUFUS_JOB_WINDOWS_TO_GO) {
        windows_flags = g_strdup_printf("%u", job->windows.flags);
        direct_arguments[argument++] = "--windows-flags";
        direct_arguments[argument++] = windows_flags;
        direct_arguments[argument++] = "--architecture";
        direct_arguments[argument++] = job->windows.architecture;
        windows_index = g_strdup_printf("%u", job->windows_index);
        direct_arguments[argument++] = "--windows-index";
        direct_arguments[argument++] = windows_index;
        if (job->windows.local_account != NULL) {
            direct_arguments[argument++] = "--local-account";
            direct_arguments[argument++] = job->windows.local_account;
        }
    }
    if (job->type == LUFUS_JOB_LINUX_PERSISTENT) {
        persistence_size = g_strdup_printf("%llu", (unsigned long long)job->persistence_size);
        direct_arguments[argument++] = "--persistence-size";
        direct_arguments[argument++] = persistence_size;
    }
    if (job->test_passes > 0) {
        test_passes = g_strdup_printf("%u", job->test_passes);
        direct_arguments[argument++] = "--passes";
        direct_arguments[argument++] = test_passes;
    }
    if (job->type == LUFUS_JOB_CAPTURE) {
        direct_arguments[argument++] = "--capture-format";
        direct_arguments[argument++] = job->capture_format == LUFUS_CAPTURE_RAW ? "raw" :
                                       job->capture_format == LUFUS_CAPTURE_VHD ? "vhd" : "vhdx";
    }
    direct_arguments[argument] = NULL;

    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE |
        G_SUBPROCESS_FLAGS_STDIN_PIPE);
    if (elevate) {
        /* The host authentication program must use the host's own libraries. */
        const char *private_variables[] = {"LD_LIBRARY_PATH", "LD_PRELOAD", "LD_AUDIT",
            "GIO_MODULE_DIR", "GSETTINGS_SCHEMA_DIR", "GDK_PIXBUF_MODULE_FILE",
            "GDK_PIXBUF_MODULEDIR", "GCONV_PATH", "LOCPATH", "FONTCONFIG_FILE", NULL};
        for (guint i = 0; private_variables[i] != NULL; i++)
            g_subprocess_launcher_unsetenv(launcher, private_variables[i]);
        const char *host_path = g_getenv("TUXFLASH_HOST_PATH");
        if (host_path != NULL)
            g_subprocess_launcher_setenv(launcher, "PATH", host_path, TRUE);
    }
    process = g_subprocess_launcher_spawnv(launcher, direct_arguments, error);
    g_object_unref(launcher);
    g_free(size);
    g_free(windows_flags);
    g_free(windows_index);
    g_free(persistence_size);
    g_free(test_passes);
    return process;
}
