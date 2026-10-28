/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gio/gio.h>
#include <stdint.h>

#include "windows_options.h"

typedef enum {
    LUFUS_JOB_RAW_WRITE,
    LUFUS_JOB_WINDOWS_INSTALLER,
    LUFUS_JOB_WINDOWS_TO_GO,
    LUFUS_JOB_LINUX_PERSISTENT,
    LUFUS_JOB_DEVICE_TEST,
    LUFUS_JOB_CAPTURE,
} LufusJobType;

typedef enum {
    LUFUS_WINDOWS_TARGET_UEFI,
    LUFUS_WINDOWS_TARGET_BIOS,
    LUFUS_WINDOWS_TARGET_BIOS_UEFI,
} LufusWindowsTarget;

typedef enum {
    LUFUS_CAPTURE_RAW,
    LUFUS_CAPTURE_VHD,
    LUFUS_CAPTURE_VHDX,
} LufusCaptureFormat;

typedef struct {
    LufusJobType type;
    char *image_path;
    char *device_path;
    char *major_minor;
    uint64_t device_size;
    uint64_t disk_sequence;
    gboolean verify;
    gboolean uefi_ntfs;
    gboolean runtime_validation;
    LufusWindowsTarget windows_target;
    guint windows_index;
    guint64 persistence_size;
    guint test_passes;
    LufusCaptureFormat capture_format;
    LufusWindowsOptions windows;
} LufusJob;

gboolean lufus_job_supports_runtime_validation(LufusJobType type);
gboolean lufus_job_runtime_validation_default(LufusJobType type);

LufusJob *lufus_job_new_raw(const char *image_path,
                            const char *device_path,
                            gboolean verify,
                            GError **error);
LufusJob *lufus_job_new_windows_installer(const char *image_path,
                                          const char *device_path,
                                          gboolean uefi_ntfs,
                                          LufusWindowsTarget target_system,
                                          gboolean runtime_validation,
                                          const LufusWindowsOptions *options,
                                          GError **error);
LufusJob *lufus_job_new_windows_to_go(const char *image_path,
                                      const char *device_path,
                                      guint image_index,
                                      const LufusWindowsOptions *options,
                                      GError **error);
LufusJob *lufus_job_new_linux_persistent(const char *image_path,
                                         const char *device_path,
                                         guint64 persistence_size,
                                         gboolean runtime_validation,
                                         GError **error);
LufusJob *lufus_job_new_device_test(const char *device_path,
                                    guint passes,
                                    GError **error);
LufusJob *lufus_job_new_capture(const char *device_path,
                                const char *output_path,
                                LufusCaptureFormat format,
                                GError **error);
gboolean lufus_job_set_pretest(LufusJob *job, guint passes, GError **error);
void lufus_job_free(LufusJob *job);

GSubprocess *lufus_job_spawn(const LufusJob *job,
                             const char *helper_path,
                             gboolean elevate,
                             GError **error);

gboolean lufus_job_cancel(GSubprocess *process, GError **error);
