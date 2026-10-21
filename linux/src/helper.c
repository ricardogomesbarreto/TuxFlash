/*
 * Lufus privileged image writer
 * Copyright (C) 2026 Open Research Tools contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "progress.h"
#include "device_safety.h"
#include "volume_label.h"
#include "windows_options.h"
#include <json-glib/json-glib.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fs.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef COPY_BUFFER_SIZE
#define COPY_BUFFER_SIZE (4U * 1024U * 1024U)
#endif
#ifndef COPY_COMMIT_INTERVAL
#define COPY_COMMIT_INTERVAL (32ULL * 1024ULL * 1024ULL)
#endif
#define DURABLE_TOOL_COMMIT_INTERVAL_MS 1000

static volatile sig_atomic_t cancelled;
static pid_t decompressor_pid = -1;
static unsigned int operation_progress_start;
static unsigned int operation_progress_end = 1000;

typedef struct {
    char *path;
    struct stat details;
    uint64_t capacity;
    uint64_t disk_sequence;
} TargetInfo;

static TargetInfo *active_target;
static uint64_t expected_disk_sequence;
static bool read_only_operation;
static int revalidate_active_target(void);
static int target_descriptor_matches(const TargetInfo *target, int descriptor);

static int inspect_boot_records(const char *path)
{
    unsigned char sector[512];
    int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    ssize_t count;
    if (descriptor < 0)
        return -1;
    count = pread(descriptor, sector, sizeof(sector), 0);
    close(descriptor);
    if (count < 0)
        return -1;
    if ((size_t)count != sizeof(sector)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int sync_mounted_filesystem(const char *path)
{
    int descriptor = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int result;
    if (descriptor < 0)
        return -1;
    result = syncfs(descriptor);
    close(descriptor);
    return result;
}

static int commit_descriptor(int descriptor, bool whole_filesystem)
{
    int result;

    do {
        result = whole_filesystem ? syncfs(descriptor) : fsync(descriptor);
    } while (result != 0 && errno == EINTR);
    return result;
}

static int run_command_input(char *const arguments[], const char *input);
static int run_command_progress(char *const arguments[]);
static int run_command_progress_range(char *const arguments[], double start_percent,
                                      double end_percent);
static int run_command_progress_range_durable(char *const arguments[],
                                              double start_percent,
                                              double end_percent,
                                              const char *target_path);

static char *bundled_tool_path(const char *program)
{
    char executable[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    char *libexec_directory;
    char *usr_directory;
    char *candidate;
    if (length <= 0 || (size_t)length >= sizeof(executable) - 1)
        return NULL;
    executable[length] = '\0';
    libexec_directory = g_path_get_dirname(executable);
    usr_directory = g_path_get_dirname(libexec_directory);
    g_free(libexec_directory);
    candidate = g_build_filename(usr_directory, "bin", program, NULL);
    if (access(candidate, X_OK) != 0) {
        g_free(candidate);
        candidate = g_build_filename(usr_directory, "sbin", program, NULL);
        if (access(candidate, X_OK) != 0)
            g_clear_pointer(&candidate, g_free);
    }
    g_free(usr_directory);
    return candidate;
}

static void exec_tool(char *const arguments[])
{
    char *bundled = bundled_tool_path(arguments[0]);
    if (bundled != NULL)
        execv(bundled, arguments);
    execvp(arguments[0], arguments);
}

static void configure_bundled_environment(void)
{
    char executable[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    char *libexec_directory;
    char *usr_directory;
    char *path;
    char *library_path;
    if (length <= 0 || (size_t)length >= sizeof(executable) - 1)
        return;
    executable[length] = '\0';
    libexec_directory = g_path_get_dirname(executable);
    usr_directory = g_path_get_dirname(libexec_directory);
    g_free(libexec_directory);
    path = g_strdup_printf("%s/bin:%s/sbin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                           usr_directory, usr_directory);
    library_path = g_build_filename(usr_directory, "lib", NULL);
    setenv("PATH", path, 1);
    g_autofree char *portable_marker = g_build_filename(usr_directory, ".tuxflash-portable", NULL);
    if (access(portable_marker, R_OK) == 0)
        unsetenv("LD_LIBRARY_PATH");
    else
        setenv("LD_LIBRARY_PATH", library_path, 1);
    g_free(path);
    g_free(library_path);
    g_free(usr_directory);
}

static pid_t wait_child(pid_t child, int *status)
{
    pid_t result;
    do { result = waitpid(child, status, 0); } while (result < 0 && errno == EINTR);
    return result;
}

static void handle_signal(int signal_number)
{
    int saved_errno = errno;
    (void)signal_number;
    cancelled = 1;
    if (decompressor_pid > 0)
        kill(decompressor_pid, SIGTERM);
    errno = saved_errno;
}

static int install_signal_handlers(void)
{
    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    /* signal() may enable SA_RESTART: a cancellation would then resume a
     * blocked read instead of returning EINTR to the verification loop. */
    return sigaction(SIGINT, &action, NULL) == 0 &&
           sigaction(SIGTERM, &action, NULL) == 0 ? 0 : -1;
}

static void event_message(const char *type, const char *message)
{
    const unsigned char *cursor;
    fputs(type, stdout);
    fputc(' ', stdout);
    for (cursor = (const unsigned char *)message; *cursor != '\0'; cursor++) {
        if (*cursor == '\n' || *cursor == '\r')
            fputc(' ', stdout);
        else if (*cursor >= 0x20)
            fputc(*cursor, stdout);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

/* Progress is kept in the privileged helper so the GUI and CLI consume the
 * exact same backend state.  STAGE ranges use thousandths of the complete job;
 * PROGRESS and PROGRESS_PERCENT are relative to the current stage.
 * STAGE_PERCENT asks frontends to put the stage-relative percentage in the
 * progress bar text, while STAGE_TEXT uses the supplied static text there. */
static void event_stage_type(const char *type, unsigned int local_start,
                             unsigned int local_end, const char *message)
{
    unsigned int span = operation_progress_end - operation_progress_start;
    unsigned int start = operation_progress_start +
                         (unsigned int)(((uint64_t)span * local_start) / 1000U);
    unsigned int end = operation_progress_start +
                       (unsigned int)(((uint64_t)span * local_end) / 1000U);
    const unsigned char *cursor;

    printf("%s %u %u ", type, start, end);
    for (cursor = (const unsigned char *)message; *cursor != '\0'; cursor++) {
        if (*cursor == '\n' || *cursor == '\r')
            fputc(' ', stdout);
        else if (*cursor >= 0x20)
            fputc(*cursor, stdout);
    }
    fputc('\n', stdout);
    fflush(stdout);
}

static bool tool_available(const char *name)
{
    g_autofree char *path = bundled_tool_path(name);
    if (path == NULL)
        path = g_find_program_in_path(name);
    return path != NULL;
}

static bool require_tools(const char *const *names)
{
    bool available = true;
    for (guint i = 0; names[i] != NULL; i++) {
        if (!tool_available(names[i])) {
            g_autofree char *message = g_strdup_printf("Required tool is missing: %s", names[i]);
            event_message("ERROR", message);
            available = false;
        }
    }
    return available;
}

static int check_tools(void)
{
    const char *tools[] = {"lsblk", "findmnt", "mount", "umount", "blkid", "blockdev",
        "wipefs", "sfdisk", "mkfs.vfat", "mkfs.ntfs", "mkfs.ext4", "ntfs-3g",
        "wimlib-imagex", "7z", "qemu-img", "hivexsh", "uuidgen", "bash",
        "hexdump", "xxd", "f3probe", "gzip", "xz", "bzip2", "zstd", NULL};
    bool all = true;
    fputs("{\"application\":\"TuxFlash\",\"tools\":{", stdout);
    for (guint i = 0; tools[i] != NULL; i++) {
        bool available = tool_available(tools[i]);
        all = all && available;
        fprintf(stdout, "%s\"%s\":%s", i == 0 ? "" : ",", tools[i], available ? "true" : "false");
    }
    fprintf(stdout, "},\"all_tools_available\":%s}\n", all ? "true" : "false");
    return all ? 0 : 69;
}

static void event_stage(unsigned int local_start, unsigned int local_end,
                        const char *message)
{
    event_stage_type("STAGE", local_start, local_end, message);
}

static void event_stage_percent(unsigned int local_start, unsigned int local_end,
                                const char *message)
{
    event_stage_type("STAGE_PERCENT", local_start, local_end, message);
}

static void event_stage_text(unsigned int local_start, unsigned int local_end,
                             const char *message)
{
    event_stage_type("STAGE_TEXT", local_start, local_end, message);
}

static void set_operation_progress_subrange(unsigned int parent_start,
                                            unsigned int parent_end,
                                            unsigned int local_start,
                                            unsigned int local_end)
{
    unsigned int parent_span = parent_end - parent_start;

    operation_progress_start = parent_start +
        (unsigned int)(((uint64_t)parent_span * local_start) / 1000U);
    operation_progress_end = parent_start +
        (unsigned int)(((uint64_t)parent_span * local_end) / 1000U);
}

static void event_indeterminate_progress(void)
{
    fputs("PROGRESS 0 0\n", stdout);
    fflush(stdout);
}

static void event_byte_progress(uint64_t completed, uint64_t total)
{
    printf("PROGRESS %llu %llu\n",
           (unsigned long long)completed,
           (unsigned long long)total);
    fflush(stdout);
}

static void event_percent_progress(double percent)
{
    unsigned int thousandths;
    if (percent < 0.0)
        percent = 0.0;
    if (percent > 100.0)
        percent = 100.0;
    thousandths = (unsigned int)(percent * 10.0 + 0.5);
    printf("PROGRESS_PERCENT %u\n", thousandths);
    fflush(stdout);
}

static void event_errno(const char *context)
{
    char message[512];
    snprintf(message, sizeof(message), "%s: %s", context, strerror(errno));
    event_message("ERROR", message);
}

static bool has_suffix(const char *path, const char *suffix)
{
    size_t path_length = strlen(path);
    size_t suffix_length = strlen(suffix);
    return path_length >= suffix_length &&
           strcasecmp(path + path_length - suffix_length, suffix) == 0;
}

static bool is_virtual_disk_image(const char *source)
{
    return has_suffix(source, ".vhd") || has_suffix(source, ".vhdx");
}

/* QEMU JSON includes child nodes before the root virtual-size. Reading the
 * first matching text field returns the container length, not disk capacity. */
static uint64_t virtual_disk_size_from_json(const char *output)
{
    JsonParser *parser = json_parser_new();
    uint64_t result = 0;
    if (output != NULL && json_parser_load_from_data(parser, output, -1, NULL)) {
        JsonNode *root = json_parser_get_root(parser);
        if (root != NULL && JSON_NODE_HOLDS_OBJECT(root)) {
            JsonObject *object = json_node_get_object(root);
            JsonNode *size = json_object_get_member(object, "virtual-size");
            if (size != NULL && JSON_NODE_HOLDS_VALUE(size) &&
                json_node_get_value_type(size) == G_TYPE_INT64 &&
                json_node_get_int(size) > 0)
                result = (uint64_t)json_node_get_int(size);
        }
    }
    g_object_unref(parser);
    return result;
}

static const char *virtual_disk_format(const char *source)
{
    return has_suffix(source, ".vhd") ? "vpc" : "vhdx";
}

static uint64_t virtual_disk_size(const char *source)
{
    GError *error = NULL;
    GSubprocess *process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                            G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                            &error, "qemu-img", "info", "--output=json",
                                            "--force-share", "-f", virtual_disk_format(source),
                                            source, NULL);
    char *output = NULL;
    uint64_t result = 0;
    if (process != NULL && g_subprocess_communicate_utf8(process, NULL, NULL,
                                                          &output, NULL, &error) &&
        g_subprocess_get_successful(process))
        result = virtual_disk_size_from_json(output);
    if (error != NULL) {
        event_message("LOG", error->message);
        g_clear_error(&error);
    }
    g_free(output);
    g_clear_object(&process);
    return result;
}

/* fsync commits writes, but buffered reads can still hit the old page cache.
 * Invalidate the block-device cache before reporting media verification. */
static int clear_verification_cache(int descriptor)
{
    struct stat details;
    if (fstat(descriptor, &details) != 0)
        return -1;
    if (!S_ISBLK(details.st_mode)) {
        errno = EINVAL;
        return -1;
    }
    return ioctl(descriptor, BLKFLSBUF, 0);
}

static char *qemu_option_escape(const char *value)
{
    GString *escaped = g_string_new(NULL);
    const char *cursor;
    for (cursor = value; *cursor != '\0'; cursor++) {
        if (*cursor == ',')
            g_string_append_c(escaped, ',');
        g_string_append_c(escaped, *cursor);
    }
    return g_string_free(escaped, FALSE);
}

static int write_virtual_disk(const char *source, TargetInfo *target, bool verify)
{
    uint64_t size = virtual_disk_size(source);
    char *convert[] = {"qemu-img", "convert", "-p", "-n", "-f",
                       (char *)virtual_disk_format(source), "-O", "raw",
                       (char *)source, target->path, NULL};
    char *escaped_source = qemu_option_escape(source);
    char *escaped_target = qemu_option_escape(target->path);
    char *source_options = g_strdup_printf("driver=%s,file.driver=file,file.filename=%s",
                                            virtual_disk_format(source),
                                            escaped_source);
    char *target_options = g_strdup_printf("driver=raw,size=%llu,file.driver=host_device,file.filename=%s",
                                            (unsigned long long)size, escaped_target);
    char *compare[] = {"qemu-img", "compare", "-p", "--image-opts",
                       source_options, target_options, NULL};
    int descriptor;
    if (size == 0) {
        event_message("ERROR", "Could not inspect the VHD/VHDX virtual size");
        goto fail;
    }
    if (size > target->capacity) {
        event_message("ERROR", "The virtual disk is larger than the selected device");
        goto fail;
    }
    event_message("STARTED", "Writing virtual disk image");
    event_stage(0, verify ? 850 : 1000, "Writing virtual disk image");
    if (run_command_progress(convert) != 0) {
        event_message("ERROR", cancelled ? "Write cancelled; target is incomplete" :
                                       "VHD/VHDX conversion failed");
        goto fail;
    }
    if (verify) {
        event_message("VERIFYING", "Comparing virtual disk with target");
        event_stage(850, 1000, "Verifying virtual disk image");
        descriptor = open(target->path, O_RDONLY | O_CLOEXEC | O_EXCL);
        if (descriptor < 0 || clear_verification_cache(descriptor) != 0) {
            event_errno("Could not invalidate the device cache for verification");
            if (descriptor >= 0)
                close(descriptor);
            goto fail;
        }
        close(descriptor);
        event_message("LOG", "Device cache invalidated before verification");
        if (run_command_progress(compare) != 0) {
            event_message("ERROR", cancelled ? "Verification cancelled" :
                                           "Virtual disk verification failed");
            goto fail;
        }
        event_message("VERIFIED", "Virtual disk verification succeeded");
    }
    descriptor = open(target->path, O_RDONLY | O_CLOEXEC | O_EXCL);
    if (descriptor >= 0) {
        ioctl(descriptor, BLKRRPART, 0);
        close(descriptor);
    }
    printf("COMPLETE %llu\n", (unsigned long long)size);
    fflush(stdout);
    g_free(escaped_source);
    g_free(escaped_target);
    g_free(source_options);
    g_free(target_options);
    return 0;
fail:
    g_free(escaped_source);
    g_free(escaped_target);
    g_free(source_options);
    g_free(target_options);
    return 1;
}

static char *canonical_sysfs_path(const char *path)
{
    char *resolved = realpath(path, NULL);
    if (resolved == NULL)
        return NULL;
    return resolved;
}

static bool path_is_below(const char *candidate, const char *parent)
{
    size_t length = strlen(parent);
    return strcmp(candidate, parent) == 0 ||
           (strncmp(candidate, parent, length) == 0 && candidate[length] == '/');
}

static bool mountpoint_is_protected(const char *mountpoint)
{
    return tuxflash_mount_is_protected(mountpoint);
}

static void decode_lsblk_path(char *value)
{
    char *read_cursor = value;
    char *write_cursor = value;
    while (*read_cursor != '\0') {
        if (read_cursor[0] == '\\' && read_cursor[1] == 'x' &&
            read_cursor[2] != '\0' && read_cursor[3] != '\0') {
            char high = read_cursor[2];
            char low = read_cursor[3];
            int high_value = high >= '0' && high <= '9' ? high - '0' :
                             high >= 'a' && high <= 'f' ? high - 'a' + 10 :
                             high >= 'A' && high <= 'F' ? high - 'A' + 10 : -1;
            int low_value = low >= '0' && low <= '9' ? low - '0' :
                            low >= 'a' && low <= 'f' ? low - 'a' + 10 :
                            low >= 'A' && low <= 'F' ? low - 'A' + 10 : -1;
            if (high_value >= 0 && low_value >= 0) {
                *write_cursor++ = (char)((high_value << 4) | low_value);
                read_cursor += 4;
                continue;
            }
        }
        *write_cursor++ = *read_cursor++;
    }
    *write_cursor = '\0';
}

static bool lsblk_reports_protected_mount(const char *target, char *reason, size_t reason_size)
{
    int pipe_fds[2];
    pid_t child;
    FILE *output;
    char line[8192];
    int status = 0;
    bool protected = false;
    if (pipe(pipe_fds) != 0) {
        snprintf(reason, reason_size, "Could not inspect target mount tree");
        return true;
    }
    child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        snprintf(reason, reason_size, "Could not inspect target mount tree");
        return true;
    }
    if (child == 0) {
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        {
            char *arguments[] = {"lsblk", "-nrpo", "MOUNTPOINTS", "--", (char *)target, NULL};
            exec_tool(arguments);
        }
        _exit(127);
    }
    close(pipe_fds[1]);
    output = fdopen(pipe_fds[0], "r");
    if (output == NULL) {
        close(pipe_fds[0]);
        wait_child(child, NULL);
        snprintf(reason, reason_size, "Could not inspect target mount tree");
        return true;
    }
    while (fgets(line, sizeof(line), output) != NULL) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        decode_lsblk_path(line);
        if (mountpoint_is_protected(line)) {
            snprintf(reason, reason_size, "Target contains protected mount %.400s", line);
            protected = true;
            break;
        }
    }
    fclose(output);
    if (wait_child(child, &status) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        snprintf(reason, reason_size, "Could not inspect target mount tree");
        return true;
    }
    return protected;
}

static bool sysfs_uses_target(const char *node, const char *target, unsigned int depth)
{
    if (path_is_below(node, target)) return true;
    if (depth >= 64) return true; /* Invalid topology: refuse rather than guess. */
    g_autofree char *slaves_path = g_build_filename(node, "slaves", NULL);
    DIR *slaves = opendir(slaves_path);
    if (slaves == NULL) return errno != ENOENT;
    struct dirent *entry;
    bool uses = false;
    while ((entry = readdir(slaves)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        g_autofree char *link = g_build_filename(slaves_path, entry->d_name, NULL);
        g_autofree char *dependency = realpath(link, NULL);
        if (!dependency || sysfs_uses_target(dependency, target, depth + 1)) { uses = true; break; }
    }
    closedir(slaves);
    return uses;
}

static bool target_contains_active_swap(const char *target_sysfs)
{
    FILE *swaps = fopen("/proc/swaps", "re");
    if (!swaps) return true;
    char line[8192], name[4096];
    bool unsafe = false;
    (void)fgets(line, sizeof(line), swaps); /* header */
    while (fgets(line, sizeof(line), swaps)) {
        struct stat details;
        if (sscanf(line, "%4095s", name) != 1) { unsafe = true; break; }
        g_autofree char *decoded = g_strcompress(name);
        if (stat(decoded, &details) != 0) { unsafe = true; break; }
        dev_t device = S_ISBLK(details.st_mode) ? details.st_rdev : details.st_dev;
        g_autofree char *link = g_strdup_printf("/sys/dev/block/%u:%u", major(device), minor(device));
        g_autofree char *node = realpath(link, NULL);
        if (!node || sysfs_uses_target(node, target_sysfs, 0)) { unsafe = true; break; }
    }
    if (ferror(swaps)) unsafe = true;
    fclose(swaps);
    return unsafe;
}

static bool target_contains_system_mount(dev_t target_device, char *reason, size_t reason_size)
{
    char target_link[128];
    char device_link[128];
    char line[8192];
    char mountpoint[4096];
    unsigned int device_major;
    unsigned int device_minor;
    char *target_sysfs;
    FILE *mounts;
    bool unsafe = false;

    snprintf(target_link, sizeof(target_link), "/sys/dev/block/%u:%u",
             major(target_device), minor(target_device));
    target_sysfs = canonical_sysfs_path(target_link);
    if (target_sysfs == NULL) {
        snprintf(reason, reason_size, "Could not resolve target device in sysfs");
        return true;
    }

    mounts = fopen("/proc/self/mountinfo", "re");
    if (mounts == NULL) {
        free(target_sysfs);
        snprintf(reason, reason_size, "Could not inspect mounted filesystems");
        return true;
    }

    while (fgets(line, sizeof(line), mounts) != NULL) {
        char *cursor = line;
        int fields = sscanf(cursor, "%*u %*u %u:%u %*s %4095s",
                            &device_major, &device_minor, mountpoint);
        char *mounted_sysfs;
        if (fields != 3 || !mountpoint_is_protected(mountpoint))
            continue;
        snprintf(device_link, sizeof(device_link), "/sys/dev/block/%u:%u",
                 device_major, device_minor);
        mounted_sysfs = canonical_sysfs_path(device_link);
        if (mounted_sysfs != NULL && sysfs_uses_target(mounted_sysfs, target_sysfs, 0)) {
            snprintf(reason, reason_size, "Target contains protected mount %.400s", mountpoint);
            unsafe = true;
            free(mounted_sysfs);
            break;
        }
        free(mounted_sysfs);
    }

    if (ferror(mounts) || (!unsafe && target_contains_active_swap(target_sysfs))) {
        snprintf(reason, reason_size, "Target contains active swap or mount inspection failed");
        unsafe = true;
    }
    fclose(mounts);
    free(target_sysfs);
    return unsafe;
}

static bool is_whole_disk(const char *target)
{
    const char *name = strrchr(target, '/');
    char partition_marker[512];
    if (name == NULL || name[1] == '\0')
        return false;
    snprintf(partition_marker, sizeof(partition_marker),
             "/sys/class/block/%s/partition", name + 1);
    return access(partition_marker, F_OK) != 0;
}

static int unmount_children(const char *target)
{
    int pipe_fds[2];
    pid_t child;
    FILE *output;
    char line[8192];
    int status = 0;

    if (pipe(pipe_fds) != 0)
        return -1;
    child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return -1;
    }
    if (child == 0) {
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        {
            char *arguments[] = {"lsblk", "-nrpo", "PATH,MOUNTPOINTS", "--", (char *)target, NULL};
            exec_tool(arguments);
        }
        _exit(127);
    }
    close(pipe_fds[1]);
    output = fdopen(pipe_fds[0], "r");
    if (output == NULL) {
        close(pipe_fds[0]);
        wait_child(child, NULL);
        return -1;
    }
    while (fgets(line, sizeof(line), output) != NULL) {
        char device[4096];
        char mountpoint[4096];
        pid_t unmount_child;
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\n')
            line[length - 1] = '\0';
        if (sscanf(line, "%4095s %4095s", device, mountpoint) != 2)
            continue;
        event_message("LOG", "Unmounting target filesystem");
        unmount_child = fork();
        if (unmount_child == 0) {
            {
                char *arguments[] = {"umount", "--", device, NULL};
                exec_tool(arguments);
            }
            _exit(127);
        }
        if (unmount_child < 0 || wait_child(unmount_child, &status) < 0 ||
            !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fclose(output);
            wait_child(child, NULL);
            errno = EBUSY;
            return -1;
        }
    }
    fclose(output);
    if (wait_child(child, &status) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;
    return 0;
}

static int mount_partition_once(const char *partition, const char *mountpoint,
                                 bool ntfs)
{
    char *automatic[] = {"mount", "--", (char *)partition, (char *)mountpoint, NULL};
    char *kernel_ntfs[] = {"mount", "-t", "ntfs3", "--", (char *)partition,
                           (char *)mountpoint, NULL};
    char *fuse_ntfs[] = {"ntfs-3g", (char *)partition, (char *)mountpoint, NULL};
    if (!ntfs)
        return run_command_input(automatic, NULL);
    if (run_command_input(kernel_ntfs, NULL) == 0)
        return 0;
    event_message("LOG", "Trying the NTFS-3G compatibility driver");
    return run_command_input(fuse_ntfs, NULL);
}

static int mount_target_partition(const char *partition, const char *mountpoint,
                                  const char *target, bool ntfs)
{
    char *unmount_arguments[] = {"umount", "--", (char *)partition, NULL};
    (void)target;
    if (mount_partition_once(partition, mountpoint, ntfs) == 0)
        return 0;
    event_message("LOG", "Unmounting a possible desktop automount before retrying");
    if (run_command_input(unmount_arguments, NULL) != 0)
        return -1;
    usleep(250000);
    return mount_partition_once(partition, mountpoint, ntfs);
}

static int open_decompressor(const char *program, const char *source)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0)
        return -1;
    decompressor_pid = fork();
    if (decompressor_pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return -1;
    }
    if (decompressor_pid == 0) {
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        {
            char *arguments[] = {(char *)program, "-dc", "--", (char *)source, NULL};
            exec_tool(arguments);
        }
        _exit(127);
    }
    close(pipe_fds[1]);
    return pipe_fds[0];
}

static int open_source(const char *source, uint64_t *total)
{
    struct stat source_stat;
    if (stat(source, &source_stat) != 0 || !S_ISREG(source_stat.st_mode)) {
        errno = EINVAL;
        return -1;
    }
    *total = (uint64_t)source_stat.st_size;
    if (has_suffix(source, ".gz")) {
        *total = 0;
        return open_decompressor("gzip", source);
    }
    if (has_suffix(source, ".xz")) {
        *total = 0;
        return open_decompressor("xz", source);
    }
    if (has_suffix(source, ".bz2")) {
        *total = 0;
        return open_decompressor("bzip2", source);
    }
    if (has_suffix(source, ".zst") || has_suffix(source, ".zstd")) {
        *total = 0;
        return open_decompressor("zstd", source);
    }
    return open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
}

static int wait_for_decompressor(void)
{
    int status;
    pid_t pid = decompressor_pid;
    if (pid <= 0)
        return 0;
    pid_t waited = wait_child(pid, &status);
    decompressor_pid = -1;
    if (waited < 0)
        return -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int write_all(int target_fd, const unsigned char *buffer, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t result = write(target_fd, buffer + offset, length - offset);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return -1;
        offset += (size_t)result;
    }
    return 0;
}

static int copy_image(int source_fd, int target_fd, uint64_t total, uint64_t capacity,
                      uint64_t *written_out)
{
    unsigned char *buffer = malloc(COPY_BUFFER_SIZE);
    uint64_t written = 0;
    uint64_t committed = 0;
    if (buffer == NULL)
        return -1;
    while (!cancelled) {
        ssize_t count = read(source_fd, buffer, COPY_BUFFER_SIZE);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            free(buffer);
            return -1;
        }
        if (count == 0)
            break;
        /* A source whose size was known must not grow during the transfer.
         * Subtract from the limits to avoid overflowing the byte counters. */
        if (total > 0 && (written > total || (uint64_t)count > total - written)) {
            free(buffer);
            errno = EIO;
            return -1;
        }
        if (written > capacity || (uint64_t)count > capacity - written) {
            free(buffer);
            errno = ENOSPC;
            return -1;
        }
        if (write_all(target_fd, buffer, (size_t)count) != 0) {
            free(buffer);
            return -1;
        }
        written += (uint64_t)count;
        if (written - committed >= COPY_COMMIT_INTERVAL) {
            if (commit_descriptor(target_fd, false) != 0) {
                free(buffer);
                return -1;
            }
            committed = written;
            printf("PROGRESS %llu %llu\n",
                   (unsigned long long)committed, (unsigned long long)total);
            fflush(stdout);
        }
    }
    free(buffer);
    if (cancelled) {
        errno = ECANCELED;
        return -1;
    }
    if (total > 0 && written != total) {
        errno = EIO;
        return -1;
    }
    if (written == 0) {
        errno = ENODATA;
        return -1;
    }
    if (committed != written) {
        if (commit_descriptor(target_fd, false) != 0)
            return -1;
        committed = written;
        printf("PROGRESS %llu %llu\n",
               (unsigned long long)committed, (unsigned long long)total);
        fflush(stdout);
    }
    *written_out = written;
    return 0;
}

/* Compare only the image payload, and require the source to end exactly at
 * its committed length. Target devices may legitimately have trailing space. */
static int verify_stream(int source_fd, int target_fd, uint64_t expected_bytes,
                         unsigned char *source_buffer, unsigned char *target_buffer)
{
    uint64_t checked = 0;
    uint64_t reported = 0;
    gint64 reported_at = g_get_monotonic_time();
    if (expected_bytes == 0) {
        errno = ENODATA;
        return -1;
    }
    while (!cancelled && checked < expected_bytes) {
        size_t wanted = (size_t)((expected_bytes - checked) < COPY_BUFFER_SIZE ?
                                 (expected_bytes - checked) : COPY_BUFFER_SIZE);
        ssize_t source_count;
        size_t target_offset = 0;
        do {
            source_count = read(source_fd, source_buffer, wanted);
        } while (source_count < 0 && errno == EINTR && !cancelled);
        if (source_count <= 0) {
            errno = cancelled ? ECANCELED : EIO;
            return -1;
        }
        while (target_offset < (size_t)source_count) {
            ssize_t target_count = read(target_fd, target_buffer + target_offset,
                                        (size_t)source_count - target_offset);
            if (target_count < 0 && errno == EINTR && !cancelled)
                continue;
            if (target_count <= 0) {
                errno = cancelled ? ECANCELED : EIO;
                return -1;
            }
            target_offset += (size_t)target_count;
        }
        if (memcmp(source_buffer, target_buffer, (size_t)source_count) != 0) {
            errno = EILSEQ;
            return -1;
        }
        checked += (uint64_t)source_count;
        gint64 now = g_get_monotonic_time();
        /* Pipe reads can be just a few KiB. Bound protocol traffic instead of
         * flushing one progress event per pipe fragment (notably with bzip2). */
        if (checked < expected_bytes &&
            (checked - reported >= COPY_COMMIT_INTERVAL ||
             now - reported_at >= 500000)) {
            printf("VERIFY_PROGRESS %llu %llu\n",
                   (unsigned long long)checked, (unsigned long long)expected_bytes);
            fflush(stdout);
            reported = checked;
            reported_at = now;
        }
    }
    if (cancelled) {
        errno = ECANCELED;
        return -1;
    }
    /* Read through EOF before waiting for a decoder. Otherwise a source
     * that grew can block its decoder on a full pipe, or a raw prefix can be
     * incorrectly reported as a complete match. Do not compare target padding. */
    unsigned char extra;
    ssize_t tail;
    do {
        tail = read(source_fd, &extra, 1);
    } while (tail < 0 && errno == EINTR && !cancelled);
    if (cancelled || tail != 0) {
        errno = cancelled ? ECANCELED : EIO;
        return -1;
    }
    return 0;
}

static int verify_image(const char *source, const char *target, uint64_t expected_bytes)
{
    unsigned char *source_buffer = NULL;
    unsigned char *target_buffer = NULL;
    uint64_t source_total = 0;
    int source_fd = -1;
    int target_fd = -1;
    int result = -1;
    int saved_errno;

    source_fd = open_source(source, &source_total);
    if (source_fd < 0)
        goto out;
    if (revalidate_active_target() != 0) goto out;
    target_fd = open(target, O_RDONLY | O_CLOEXEC | O_EXCL | O_NOFOLLOW);
    if (target_fd < 0)
        goto out;
    if (active_target && target_descriptor_matches(active_target, target_fd) != 0) goto out;
    if (clear_verification_cache(target_fd) != 0)
        goto out;
    event_message("LOG", "Device cache invalidated before verification");
    source_buffer = malloc(COPY_BUFFER_SIZE);
    target_buffer = malloc(COPY_BUFFER_SIZE);
    if (source_buffer == NULL || target_buffer == NULL)
        goto out;

    if (verify_stream(source_fd, target_fd, expected_bytes,
                      source_buffer, target_buffer) != 0)
        goto out;
    if (wait_for_decompressor() != 0) {
        errno = cancelled ? ECANCELED : EIO;
        goto out;
    }
    if (cancelled) {
        errno = ECANCELED;
        goto out;
    }
    printf("VERIFY_PROGRESS %llu %llu\n",
           (unsigned long long)expected_bytes, (unsigned long long)expected_bytes);
    fflush(stdout);
    result = 0;

out:
    saved_errno = errno;
    if (source_fd >= 0)
        close(source_fd);
    if (target_fd >= 0)
        close(target_fd);
    free(source_buffer);
    free(target_buffer);
    if (result != 0 && decompressor_pid > 0) {
        kill(decompressor_pid, SIGTERM);
        wait_for_decompressor();
    }
    errno = saved_errno;
    return result;
}

static void target_info_clear(TargetInfo *target)
{
    if (active_target == target) active_target = NULL;
    free(target->path);
    memset(target, 0, sizeof(*target));
}

static uint64_t descriptor_disk_sequence(int descriptor, dev_t device)
{
    uint64_t sequence = 0;
#ifdef BLKGETDISKSEQ
    if (ioctl(descriptor, BLKGETDISKSEQ, &sequence) == 0) return sequence;
#endif
    return tuxflash_disk_sequence(device);
}

static int target_descriptor_matches(const TargetInfo *target, int descriptor)
{
    struct stat details;
    uint64_t capacity = 0;
    int read_only = 1;
    if (fstat(descriptor, &details) != 0 || !S_ISBLK(details.st_mode) ||
        details.st_rdev != target->details.st_rdev ||
        ioctl(descriptor, BLKGETSIZE64, &capacity) != 0 || capacity != target->capacity ||
        ioctl(descriptor, BLKROGET, &read_only) != 0 || (read_only && !read_only_operation) ||
        (target->disk_sequence && descriptor_disk_sequence(descriptor, details.st_rdev) != target->disk_sequence)) {
        errno = ENODEV;
        event_message("ERROR", "The selected device changed or became read-only");
        return -1;
    }
    return 0;
}

static int revalidate_active_target(void)
{
    if (!active_target) return 0;
    int descriptor = open(active_target->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    int result = descriptor < 0 ? -1 : target_descriptor_matches(active_target, descriptor);
    if (descriptor >= 0) close(descriptor);
    if (result != 0) {
        event_message("ERROR", "The selected device changed after confirmation");
        return -1;
    }
    char reason[512];
    if (lsblk_reports_protected_mount(active_target->path, reason, sizeof(reason)) ||
        target_contains_system_mount(active_target->details.st_rdev, reason, sizeof(reason))) {
        event_message("ERROR", reason);
        errno = EBUSY;
        return -1;
    }
    return 0;
}

static int validate_target(const char *target_argument,
                           const char *expected_identity,
                           uint64_t expected_capacity,
                           TargetInfo *target)
{
    char actual_identity[64];
    char unsafe_reason[512];
    int descriptor = -1;

    memset(target, 0, sizeof(*target));
    if (strncmp(target_argument, "/dev/", 5) != 0) {
        event_message("ERROR", "Target must be a device below /dev");
        return -1;
    }
    target->path = realpath(target_argument, NULL);
    if (target->path == NULL || strncmp(target->path, "/dev/", 5) != 0) {
        event_message("ERROR", "Could not resolve the target device");
        goto fail;
    }
    if (stat(target->path, &target->details) != 0 ||
        !S_ISBLK(target->details.st_mode) || !is_whole_disk(target->path)) {
        event_message("ERROR", "Only a whole block device may be written");
        goto fail;
    }
    snprintf(actual_identity, sizeof(actual_identity), "%u:%u",
             major(target->details.st_rdev), minor(target->details.st_rdev));
    if (strcmp(actual_identity, expected_identity) != 0) {
        event_message("ERROR", "The selected device changed after confirmation");
        goto fail;
    }
    if (lsblk_reports_protected_mount(target->path, unsafe_reason, sizeof(unsafe_reason)) ||
        target_contains_system_mount(target->details.st_rdev, unsafe_reason, sizeof(unsafe_reason))) {
        event_message("ERROR", unsafe_reason);
        goto fail;
    }
    /* Existing removable-media partitions can still be mounted here.  Inspect
     * capacity first, then unmount them before the destructive command opens
     * the whole disk exclusively. */
    descriptor = open(target->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0 || ioctl(descriptor, BLKGETSIZE64, &target->capacity) != 0) {
        event_errno("Could not inspect target capacity");
        goto fail;
    }
    target->disk_sequence = descriptor_disk_sequence(descriptor, target->details.st_rdev);
    if (target->capacity != expected_capacity) {
        event_message("ERROR", "The selected device capacity changed after confirmation");
        goto fail;
    }
    if (expected_disk_sequence && target->disk_sequence != expected_disk_sequence) {
        event_message("ERROR", "The selected device changed after confirmation");
        goto fail;
    }
    if (target_descriptor_matches(target, descriptor) != 0) goto fail;
    close(descriptor);
    descriptor = -1;
    active_target = target;
    return 0;

fail:
    if (descriptor >= 0)
        close(descriptor);
    target_info_clear(target);
    return -1;
}

static int verify_command(const char *source, const char *target_argument,
                          const char *expected_identity, uint64_t expected_capacity)
{
    TargetInfo target = {0};
    struct stat details;
    int result = 1;
    /* Standalone verification never invokes a decompressor or a write job. */
    if (is_virtual_disk_image(source) || has_suffix(source, ".gz") ||
        has_suffix(source, ".xz") || has_suffix(source, ".bz2") ||
        has_suffix(source, ".zst") || has_suffix(source, ".zstd") ||
        has_suffix(source, ".ffu") ||
        stat(source, &details) != 0 || !S_ISREG(details.st_mode) || details.st_size <= 0) {
        event_message("ERROR", "Standalone verification requires a nonempty uncompressed raw image or ISO");
        return 1;
    }
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if ((uint64_t)details.st_size > target.capacity) {
        event_message("ERROR", "The image is larger than the selected device");
        goto out;
    }
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount the device for verification");
        goto out;
    }
    event_message("STARTED", "Verifying existing device image without rewriting it");
    event_stage(0, 1000, "Verifying existing device image");
    if (verify_image(source, target.path, (uint64_t)details.st_size) != 0) {
        event_errno(cancelled ? "Verification cancelled" : "Image verification failed");
        goto out;
    }
    event_message("VERIFIED", "Existing device image verification succeeded");
    event_message("COMPLETE", "Existing device image verified");
    result = 0;
out:
    target_info_clear(&target);
    return result;
}

static int write_command(const char *source, const char *target_argument,
                         const char *expected_identity, uint64_t expected_capacity,
                         bool verify)
{
    TargetInfo target = {0};
    uint64_t source_total;
    uint64_t written = 0;
    int source_fd = -1;
    int target_fd = -1;
    int result = 1;

    if (has_suffix(source, ".ffu")) {
        event_message("ERROR", "FFU apply requires Microsoft's Windows/WinPE FFU provider; refusing unsafe raw container copy");
        return 1;
    }
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount target filesystems");
        goto out;
    }
    if (is_virtual_disk_image(source)) {
        result = write_virtual_disk(source, &target, verify);
        goto out;
    }
    source_fd = open_source(source, &source_total);
    if (source_fd < 0) {
        event_errno("Could not open source image");
        goto out;
    }
    if (source_total > target.capacity) {
        event_message("ERROR", "The image is larger than the selected device");
        goto out;
    }
    if (cancelled || revalidate_active_target() != 0) goto out;
    target_fd = open(target.path, O_WRONLY | O_CLOEXEC | O_EXCL | O_NOFOLLOW);
    if (target_fd < 0) {
        event_errno("Could not open target for writing");
        goto out;
    }
    if (target_descriptor_matches(&target, target_fd) != 0) goto out;
    event_message("STARTED", "Writing image");
    event_stage(0, verify ? 450 : 930, "Writing image data");
    if (copy_image(source_fd, target_fd, source_total, target.capacity, &written) != 0) {
        event_errno(cancelled ? "Write cancelled; target is incomplete" : "Image write failed");
        goto out;
    }
    if (wait_for_decompressor() != 0) {
        event_message("ERROR", "Image decompression failed");
        goto out;
    }
    event_stage(verify ? 450 : 930, verify ? 500 : 1000,
                "Flushing data to the device");
    event_indeterminate_progress();
    if (fsync(target_fd) != 0) {
        event_errno("Could not flush image data to the device");
        goto out;
    }
    close(source_fd);
    source_fd = -1;
    close(target_fd);
    target_fd = -1;
    if (verify) {
        event_message("VERIFYING", "Reading image back from target");
        event_stage(500, 1000, "Verifying image data");
        if (verify_image(source, target.path, written) != 0) {
            event_errno(cancelled ? "Verification cancelled" : "Image verification failed");
            goto out;
        }
        event_message("VERIFIED", "Image verification succeeded");
    }
    printf("COMPLETE %llu\n", (unsigned long long)written);
    fflush(stdout);
    target_fd = open(target.path, O_RDONLY | O_CLOEXEC | O_EXCL);
    if (target_fd >= 0)
        ioctl(target_fd, BLKRRPART, 0);
    result = 0;

out:
    if (source_fd >= 0)
        close(source_fd);
    if (target_fd >= 0)
        close(target_fd);
    if (result != 0 && decompressor_pid > 0) {
        kill(decompressor_pid, SIGTERM);
        wait_for_decompressor();
    }
    target_info_clear(&target);
    return result;
}

static int run_command_input(char *const arguments[], const char *input)
{
    int pipe_fds[2] = {-1, -1};
    pid_t child;
    int status;
    if (strcmp(arguments[0], "umount") != 0 &&
        (cancelled || revalidate_active_target() != 0)) { errno = cancelled ? ECANCELED : errno; return -1; }
    if (input != NULL && pipe(pipe_fds) != 0)
        return -1;
    child = fork();
    if (child < 0) {
        if (pipe_fds[0] >= 0) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }
        return -1;
    }
    if (child == 0) {
        if (input != NULL) {
            dup2(pipe_fds[0], STDIN_FILENO);
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }
        exec_tool(arguments);
        _exit(127);
    }
    decompressor_pid = child;
    if (input != NULL) {
        size_t offset = 0;
        size_t length = strlen(input);
        close(pipe_fds[0]);
        while (offset < length) {
            ssize_t count = write(pipe_fds[1], input + offset, length - offset);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                break;
            offset += (size_t)count;
        }
        close(pipe_fds[1]);
    }
    if (wait_child(child, &status) < 0) {
        decompressor_pid = -1;
        return -1;
    }
    decompressor_pid = -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static char *windows_image_default_language(const char *image_path,
                                            guint image_index,
                                            GError **error)
{
    char *index = g_strdup_printf("%u", image_index);
    const char *arguments[] = {"wimlib-imagex", "info", image_path, index, NULL};
    GSubprocess *process;
    char *standard_output = NULL;
    char *standard_error = NULL;
    char *language = NULL;
    char *cursor;
    char *end;

    process = g_subprocess_newv(arguments,
                                G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                error);
    g_free(index);
    if (process == NULL)
        return NULL;
    if (!g_subprocess_communicate_utf8(process, NULL, NULL,
                                       &standard_output, &standard_error,
                                       error))
        goto out;
    if (!g_subprocess_get_successful(process)) {
        char *detail = standard_error != NULL ? g_strstrip(standard_error) : NULL;
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "%s", detail != NULL && detail[0] != '\0' ? detail :
                    "The selected Windows image index does not exist");
        goto out;
    }
    cursor = standard_output != NULL ? strstr(standard_output, "Default Language:") : NULL;
    if (cursor == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Could not determine the selected Windows image language");
        goto out;
    }
    cursor += strlen("Default Language:");
    while (g_ascii_isspace(*cursor) && *cursor != '\r' && *cursor != '\n')
        cursor++;
    end = strpbrk(cursor, "\r\n");
    language = end != NULL ? g_strndup(cursor, (gsize)(end - cursor)) : g_strdup(cursor);
    g_strstrip(language);
    if (language[0] == '\0') {
        g_clear_pointer(&language, g_free);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "The selected Windows image has no default language");
    }

out:
    g_free(standard_output);
    g_free(standard_error);
    g_object_unref(process);
    return language;
}

static bool parse_tool_percent(const char *line, double *percent)
{
    const char *cursor;
    char *end;
    double value;

    /* qemu-img -p uses "(12.34/100%)".  The number immediately before '%'
     * would otherwise be the constant 100, so recognize its first field. */
    for (cursor = line; (cursor = strchr(cursor, '(')) != NULL; cursor++) {
        errno = 0;
        value = strtod(cursor + 1, &end);
        if (errno == 0 && end != cursor + 1 && *end == '/' &&
            value >= 0.0 && value <= 100.0) {
            *percent = value;
            return true;
        }
    }

    /* wimlib and similar tools use forms such as "[ 42%]". */
    for (cursor = line; (cursor = strchr(cursor, '%')) != NULL; cursor++) {
        const char *start = cursor;
        while (start > line && (g_ascii_isdigit(start[-1]) || start[-1] == '.'))
            start--;
        if (start == cursor)
            continue;
        errno = 0;
        value = strtod(start, &end);
        if (errno == 0 && end == cursor && value >= 0.0 && value <= 100.0) {
            *percent = value;
            return true;
        }
    }
    return false;
}

typedef struct {
    int descriptor;
    bool whole_filesystem;
    double last_committed_percent;
} DurableProgressTarget;

static int durable_progress_target_open(DurableProgressTarget *target,
                                        const char *path)
{
    struct stat details;
    int flags;

    target->descriptor = -1;
    target->whole_filesystem = false;
    target->last_committed_percent = -1.0;
    if (stat(path, &details) != 0)
        return -1;
    target->whole_filesystem = S_ISDIR(details.st_mode);
    flags = O_CLOEXEC | O_NOFOLLOW |
            (target->whole_filesystem ? O_RDONLY | O_DIRECTORY : O_RDWR);
    target->descriptor = open(path, flags);
    return target->descriptor >= 0 ? 0 : -1;
}

static int process_tool_progress_line(GString *line, double start_percent,
                                      double end_percent,
                                      DurableProgressTarget *durable_target)
{
    double percent;
    g_strstrip(line->str);
    if (line->str[0] == '\0')
        return 0;
    if (parse_tool_percent(line->str, &percent)) {
        /* A tool may print 100% before it has closed its output files.  Keep
         * the last fraction pending; the runner commits it after the process
         * exits, then publishes the end of the range. */
        if (durable_target != NULL && percent >= 100.0)
            return 0;
        if (durable_target != NULL && percent > 0.0 &&
            percent > durable_target->last_committed_percent) {
            if (commit_descriptor(durable_target->descriptor,
                                  durable_target->whole_filesystem) != 0)
                return -1;
            durable_target->last_committed_percent = percent;
        }
        event_percent_progress(start_percent +
                               (percent / 100.0) * (end_percent - start_percent));
    } else {
        event_message("LOG", line->str);
    }
    return 0;
}

/* Run a long external tool while converting its human progress display into
 * the helper protocol.  Both GUI and CLI therefore track qemu-img and wimlib
 * without either frontend reimplementing those operations. */
static int run_command_progress_range_internal(char *const arguments[],
                                               double start_percent,
                                               double end_percent,
                                               const char *durable_path)
{
    int output_pipe[2] = {-1, -1};
    pid_t child;
    int status;
    int result = -1;
    int saved_errno = 0;
    char buffer[4096];
    GString *line;
    DurableProgressTarget durable_target = {.descriptor = -1};
    DurableProgressTarget *durable = durable_path != NULL ? &durable_target : NULL;

    if (cancelled || revalidate_active_target() != 0) { errno = cancelled ? ECANCELED : errno; return -1; }
    if (durable != NULL && durable_progress_target_open(durable, durable_path) != 0)
        return -1;
    if (pipe(output_pipe) != 0)
        goto out;
    child = fork();
    if (child < 0) {
        close(output_pipe[0]);
        close(output_pipe[1]);
        goto out;
    }
    if (child == 0) {
        dup2(output_pipe[1], STDOUT_FILENO);
        dup2(output_pipe[1], STDERR_FILENO);
        close(output_pipe[0]);
        close(output_pipe[1]);
        exec_tool(arguments);
        _exit(127);
    }

    close(output_pipe[1]);
    decompressor_pid = child;
    line = g_string_sized_new(256);
    event_indeterminate_progress();
    while (!cancelled) {
        ssize_t count;
        ssize_t index;

        if (durable != NULL) {
            struct pollfd ready = {
                .fd = output_pipe[0],
                .events = POLLIN,
            };
            int poll_result = poll(&ready, 1, DURABLE_TOOL_COMMIT_INTERVAL_MS);
            if (poll_result < 0 && errno == EINTR)
                continue;
            if (poll_result < 0) {
                saved_errno = errno;
                break;
            }
            if (poll_result == 0) {
                /* wimlib's text UI normally reports only whole percentages,
                 * which can represent hundreds of MiB on a Windows To Go
                 * image.  Commit on a timer too, so a large image cannot
                 * accumulate an entire percentage in the page cache. */
                if (commit_descriptor(durable->descriptor,
                                      durable->whole_filesystem) != 0) {
                    saved_errno = errno;
                    break;
                }
                continue;
            }
        }
        count = read(output_pipe[0], buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            saved_errno = errno;
            break;
        }
        if (count == 0)
            break;
        for (index = 0; index < count; index++) {
            if (buffer[index] == '\r' || buffer[index] == '\n') {
                if (process_tool_progress_line(line, start_percent, end_percent,
                                               durable) != 0) {
                    saved_errno = errno;
                    break;
                }
                g_string_truncate(line, 0);
            } else if (line->len < 16384) {
                g_string_append_c(line, buffer[index]);
            }
        }
        if (saved_errno != 0)
            break;
    }
    if (line->len > 0 && saved_errno == 0 &&
        process_tool_progress_line(line, start_percent, end_percent, durable) != 0)
        saved_errno = errno;
    g_string_free(line, TRUE);
    close(output_pipe[0]);
    if (cancelled || saved_errno != 0)
        kill(child, SIGTERM);
    if (wait_child(child, &status) < 0) {
        if (saved_errno == 0)
            saved_errno = errno;
        decompressor_pid = -1;
        goto out;
    }
    decompressor_pid = -1;
    if (cancelled) {
        saved_errno = ECANCELED;
        goto out;
    }
    if (saved_errno != 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        goto out;
    if (durable != NULL) {
        if (commit_descriptor(durable->descriptor, durable->whole_filesystem) != 0) {
            saved_errno = errno;
            goto out;
        }
        event_percent_progress(end_percent);
    }
    result = 0;

out:
    if (durable_target.descriptor >= 0)
        close(durable_target.descriptor);
    if (saved_errno != 0)
        errno = saved_errno;
    return result;
}

static int run_command_progress_range(char *const arguments[], double start_percent,
                                      double end_percent)
{
    return run_command_progress_range_internal(arguments, start_percent, end_percent,
                                               NULL);
}

static int run_command_progress_range_durable(char *const arguments[],
                                              double start_percent,
                                              double end_percent,
                                              const char *target_path)
{
    return run_command_progress_range_internal(arguments, start_percent, end_percent,
                                               target_path);
}

static int run_command_progress(char *const arguments[])
{
    return run_command_progress_range(arguments, 0.0, 100.0);
}

static char *bundled_bcd_sys_tool_path(const char *name)
{
    char executable[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    char *libexec_directory;
    char *usr_directory;
    char *result;
    if (length <= 0 || (size_t)length >= sizeof(executable) - 1)
        return NULL;
    executable[length] = '\0';
    libexec_directory = g_path_get_dirname(executable);
    usr_directory = g_path_get_dirname(libexec_directory);
    g_free(libexec_directory);
    result = g_build_filename(usr_directory, "libexec", "bcd-sys", name, NULL);
    g_free(usr_directory);
    if (access(result, R_OK) != 0)
        g_clear_pointer(&result, g_free);
    return result;
}

static char *bundled_bcd_sys_path(void)
{
    return bundled_bcd_sys_tool_path("AppRun");
}

static char *bundled_data_path(const char *name)
{
    char executable[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    char *libexec_directory;
    char *usr_directory;
    char *result;
    if (length <= 0 || (size_t)length >= sizeof(executable) - 1)
        return NULL;
    executable[length] = '\0';
    libexec_directory = g_path_get_dirname(executable);
    usr_directory = g_path_get_dirname(libexec_directory);
    g_free(libexec_directory);
    result = g_build_filename(usr_directory, "share", "lufus", name, NULL);
    g_free(usr_directory);
    if (access(result, R_OK) != 0)
        g_clear_pointer(&result, g_free);
    return result;
}

typedef struct {
    uint64_t completed;
    uint64_t submitted;
    uint64_t total;
} CopyProgress;

static void copy_progress_emit(const CopyProgress *progress)
{
    printf("PROGRESS %llu %llu\n",
           (unsigned long long)progress->completed,
           (unsigned long long)progress->total);
    fflush(stdout);
}

static int copy_progress_commit(CopyProgress *progress, int filesystem_fd)
{
    if (progress == NULL || progress->submitted == progress->completed)
        return 0;
    if (commit_descriptor(filesystem_fd, true) != 0)
        return -1;
    progress->completed = progress->submitted;
    copy_progress_emit(progress);
    return 0;
}

static int copy_progress_commit_path(CopyProgress *progress, const char *path)
{
    int descriptor;
    int result;

    descriptor = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0)
        return -1;
    result = copy_progress_commit(progress, descriptor);
    close(descriptor);
    return result;
}

static int copy_regular_file(const char *source, const char *destination, mode_t mode,
                             CopyProgress *progress)
{
    unsigned char *buffer = NULL;
    int source_fd = -1;
    int destination_fd = -1;
    int result = -1;
    source_fd = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (source_fd < 0)
        goto out;
    destination_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                          mode & 0777);
    if (destination_fd < 0)
        goto out;
    buffer = malloc(COPY_BUFFER_SIZE);
    if (buffer == NULL)
        goto out;
    while (!cancelled) {
        ssize_t count = read(source_fd, buffer, COPY_BUFFER_SIZE);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 || (count > 0 && write_all(destination_fd, buffer, (size_t)count) != 0))
            goto out;
        if (count == 0)
            break;
        if (progress != NULL) {
            if ((uint64_t)count > UINT64_MAX - progress->submitted) {
                errno = EOVERFLOW;
                goto out;
            }
            progress->submitted += (uint64_t)count;
            if (progress->submitted - progress->completed >= COPY_COMMIT_INTERVAL &&
                copy_progress_commit(progress, destination_fd) != 0)
                goto out;
        }
    }
    if (cancelled) {
        errno = ECANCELED;
        goto out;
    }
    result = 0;
out:
    if (source_fd >= 0)
        close(source_fd);
    if (destination_fd >= 0)
        close(destination_fd);
    free(buffer);
    return result;
}

static bool is_install_image(const char *name)
{
    return strcasecmp(name, "install.wim") == 0 || strcasecmp(name, "install.esd") == 0;
}

/* Materialize ISO directory/file aliases on filesystems whose firmware cannot
 * follow Unix symlinks. Canonical paths must remain within the source mount;
 * directory aliases may not revisit an ancestor. Never copy special files. */
typedef struct {
    const char *root;
    GHashTable *ancestors;
    bool skip_split_image;
    uint64_t *total;
    uint64_t *skipped_total;
    char **split_source;
    char **split_destination;
    CopyProgress *progress;
} ImageTreeContext;

static char *image_tree_resolve(const char *root, const char *path,
                                struct stat *details)
{
    char *resolved = realpath(path, NULL);
    size_t length = strlen(root);
    if (resolved == NULL)
        return NULL;
    if (strncmp(root, resolved, length) != 0 ||
        (resolved[length] != '\0' && resolved[length] != '/')) {
        free(resolved);
        errno = EPERM;
        return NULL;
    }
    if (lstat(resolved, details) != 0) {
        free(resolved);
        return NULL;
    }
    if (!S_ISDIR(details->st_mode) && !S_ISREG(details->st_mode)) {
        free(resolved);
        errno = EINVAL;
        return NULL;
    }
    return resolved;
}

static int image_tree_walk(ImageTreeContext *context, const char *source,
                            const char *destination, unsigned int depth)
{
    DIR *directory = NULL;
    int result = -1;
    if (depth >= 128 || g_hash_table_contains(context->ancestors, source)) {
        errno = ELOOP;
        return -1;
    }
    g_hash_table_add(context->ancestors, g_strdup(source));
    if (destination != NULL && mkdir(destination, 0755) != 0 && errno != EEXIST)
        goto out;
    directory = opendir(source);
    if (directory == NULL)
        goto out;
    for (;;) {
        struct dirent *entry;
        struct stat details;
        char *path;
        char *resolved;
        char *output = NULL;
        int child_result = 0;
        if (cancelled) {
            errno = ECANCELED;
            goto out;
        }
        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0)
                goto out;
            result = 0;
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        path = g_build_filename(source, entry->d_name, NULL);
        resolved = image_tree_resolve(context->root, path, &details);
        g_free(path);
        if (resolved == NULL)
            goto out;
        if (destination != NULL)
            output = g_build_filename(destination, entry->d_name, NULL);
        if (S_ISDIR(details.st_mode)) {
            child_result = image_tree_walk(context, resolved, output, depth + 1);
        } else if (destination == NULL) {
            bool skip = context->skip_split_image && is_install_image(entry->d_name) &&
                        details.st_size > 4000000000LL;
            uint64_t *total = skip ? context->skipped_total : context->total;
            if (total != NULL) {
                if ((uint64_t)details.st_size > UINT64_MAX - *total) {
                    errno = EOVERFLOW;
                    child_result = -1;
                } else {
                    *total += (uint64_t)details.st_size;
                }
            }
        } else if (context->split_source != NULL && context->split_destination != NULL &&
                   is_install_image(entry->d_name) && details.st_size > 4000000000LL) {
            if (*context->split_source != NULL) {
                errno = EINVAL;
                child_result = -1;
            } else {
                char *parent = g_path_get_dirname(output);
                *context->split_source = g_strdup(resolved);
                *context->split_destination = g_build_filename(parent, "install.swm", NULL);
                g_free(parent);
            }
        } else {
            char log_message[512];
            snprintf(log_message, sizeof(log_message), "Copying %.480s", entry->d_name);
            event_message("LOG", log_message);
            child_result = copy_regular_file(resolved, output, details.st_mode,
                                             context->progress);
        }
        {
            int saved = errno;
            free(resolved);
            g_free(output);
            errno = saved;
        }
        if (child_result != 0)
            goto out;
    }
out:
    {
        int saved = errno;
        if (directory != NULL)
            closedir(directory);
        g_hash_table_remove(context->ancestors, source);
        errno = saved;
    }
    return result;
}

static int copy_tree_size(const char *source, bool skip_split_image, uint64_t *total,
                          uint64_t *skipped_total)
{
    char *root = realpath(source, NULL);
    int result;
    if (root == NULL)
        return -1;
    ImageTreeContext context = {
        .root = root,
        .ancestors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL),
        .skip_split_image = skip_split_image,
        .total = total,
        .skipped_total = skipped_total,
    };
    result = image_tree_walk(&context, root, NULL, 0);
    g_hash_table_destroy(context.ancestors);
    free(root);
    return result;
}

static int copy_tree(const char *source, const char *destination,
                     char **split_source, char **split_destination,
                     CopyProgress *progress)
{
    char *root = realpath(source, NULL);
    int result;
    if (root == NULL)
        return -1;
    ImageTreeContext context = {
        .root = root,
        .ancestors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL),
        .split_source = split_source,
        .split_destination = split_destination,
        .progress = progress,
    };
    result = image_tree_walk(&context, root, destination, 0);
    g_hash_table_destroy(context.ancestors);
    free(root);
    return result;
}

static int install_windows_legacy_boot(const char *disk, const char *partition)
{
    int disk_fd = -1;
    int partition_fd = -1;
    int result = -1;
    /* These byte arrays come from the vendored GPL-2.0-or-later ms-sys
     * sources used by Rufus. The partition table, disk signature and NTFS
     * BPB are deliberately left untouched. */
#include "../../upstream/rufus/src/ms-sys/inc/mbr_win7.h"
#include "../../upstream/rufus/src/ms-sys/inc/br_ntfs_0x0.h"
#include "../../upstream/rufus/src/ms-sys/inc/br_ntfs_0x54.h"

    /* The kernel considers a partition an active user of its parent disk, so
     * opening both nodes with O_EXCL is impossible even when nothing is
     * mounted. Safety was already revalidated immediately before this call. */
    if (cancelled || revalidate_active_target() != 0) return -1;
    disk_fd = open(disk, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    partition_fd = open(partition, O_RDWR | O_CLOEXEC);
    if (disk_fd < 0 || partition_fd < 0)
        goto out;
    if (active_target && target_descriptor_matches(active_target, disk_fd) != 0) goto out;
    if (pwrite(disk_fd, mbr_win7_0x0, sizeof(mbr_win7_0x0), 0) !=
            (ssize_t)sizeof(mbr_win7_0x0) ||
        pwrite(partition_fd, br_ntfs_0x0, sizeof(br_ntfs_0x0), 0) !=
            (ssize_t)sizeof(br_ntfs_0x0) ||
        pwrite(partition_fd, br_ntfs_0x54, sizeof(br_ntfs_0x54), 0x54) !=
            (ssize_t)sizeof(br_ntfs_0x54) ||
        fsync(disk_fd) != 0 || fsync(partition_fd) != 0)
        goto out;
    result = 0;
out:
    if (disk_fd >= 0)
        close(disk_fd);
    if (partition_fd >= 0)
        close(partition_fd);
    return result;
}

static bool is_validation_loader(const char *path)
{
    static const char *names[] = {
        "bootx64.efi", "bootia32.efi", "bootaa64.efi", "bootarm.efi",
        "bootriscv64.efi", "bootloongarch64.efi",
    };
    char *basename = g_path_get_basename(path);
    guint index;
    bool matches = false;
    for (index = 0; index < G_N_ELEMENTS(names); index++)
        if (g_ascii_strcasecmp(basename, names[index]) == 0) {
            matches = true;
            break;
        }
    g_free(basename);
    return matches;
}

static char *find_case_child(const char *parent, const char *name)
{
    DIR *directory = opendir(parent);
    struct dirent *entry;
    char *result = NULL;
    if (directory == NULL)
        return NULL;
    while ((entry = readdir(directory)) != NULL)
        if (g_ascii_strcasecmp(entry->d_name, name) == 0) {
            result = g_build_filename(parent, entry->d_name, NULL);
            break;
        }
    closedir(directory);
    return result;
}

static bool is_validation_manifest_entry(const char *relative)
{
    return g_ascii_strcasecmp(relative, "md5sum.txt") != 0 &&
           !is_validation_loader(relative);
}

static int validation_tree_size(const char *root, const char *relative,
                                uint64_t *total)
{
    char *directory_path = relative[0] == '\0' ? g_strdup(root) :
                           g_build_filename(root, relative, NULL);
    DIR *directory = opendir(directory_path);
    struct dirent *entry;
    if (directory == NULL) {
        g_free(directory_path);
        return -1;
    }
    while (!cancelled && (entry = readdir(directory)) != NULL) {
        char *child_relative;
        char *child_path;
        struct stat details;
        int child_result = 0;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        child_relative = relative[0] == '\0' ? g_strdup(entry->d_name) :
                         g_build_filename(relative, entry->d_name, NULL);
        child_path = g_build_filename(root, child_relative, NULL);
        if (lstat(child_path, &details) != 0) {
            child_result = -1;
        } else if (S_ISDIR(details.st_mode)) {
            child_result = validation_tree_size(root, child_relative, total);
        } else if (S_ISREG(details.st_mode) &&
                   is_validation_manifest_entry(child_relative)) {
            if ((uint64_t)details.st_size > UINT64_MAX - *total) {
                errno = EOVERFLOW;
                child_result = -1;
            } else {
                *total += (uint64_t)details.st_size;
            }
        }
        g_free(child_relative);
        g_free(child_path);
        if (child_result != 0) {
            closedir(directory);
            g_free(directory_path);
            return -1;
        }
    }
    closedir(directory);
    g_free(directory_path);
    if (cancelled) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

static int md5_tree(const char *root, const char *relative, GString *manifest,
                    uint64_t total, uint64_t *checked,
                    unsigned int *last_thousandths,
                    unsigned char *buffer, size_t buffer_size)
{
    char *directory_path = relative[0] == '\0' ? g_strdup(root) :
                           g_build_filename(root, relative, NULL);
    DIR *directory = opendir(directory_path);
    struct dirent *entry;
    if (directory == NULL) {
        g_free(directory_path);
        return -1;
    }
    while (!cancelled && (entry = readdir(directory)) != NULL) {
        char *child_relative;
        char *child_path;
        struct stat details;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        child_relative = relative[0] == '\0' ? g_strdup(entry->d_name) :
                         g_build_filename(relative, entry->d_name, NULL);
        child_path = g_build_filename(root, child_relative, NULL);
        if (lstat(child_path, &details) != 0) {
            g_free(child_relative);
            g_free(child_path);
            closedir(directory);
            g_free(directory_path);
            return -1;
        }
        if (S_ISDIR(details.st_mode)) {
            if (md5_tree(root, child_relative, manifest, total, checked,
                         last_thousandths, buffer, buffer_size) != 0) {
                g_free(child_relative);
                g_free(child_path);
                closedir(directory);
                g_free(directory_path);
                return -1;
            }
        } else if (S_ISREG(details.st_mode) &&
                   is_validation_manifest_entry(child_relative)) {
            GChecksum *checksum = g_checksum_new(G_CHECKSUM_MD5);
            int descriptor = open(child_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            ssize_t count = 0;
            if (descriptor < 0) {
                g_checksum_free(checksum);
                g_free(child_relative);
                g_free(child_path);
                closedir(directory);
                g_free(directory_path);
                return -1;
            }
            /* All writes are committed before this pass.  Drop clean cached
             * pages so validation reads the target media instead of merely
             * re-reading the buffers used by the copy stage. */
            (void)posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
            while (!cancelled) {
                unsigned int thousandths;
                do {
                    count = read(descriptor, buffer, buffer_size);
                } while (count < 0 && errno == EINTR);
                if (count <= 0)
                    break;
                g_checksum_update(checksum, buffer, (gsize)count);
                if ((uint64_t)count > UINT64_MAX - *checked) {
                    errno = EOVERFLOW;
                    count = -1;
                    break;
                }
                *checked += (uint64_t)count;
                thousandths = lufus_byte_progress_thousandths(*checked, total);
                if (thousandths != *last_thousandths) {
                    event_byte_progress(*checked, total);
                    *last_thousandths = thousandths;
                }
            }
            (void)posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
            close(descriptor);
            if (cancelled) {
                errno = ECANCELED;
                count = -1;
            }
            if (count < 0) {
                g_checksum_free(checksum);
                g_free(child_relative);
                g_free(child_path);
                closedir(directory);
                g_free(directory_path);
                return -1;
            }
            for (char *cursor = child_relative; *cursor != '\0'; cursor++)
                if (*cursor == G_DIR_SEPARATOR)
                    *cursor = '/';
            g_string_append_printf(manifest, "%s  ./%s\n",
                                   g_checksum_get_string(checksum), child_relative);
            g_checksum_free(checksum);
        }
        g_free(child_relative);
        g_free(child_path);
    }
    closedir(directory);
    g_free(directory_path);
    if (cancelled) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

static int enable_runtime_validation(const char *root)
{
    static const struct {
        const char *name;
        const char *asset;
    } loaders[] = {
        {"bootx64.efi", "md5/bootx64.efi"},
        {"bootia32.efi", "md5/bootia32.efi"},
        {"bootaa64.efi", "md5/bootaa64.efi"},
        {"bootarm.efi", "md5/bootarm.efi"},
        {"bootriscv64.efi", "md5/bootriscv64.efi"},
        {"bootloongarch64.efi", "md5/bootloongarch64.efi"},
    };
    char *efi = find_case_child(root, "efi");
    char *efi_boot = efi != NULL ? find_case_child(efi, "boot") : NULL;
    GString *manifest = g_string_new(NULL);
    unsigned char *buffer = NULL;
    uint64_t total = 0;
    uint64_t checked = 0;
    unsigned int last_thousandths = UINT_MAX;
    guint installed = 0;
    guint index;
    int result = -1;
    for (index = 0; index < G_N_ELEMENTS(loaders); index++) {
        char *loader = efi_boot != NULL ?
                       find_case_child(efi_boot, loaders[index].name) : NULL;
        char *stem = g_strndup(loaders[index].name, strlen(loaders[index].name) - 4);
        char *original_name = g_strdup_printf("%s_original.efi", stem);
        char *original = efi_boot != NULL ?
                         g_build_filename(efi_boot, original_name, NULL) : NULL;
        char *asset = bundled_data_path(loaders[index].asset);
        g_free(stem);
        g_free(original_name);
        if (loader != NULL && access(loader, R_OK) == 0 && asset != NULL) {
            if (rename(loader, original) != 0 ||
                copy_regular_file(asset, loader, 0644, NULL) != 0) {
                g_free(loader);
                g_free(original);
                g_free(asset);
                goto out;
            }
            installed++;
        }
        g_free(loader);
        g_free(original);
        g_free(asset);
    }
    if (installed == 0) {
        errno = ENOENT;
        goto out;
    }
    /* Commit loader replacement and all preceding media changes before the
     * validation pass starts reading from the target. */
    if (sync_mounted_filesystem(root) != 0 ||
        validation_tree_size(root, "", &total) != 0)
        goto out;
    if (total == 0)
        event_indeterminate_progress();
    else
        event_byte_progress(0, total);
    buffer = malloc(COPY_BUFFER_SIZE);
    if (buffer == NULL)
        goto out;
    if (md5_tree(root, "", manifest, total, &checked, &last_thousandths,
                 buffer, COPY_BUFFER_SIZE) != 0)
        goto out;
    if (checked != total) {
        errno = EIO;
        goto out;
    }
    {
        char *header = g_strdup_printf("# md5sum_totalbytes = 0x%llx\n",
                                       (unsigned long long)total);
        g_string_prepend(manifest, header);
        g_free(header);
    }
    {
        char *path = g_build_filename(root, "md5sum.txt", NULL);
        GError *error = NULL;
        if (!g_file_set_contents(path, manifest->str, (gssize)manifest->len, &error)) {
            if (error != NULL) {
                event_message("ERROR", error->message);
                g_clear_error(&error);
            }
            g_free(path);
            goto out;
        }
        g_free(path);
    }
    if (sync_mounted_filesystem(root) != 0)
        goto out;
    if (total == 0)
        event_percent_progress(100.0);
    else
        event_byte_progress(total, total);
    event_message("LOG", "Installed signed UEFI runtime media validation");
    result = 0;
out:
    free(buffer);
    g_string_free(manifest, TRUE);
    g_free(efi);
    g_free(efi_boot);
    return result;
}

static char *find_partition_path(const char *disk, guint number)
{
    char *candidates[2];
    guint attempt;
    candidates[0] = g_strdup_printf("%s%u", disk, number);
    candidates[1] = g_strdup_printf("%sp%u", disk, number);
    for (attempt = 0; attempt < 50; attempt++) {
        guint index;
        for (index = 0; index < 2; index++) {
            struct stat details;
            if (stat(candidates[index], &details) == 0 && S_ISBLK(details.st_mode)) {
                char *result = candidates[index];
                g_free(candidates[1 - index]);
                return result;
            }
        }
        usleep(100000);
    }
    g_free(candidates[0]);
    g_free(candidates[1]);
    errno = ENOENT;
    return NULL;
}

static int windows_installer_command(const char *source, const char *target_argument,
                                     const char *expected_identity, uint64_t expected_capacity,
                                     bool uefi_ntfs, const char *target_system,
                                     bool runtime_validation, guint windows_flags,
                                     const char *architecture, const char *local_account,
                                     guint windows_index)
{
    static const char *fat_partition_script =
        "label: gpt\n"
        "unit: sectors\n"
        "first-lba: 2048\n"
        "\n"
        /* The installer payload must remain visible to WinPE after it boots.
         * Marking the whole FAT32 volume as an EFI System Partition makes
         * Windows report it as Hidden and Setup can no longer find install.swm.
         * OVMF still discovers the removable-media EFI/BOOT/BOOTX64.EFI path
         * on a Microsoft Basic Data FAT filesystem. */
        "start=2048, type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7, name=\"%s\"\n";
    TargetInfo target = {0};
    LufusWindowsOptions options = {0};
    char source_template[] = "/tmp/lufus-iso.XXXXXX";
    char target_template[] = "/tmp/lufus-target.XXXXXX";
    char answer_template[] = "/tmp/lufus-unattend.XXXXXX";
    char *source_mount = NULL;
    char *target_mount = NULL;
    char *partition = NULL;
    char *uefi_partition = NULL;
    char *uefi_image = NULL;
    char *partition_script = NULL;
    char *split_source = NULL;
    char *split_destination = NULL;
    char *boot_wim = NULL;
    char *install_image = NULL;
    char *detected_language = NULL;
    char *answer_path = NULL;
    char *oem_directory = NULL;
    char *oem_answer = NULL;
    char *appraiser = NULL;
    char *appraiser_backup = NULL;
    bool source_mounted = false;
    bool target_mounted = false;
    int target_fd = -1;
    int answer_fd = -1;
    int result = 1;
    CopyProgress copy_progress = {0};
    uint64_t split_image_bytes = 0;
    bool legacy_bios = target_system != NULL &&
                       (strcmp(target_system, "bios") == 0 ||
                        strcmp(target_system, "bios-uefi") == 0);
    bool hybrid_boot = target_system != NULL && strcmp(target_system, "bios-uefi") == 0;
    bool ntfs_payload = uefi_ntfs || legacy_bios;
    g_autofree char *volume_label = lufus_volume_label_for_image(source, !ntfs_payload);
    g_autofree char *partition_attributes = g_strdup_printf("name=\"%s\"", volume_label);
    LufusWindowsProgressLayout progress =
        lufus_rufus_windows_progress_layout(ntfs_payload, windows_flags != 0);
    LufusMediaProgressLayout media_progress =
        lufus_media_progress_layout(runtime_validation);
    unsigned int parent_progress_start = operation_progress_start;
    unsigned int parent_progress_end = operation_progress_end;

    options.flags = windows_flags;
    options.image_index = windows_index;
    options.boot_media_partition = (uefi_ntfs || hybrid_boot) ? 2 : 1;
    options.architecture = (char *)(architecture != NULL ? architecture : "amd64");
    options.local_account = (char *)local_account;
    if ((windows_flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0 &&
        g_strcmp0(target_system, "uefi") != 0) {
        event_message("ERROR", "Silent installation requires UEFI Windows media");
        return 1;
    }
    {
        GError *error = NULL;
        if (!lufus_windows_options_validate(&options, &error)) {
            event_message("ERROR", error != NULL ? error->message :
                          "Invalid Windows setup options");
            g_clear_error(&error);
            return 1;
        }
    }
    if (stat(source, &target.details) != 0 || !S_ISREG(target.details.st_mode)) {
        event_message("ERROR", "Windows source must be a local ISO file");
        return 1;
    }
    memset(&target.details, 0, sizeof(target.details));
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount target filesystems");
        goto out;
    }
    source_mount = mkdtemp(source_template);
    target_mount = mkdtemp(target_template);
    if (source_mount == NULL || target_mount == NULL) {
        event_errno("Could not create temporary mount directories");
        goto out;
    }
    set_operation_progress_subrange(parent_progress_start, parent_progress_end,
                                    media_progress.media_start,
                                    media_progress.media_end);
    event_message("STARTED", "Creating Windows installer media");
    event_stage_text(0, progress.analyze_end, "Analyzing existing boot records...");
    event_indeterminate_progress();
    if (inspect_boot_records(target.path) != 0) {
        event_errno("Could not inspect the existing boot records");
        goto out;
    }
    event_percent_progress(100.0);
    {
        /* UDF hides files with the hidden attribute unless unhide is set.
         * Windows media can contain such payload files (e.g. sources/ws.dat). */
        char *mount_source[] = {"mount", "-o", "loop,ro,unhide", "--", (char *)source, source_mount, NULL};
        if (run_command_input(mount_source, NULL) != 0) {
            event_message("ERROR", "Could not mount the Windows ISO");
            goto out;
        }
        source_mounted = true;
    }
    boot_wim = g_build_filename(source_mount, "sources", "boot.wim", NULL);
    if (access(boot_wim, R_OK) != 0) {
        event_message("ERROR", "The selected ISO does not contain sources/boot.wim");
        goto out;
    }
    if ((windows_flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
        install_image = g_build_filename(source_mount, "sources", "install.wim", NULL);
        if (access(install_image, R_OK) != 0) {
            g_free(install_image);
            install_image = g_build_filename(source_mount, "sources", "install.esd", NULL);
        }
        if (access(install_image, R_OK) != 0) {
            g_free(install_image);
            install_image = g_build_filename(source_mount, "sources", "install.swm", NULL);
        }
        if (access(install_image, R_OK) != 0) {
            event_message("ERROR", "The selected ISO has no supported Windows install image");
            goto out;
        }
        event_message("LOG", "Checking the selected unattended Windows edition");
        {
            GError *error = NULL;
            detected_language = windows_image_default_language(install_image,
                                                               windows_index,
                                                               &error);
            if (detected_language == NULL) {
                event_message("ERROR", error != NULL ? error->message :
                              "Could not inspect the selected Windows edition");
                g_clear_error(&error);
                goto out;
            }
        }
        options.language = detected_language;
        {
            GError *error = NULL;
            if (!lufus_windows_options_validate(&options, &error)) {
                event_message("ERROR", error != NULL ? error->message :
                              "Invalid Windows image language");
                g_clear_error(&error);
                goto out;
            }
        }
        {
            char message[128];
            snprintf(message, sizeof(message), "Using Windows language %s for unattended setup",
                     detected_language);
            event_message("LOG", message);
        }
    }
    if (ntfs_payload && (uefi_ntfs || hybrid_boot)) {
        uint64_t sectors = target.capacity / 512;
        uint64_t uefi_start;
        uint64_t main_size;
        if (sectors < 8192) {
            event_message("ERROR", "The target is too small for UEFI:NTFS media");
            goto out;
        }
        uefi_start = (sectors - 34 - 2048) & ~UINT64_C(2047);
        main_size = uefi_start - 2048;
        partition_script = g_strdup_printf(
            "%s"
            "unit: sectors\n"
            "\n"
            "start=2048, size=%" PRIu64 ", type=%s, %s\n"
            "start=%" PRIu64 ", size=2048, type=%s\n",
            hybrid_boot ? "label: dos\n" : "label: gpt\nfirst-lba: 2048\n",
            main_size,
            hybrid_boot ? "7" : "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7",
            hybrid_boot ? "bootable" : partition_attributes,
            uefi_start,
            hybrid_boot ? "ef" : "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7");
    } else if (legacy_bios) {
        partition_script = g_strdup(
            "label: dos\nunit: sectors\n\n"
            "start=2048, type=7, bootable\n");
    } else {
        partition_script = g_strdup_printf(fat_partition_script, volume_label);
    }
    {
        char *wipe[] = {"wipefs", "--all", "--force", "--quiet", target.path, NULL};
        char *sfdisk[] = {"sfdisk", "--wipe", "always", target.path, NULL};
        event_stage_text(progress.analyze_end, progress.zero_end,
                         "Clearing MBR/PBR/GPT structures...");
        event_indeterminate_progress();
        if (run_command_input(wipe, NULL) != 0) {
            event_message("ERROR", "Could not clear the existing partition records");
            goto out;
        }
        event_stage_text(progress.zero_end, progress.partition_end,
                         legacy_bios ? "Partitioning (MBR)..." : "Partitioning (GPT)...");
        event_indeterminate_progress();
        if (run_command_input(sfdisk, partition_script) != 0) {
            event_message("ERROR", "Could not create the GPT partition table");
            goto out;
        }
    }
    event_stage_text(progress.partition_end, progress.format_end,
                     ntfs_payload ? "Formatting (NTFS)..." : "Formatting (FAT32)...");
    event_indeterminate_progress();
    target_fd = open(target.path, O_RDONLY | O_CLOEXEC | O_EXCL);
    if (target_fd >= 0) {
        ioctl(target_fd, BLKRRPART, 0);
        close(target_fd);
        target_fd = -1;
    }
    partition = find_partition_path(target.path, 1);
    if (partition == NULL) {
        event_errno("Could not locate the new target partition");
        goto out;
    }
    if (ntfs_payload) {
        char *mkfs[] = {"mkfs.ntfs", "-F", "-Q", "-L", volume_label, partition, NULL};
        if (run_command_input(mkfs, NULL) != 0) {
            event_message("ERROR", "Could not format the target as NTFS");
            goto out;
        }
        if (uefi_ntfs || hybrid_boot) {
            uefi_partition = find_partition_path(target.path, 2);
            uefi_image = bundled_data_path("uefi-ntfs.img");
            if (uefi_partition == NULL || uefi_image == NULL) {
                event_message("ERROR", "Could not locate the UEFI:NTFS loader or target partition");
                goto out;
            }
            {
                char *input = g_strdup_printf("if=%s", uefi_image);
                char *output = g_strdup_printf("of=%s", uefi_partition);
                char *dd[] = {"dd", input, output, "bs=1M", "conv=fsync,notrunc", "status=none", NULL};
                int dd_result = run_command_input(dd, NULL);
                g_free(input);
                g_free(output);
                if (dd_result != 0) {
                    event_message("ERROR", "Could not write the UEFI:NTFS loader");
                    goto out;
                }
            }
            event_message("LOG", "Installed the signed UEFI:NTFS loader");
        }
        if (legacy_bios) {
            event_stage_text(progress.format_end, progress.copy_start,
                             "Writing Master Boot Record...");
            event_indeterminate_progress();
            if (install_windows_legacy_boot(target.path, partition) != 0) {
                event_errno("Could not install legacy Windows MBR and NTFS boot records");
                goto out;
            }
            event_message("LOG", "Installed legacy BIOS Windows boot records from ms-sys");
        }
    } else {
        char *mkfs[] = {"mkfs.vfat", "-F", "32", "-n", volume_label, partition, NULL};
        if (run_command_input(mkfs, NULL) != 0) {
            event_message("ERROR", "Could not format the target as FAT32");
            goto out;
        }
    }
    if (!legacy_bios) {
        event_stage_text(progress.format_end, progress.copy_start,
                         "Writing Master Boot Record...");
        event_percent_progress(100.0);
    }
    {
        event_stage_text(progress.copy_start, progress.copy_start,
                         "Writing Partition Boot Record...");
        event_indeterminate_progress();
        if (mount_target_partition(partition, target_mount, target.path, ntfs_payload) != 0) {
            event_message("ERROR", "Could not mount the target filesystem");
            goto out;
        }
        target_mounted = true;
    }
    if (copy_tree_size(source_mount, !ntfs_payload, &copy_progress.total,
                       &split_image_bytes) != 0 ||
        split_image_bytes > UINT64_MAX - copy_progress.total) {
        event_errno("Could not calculate Windows media size");
        goto out;
    }
    copy_progress.total += split_image_bytes;
    event_stage_percent(progress.copy_start, progress.copy_end,
                        LUFUS_RUFUS_STATUS_COPY_ISO);
    if (copy_progress.total == 0)
        event_indeterminate_progress();
    if (copy_tree(source_mount, target_mount,
                  ntfs_payload ? NULL : &split_source,
                  ntfs_payload ? NULL : &split_destination,
                  &copy_progress) != 0) {
        event_errno(cancelled ? "Windows media creation cancelled" : "Could not copy Windows ISO files");
        goto out;
    }
    if (copy_progress_commit_path(&copy_progress, target_mount) != 0) {
        event_errno("Could not commit copied Windows ISO files to the device");
        goto out;
    }
    if (split_source != NULL) {
        char *split[] = {"wimlib-imagex", "split", split_source, split_destination, "3800", NULL};
        double split_start_percent = copy_progress.total == 0 ? 0.0 :
                                     100.0 * (double)copy_progress.completed /
                                     (double)copy_progress.total;
        event_message("LOG", "Splitting the Windows install image for FAT32");
        if (run_command_progress_range_durable(split, split_start_percent, 100.0,
                                               target_mount) != 0) {
            event_message("ERROR", "Could not split the Windows install image");
            goto out;
        }
        event_percent_progress(100.0);
    }
    if (options.flags != 0) {
        event_stage_percent(progress.copy_end, progress.finalize_allocation_start,
                            LUFUS_RUFUS_STATUS_CUSTOMIZE_WINDOWS);
        event_percent_progress(0.0);
        answer_fd = mkstemp(answer_template);
        if (answer_fd < 0) {
            event_errno("Could not create Windows answer file");
            goto out;
        }
        close(answer_fd);
        answer_fd = -1;
        answer_path = answer_template;
        {
            GError *error = NULL;
            if (!lufus_windows_write_unattend(answer_path, &options, &error)) {
                event_message("ERROR", error != NULL ? error->message : "Could not create Windows answer file");
                g_clear_error(&error);
                goto out;
            }
        }
        event_percent_progress(100.0 * 101.0 / LUFUS_RUFUS_PATCH_PROGRESS_TOTAL);
        if ((options.flags & (LUFUS_WINDOWS_BYPASS_REQUIREMENTS |
                              LUFUS_WINDOWS_SILENT_INSTALL)) != 0) {
            char *target_boot_wim = g_build_filename(target_mount, "sources", "boot.wim", NULL);
            char *command = g_strdup_printf("--command=add %s /Autounattend.xml", answer_path);
            char *update[] = {"wimlib-imagex", "update", target_boot_wim, "2", command, NULL};
            if ((options.flags & LUFUS_WINDOWS_BYPASS_REQUIREMENTS) != 0) {
                appraiser = g_build_filename(target_mount, "sources", "appraiserres.dll", NULL);
                appraiser_backup = g_build_filename(target_mount, "sources", "appraiserres.bak", NULL);
                if (access(appraiser, F_OK) == 0) {
                    if (rename(appraiser, appraiser_backup) != 0 ||
                        (answer_fd = open(appraiser, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0) {
                        event_errno("Could not install the Windows upgrade compatibility bypass");
                        g_free(command);
                        g_free(target_boot_wim);
                        goto out;
                    }
                    close(answer_fd);
                    answer_fd = -1;
                }
            }
            event_percent_progress(100.0 * 102.0 / LUFUS_RUFUS_PATCH_PROGRESS_TOTAL);
            event_message("LOG", "Injecting Windows setup options into boot.wim");
            event_percent_progress(100.0 * 103.0 / LUFUS_RUFUS_PATCH_PROGRESS_TOTAL);
            event_percent_progress(100.0 * 104.0 / LUFUS_RUFUS_PATCH_PROGRESS_TOTAL);
            if (run_command_input(update, NULL) != 0) {
                char *fallback[] = {"wimlib-imagex", "update", target_boot_wim, "1", command, NULL};
                event_message("LOG", "Windows Setup was not at boot.wim index 2; trying index 1");
                if (run_command_input(fallback, NULL) != 0) {
                    event_message("ERROR", "Could not customize sources/boot.wim");
                    g_free(command);
                    g_free(target_boot_wim);
                    goto out;
                }
            }
            g_free(command);
            g_free(target_boot_wim);
        } else {
            oem_directory = g_build_filename(target_mount, "sources", "$OEM$", "$$", "Panther", NULL);
            oem_answer = g_build_filename(oem_directory, "unattend.xml", NULL);
            if (g_mkdir_with_parents(oem_directory, 0755) != 0 ||
                copy_regular_file(answer_path, oem_answer, 0644, NULL) != 0) {
                event_errno("Could not install Windows setup options");
                goto out;
            }
            event_percent_progress(100.0 * 103.0 / LUFUS_RUFUS_PATCH_PROGRESS_TOTAL);
        }
        /* Rufus always closes OP_PATCH at 207/207, including option sets that
         * do not require boot.wim modification. */
        if (sync_mounted_filesystem(target_mount) != 0) {
            event_errno("Could not commit Windows customization to the device");
            goto out;
        }
        event_percent_progress(100.0);
    }
    event_stage_text(progress.finalize_visible_start, 1000,
                     LUFUS_RUFUS_STATUS_FINALIZE);
    event_percent_progress(100.0);
    if (runtime_validation) {
        set_operation_progress_subrange(parent_progress_start, parent_progress_end,
                                        media_progress.verification_start,
                                        media_progress.verification_end);
        event_stage_percent(0, 1000, LUFUS_STATUS_VERIFY_MEDIA);
        event_indeterminate_progress();
        event_message("LOG", "Installing runtime media validation");
        if (enable_runtime_validation(target_mount) != 0) {
            event_errno("Could not install runtime UEFI media validation");
            goto out;
        }
    }
    set_operation_progress_subrange(parent_progress_start, parent_progress_end,
                                    media_progress.finalize_start,
                                    media_progress.finalize_end);
    event_stage_text(0, 1000, LUFUS_RUFUS_STATUS_FINALIZE);
    event_percent_progress(0.0);
    if (sync_mounted_filesystem(target_mount) != 0) {
        event_errno("Could not flush the Windows media filesystem");
        goto out;
    }
    result = 0;

out:
    if (answer_fd >= 0)
        close(answer_fd);
    if (target_mounted) {
        char *unmount[] = {"umount", "--", target_mount, NULL};
        if (run_command_input(unmount, NULL) != 0 && result == 0)
            result = 1;
    }
    if (source_mounted) {
        char *unmount[] = {"umount", "--", source_mount, NULL};
        run_command_input(unmount, NULL);
    }
    if (result == 0) {
        event_percent_progress(100.0);
        event_message("COMPLETE", "Windows installer media created");
    }
    if (answer_path != NULL)
        unlink(answer_path);
    if (target_mount != NULL)
        rmdir(target_mount);
    if (source_mount != NULL)
        rmdir(source_mount);
    g_free(partition);
    g_free(uefi_partition);
    g_free(uefi_image);
    g_free(partition_script);
    g_free(split_source);
    g_free(split_destination);
    g_free(boot_wim);
    g_free(install_image);
    g_free(detected_language);
    g_free(oem_directory);
    g_free(oem_answer);
    g_free(appraiser);
    g_free(appraiser_backup);
    target_info_clear(&target);
    operation_progress_start = parent_progress_start;
    operation_progress_end = parent_progress_end;
    return result;
}

static int windows_to_go_command(const char *source, const char *target_argument,
                                 const char *expected_identity, uint64_t expected_capacity,
                                 guint windows_flags, const char *architecture,
                                 const char *local_account, guint windows_index)
{
    static const char *partition_format =
        "label: gpt\n"
        "unit: sectors\n"
        "first-lba: 2048\n"
        "\n"
        "start=2048, size=1048576, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name=\"EFI System\"\n"
        "type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7, name=\"%s\"\n";
    g_autofree char *volume_label = lufus_volume_label_for_image(source, FALSE);
    g_autofree char *partition_script = g_strdup_printf(partition_format, volume_label);
    TargetInfo target = {0};
    LufusWindowsOptions options = {0};
    char source_template[] = "/tmp/lufus-iso.XXXXXX";
    char esp_template[] = "/tmp/lufus-esp.XXXXXX";
    char windows_template[] = "/tmp/lufus-windows.XXXXXX";
    char answer_template[] = "/tmp/lufus-unattend.XXXXXX";
    char *source_mount = NULL;
    char *esp_mount = NULL;
    char *windows_mount = NULL;
    char *bcd_script = NULL;
    char *esp_partition = NULL;
    char *windows_partition = NULL;
    char *install_image = NULL;
    char *split_reference = NULL;
    char *answer_path = NULL;
    char *panther = NULL;
    char *unattend = NULL;
    char *system_hive = NULL;
    char *san_policy_tool = NULL;
    bool source_mounted = false;
    bool esp_mounted = false;
    bool windows_mounted = false;
    int descriptor = -1;
    int answer_fd = -1;
    int result = 1;
    LufusWindowsProgressLayout progress =
        lufus_rufus_windows_progress_layout(true, false);

    if (windows_index == 0) {
        event_message("ERROR", "Windows image indexes start at 1");
        return 1;
    }
    if ((windows_flags & LUFUS_WINDOWS_SILENT_INSTALL) != 0) {
        event_message("ERROR", "Silent installation applies only to Windows installer media");
        return 1;
    }
    options.flags = windows_flags;
    options.image_index = windows_index;
    options.architecture = (char *)(architecture != NULL ? architecture : "amd64");
    options.local_account = (char *)local_account;
    {
        GError *error = NULL;
        if (!lufus_windows_options_validate(&options, &error)) {
            event_message("ERROR", error != NULL ? error->message :
                          "Invalid Windows setup options");
            g_clear_error(&error);
            return 1;
        }
    }
    if (stat(source, &target.details) != 0 || !S_ISREG(target.details.st_mode)) {
        event_message("ERROR", "Windows source must be a local ISO file");
        return 1;
    }
    memset(&target.details, 0, sizeof(target.details));
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if (target.capacity < 8ULL * 1024ULL * 1024ULL * 1024ULL) {
        event_message("ERROR", "Windows To Go requires a target of at least 8 GiB");
        goto out;
    }
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount target filesystems");
        goto out;
    }
    source_mount = mkdtemp(source_template);
    esp_mount = mkdtemp(esp_template);
    windows_mount = mkdtemp(windows_template);
    if (source_mount == NULL || esp_mount == NULL || windows_mount == NULL) {
        event_errno("Could not create temporary directories");
        goto out;
    }
    event_message("STARTED", "Creating Windows To Go media");
    event_stage_text(0, progress.analyze_end, "Analyzing existing boot records...");
    event_indeterminate_progress();
    if (inspect_boot_records(target.path) != 0) {
        event_errno("Could not inspect the existing boot records");
        goto out;
    }
    event_percent_progress(100.0);
    {
        char *mount_source[] = {"mount", "-o", "loop,ro,unhide", "--", (char *)source, source_mount, NULL};
        if (run_command_input(mount_source, NULL) != 0) {
            event_message("ERROR", "Could not mount the Windows ISO");
            goto out;
        }
        source_mounted = true;
    }
    install_image = g_build_filename(source_mount, "sources", "install.wim", NULL);
    if (access(install_image, R_OK) != 0) {
        g_free(install_image);
        install_image = g_build_filename(source_mount, "sources", "install.esd", NULL);
    }
    if (access(install_image, R_OK) != 0) {
        g_free(install_image);
        install_image = g_build_filename(source_mount, "sources", "install.swm", NULL);
        if (access(install_image, R_OK) == 0)
            split_reference = g_build_filename(source_mount, "sources", "install*.swm", NULL);
    }
    if (access(install_image, R_OK) != 0) {
        event_message("ERROR", "The selected ISO has no supported Windows install image");
        goto out;
    }
    {
        char *wipe[] = {"wipefs", "--all", "--force", "--quiet", target.path, NULL};
        char *sfdisk[] = {"sfdisk", "--wipe", "always", target.path, NULL};
        event_stage_text(progress.analyze_end, progress.zero_end,
                         "Clearing MBR/PBR/GPT structures...");
        event_indeterminate_progress();
        if (run_command_input(wipe, NULL) != 0) {
            event_message("ERROR", "Could not clear the existing partition records");
            goto out;
        }
        event_stage_text(progress.zero_end, progress.partition_end,
                         "Partitioning (GPT)...");
        event_indeterminate_progress();
        if (run_command_input(sfdisk, partition_script) != 0) {
            event_message("ERROR", "Could not create Windows To Go partitions");
            goto out;
        }
    }
    descriptor = open(target.path, O_RDONLY | O_CLOEXEC | O_EXCL);
    if (descriptor >= 0) {
        ioctl(descriptor, BLKRRPART, 0);
        close(descriptor);
        descriptor = -1;
    }
    esp_partition = find_partition_path(target.path, 1);
    windows_partition = find_partition_path(target.path, 2);
    if (esp_partition == NULL || windows_partition == NULL) {
        event_errno("Could not locate Windows To Go partitions");
        goto out;
    }
    {
        char *format_esp[] = {"mkfs.vfat", "-F", "32", esp_partition, NULL};
        char *format_windows[] = {"mkfs.ntfs", "-F", "-Q", "-L", volume_label, windows_partition, NULL};
        event_stage_text(progress.partition_end, progress.format_end,
                         "Formatting (NTFS)...");
        event_indeterminate_progress();
        if (run_command_input(format_esp, NULL) != 0 ||
            run_command_input(format_windows, NULL) != 0) {
            event_message("ERROR", "Could not format Windows To Go partitions");
            goto out;
        }
    }
    event_stage_text(progress.format_end, progress.copy_start,
                     "Writing Master Boot Record...");
    event_percent_progress(100.0);
    event_stage_text(progress.copy_start, progress.copy_start,
                     "Writing Partition Boot Record...");
    event_percent_progress(100.0);
    {
        char *index = g_strdup_printf("%u", windows_index);
        char *reference = split_reference != NULL ?
                          g_strdup_printf("--ref=%s", split_reference) : NULL;
        char *apply[] = {"wimlib-imagex", "apply", install_image, index,
                         windows_partition, reference, NULL};
        event_message("LOG", "Applying the selected Windows image to NTFS");
        event_stage_percent(progress.copy_start, progress.copy_end,
                            LUFUS_RUFUS_STATUS_APPLY_WINDOWS);
        if (run_command_progress_range_durable(apply, 0.0, 98.0,
                                               windows_partition) != 0) {
            event_message("ERROR", "Could not apply the selected Windows image");
            g_free(reference);
            g_free(index);
            goto out;
        }
        event_percent_progress(98.0);
        g_free(reference);
        g_free(index);
    }
    {
        event_message("LOG", "Mounting Windows To Go filesystems");
        if (mount_target_partition(esp_partition, esp_mount, target.path, false) != 0) {
            event_message("ERROR", "Could not mount Windows To Go partitions");
            goto out;
        }
        esp_mounted = true;
        if (mount_target_partition(windows_partition, windows_mount, target.path, true) != 0) {
            event_message("ERROR", "Could not mount Windows To Go partitions");
            goto out;
        }
        windows_mounted = true;
    }
    if ((windows_flags & LUFUS_WINDOWS_OFFLINE_INTERNAL_DRIVES) != 0) {
        system_hive = g_build_filename(windows_mount, "Windows", "System32",
                                       "config", "SYSTEM", NULL);
        san_policy_tool = bundled_bcd_sys_tool_path("set-windows-san-policy");
        if (san_policy_tool == NULL || access(system_hive, R_OK | W_OK) != 0) {
            event_message("ERROR", "The bundled Windows SAN policy editor is unavailable");
            goto out;
        }
        {
            char *set_policy[] = {san_policy_tool, system_hive, NULL};
            event_message("LOG", "Setting Windows To Go internal disks offline");
            if (run_command_input(set_policy, NULL) != 0) {
                event_message("ERROR", "Could not apply the Windows To Go SAN policy");
                goto out;
            }
        }
    }
    bcd_script = bundled_bcd_sys_path();
    if (bcd_script == NULL) {
        event_message("ERROR", "The bundled Windows BCD provisioner is unavailable");
        goto out;
    }
    {
        char *bcd[] = {bcd_script, windows_mount, "--firmware", "uefi",
                       "--syspath", esp_mount, "--clean", NULL};
        event_message("LOG", "Creating Windows Boot Manager and BCD stores");
        /* BCD-SYS normally rejects uid 0 because its interactive command uses
         * sudo only around privileged operations.  Lufus already runs inside
         * its independently validated privileged helper, so the pinned,
         * locally patched bundle permits this one non-interactive invocation. */
        setenv("BCD_SYS_ALLOW_ROOT", "1", 1);
        if (run_command_input(bcd, NULL) != 0) {
            event_message("ERROR", "Could not provision Windows Boot Manager");
            goto out;
        }
        event_percent_progress(99.0);
    }
    if (options.flags != 0) {
        /* Rufus deliberately allocates no OP_PATCH range to Windows To Go.
         * Show its user-options status at the current file-copy position. */
        event_stage_text(progress.copy_end, progress.copy_end,
                         LUFUS_RUFUS_STATUS_APPLY_USER_OPTIONS);
        answer_fd = mkstemp(answer_template);
        if (answer_fd < 0) {
            event_errno("Could not create Windows To Go answer file");
            goto out;
        }
        close(answer_fd);
        answer_fd = -1;
        answer_path = answer_template;
        {
            GError *error = NULL;
            if (!lufus_windows_write_unattend(answer_path, &options, &error)) {
                event_message("ERROR", error != NULL ? error->message :
                              "Could not create Windows To Go answer file");
                g_clear_error(&error);
                goto out;
            }
        }
        panther = g_build_filename(windows_mount, "Windows", "Panther", NULL);
        unattend = g_build_filename(panther, "unattend.xml", NULL);
        if (g_mkdir_with_parents(panther, 0755) != 0 ||
            copy_regular_file(answer_path, unattend, 0644, NULL) != 0) {
            event_errno("Could not install Windows To Go setup options");
            goto out;
        }
    }
    event_percent_progress(100.0);
    event_stage_text(progress.finalize_visible_start, 1000,
                     LUFUS_RUFUS_STATUS_FINALIZE);
    event_percent_progress(0.0);
    if (sync_mounted_filesystem(windows_mount) != 0 ||
        sync_mounted_filesystem(esp_mount) != 0) {
        event_errno("Could not flush the Windows To Go filesystems");
        goto out;
    }
    result = 0;

out:
    if (answer_fd >= 0)
        close(answer_fd);
    if (windows_mounted) {
        char *unmount[] = {"umount", "--", windows_mount, NULL};
        if (run_command_input(unmount, NULL) != 0 && result == 0)
            result = 1;
    }
    if (esp_mounted) {
        char *unmount[] = {"umount", "--", esp_mount, NULL};
        if (run_command_input(unmount, NULL) != 0 && result == 0)
            result = 1;
    }
    if (source_mounted) {
        char *unmount[] = {"umount", "--", source_mount, NULL};
        run_command_input(unmount, NULL);
    }
    if (result == 0) {
        event_percent_progress(100.0);
        event_message("COMPLETE", "Windows To Go media created");
    }
    if (answer_path != NULL)
        unlink(answer_path);
    if (windows_mount != NULL)
        rmdir(windows_mount);
    if (esp_mount != NULL)
        rmdir(esp_mount);
    if (source_mount != NULL)
        rmdir(source_mount);
    g_free(bcd_script);
    g_free(esp_partition);
    g_free(windows_partition);
    g_free(install_image);
    g_free(split_reference);
    g_free(panther);
    g_free(unattend);
    g_free(system_hive);
    g_free(san_policy_tool);
    target_info_clear(&target);
    return result;
}

static int add_persistence_arguments(const char *directory)
{
    DIR *stream = opendir(directory);
    struct dirent *entry;
    if (stream == NULL)
        return -1;
    while ((entry = readdir(stream)) != NULL) {
        char *path;
        struct stat details;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        path = g_build_filename(directory, entry->d_name, NULL);
        if (lstat(path, &details) != 0) {
            g_free(path);
            closedir(stream);
            return -1;
        }
        if (S_ISDIR(details.st_mode)) {
            if (add_persistence_arguments(path) != 0) {
                g_free(path);
                closedir(stream);
                return -1;
            }
        } else if (S_ISREG(details.st_mode) &&
                   (g_str_has_suffix(entry->d_name, ".cfg") ||
                    g_str_has_suffix(entry->d_name, ".conf"))) {
            char *contents = NULL;
            gsize length = 0;
            if (g_file_get_contents(path, &contents, &length, NULL) && length < 4U * 1024U * 1024U) {
                char **lines = g_strsplit(contents, "\n", -1);
                GString *updated = g_string_sized_new(length + 256);
                gboolean changed = FALSE;
                guint line;
                for (line = 0; lines[line] != NULL; line++) {
                    char *value = lines[line];
                    if ((strstr(value, "linux") != NULL || strstr(value, "append") != NULL) &&
                        strstr(value, "/casper/") != NULL && strstr(value, " persistent") == NULL) {
                        char *marker = strstr(value, " ---");
                        if (marker != NULL) {
                            g_string_append_len(updated, value, marker - value);
                            g_string_append(updated, " persistent");
                            g_string_append(updated, marker);
                        } else {
                            g_string_append_printf(updated, "%s persistent", value);
                        }
                        changed = TRUE;
                    } else if ((strstr(value, "linux") != NULL || strstr(value, "append") != NULL) &&
                               strstr(value, "boot=live") != NULL && strstr(value, " persistence") == NULL) {
                        char *marker = strstr(value, "boot=live") + strlen("boot=live");
                        g_string_append_len(updated, value, marker - value);
                        g_string_append(updated, " persistence");
                        g_string_append(updated, marker);
                        changed = TRUE;
                    } else {
                        g_string_append(updated, value);
                    }
                    if (lines[line + 1] != NULL)
                        g_string_append_c(updated, '\n');
                }
                if (changed && !g_file_set_contents(path, updated->str,
                                                    (gssize)updated->len, NULL)) {
                    g_string_free(updated, TRUE);
                    g_strfreev(lines);
                    g_free(contents);
                    g_free(path);
                    closedir(stream);
                    return -1;
                }
                g_string_free(updated, TRUE);
                g_strfreev(lines);
            }
            g_free(contents);
        }
        g_free(path);
    }
    closedir(stream);
    return 0;
}

static int linux_persistent_command(const char *source, const char *target_argument,
                                    const char *expected_identity, uint64_t expected_capacity,
                                    uint64_t persistence_size, bool runtime_validation)
{
    TargetInfo target = {0};
    char source_template[] = "/tmp/lufus-linux-iso.XXXXXX";
    char target_template[] = "/tmp/lufus-linux-target.XXXXXX";
    char persistence_template[] = "/tmp/lufus-persistence.XXXXXX";
    char *source_mount = NULL;
    char *target_mount = NULL;
    char *persistence_mount = NULL;
    char *main_partition = NULL;
    char *persistence_partition = NULL;
    char *loader_partition = NULL;
    char *loader_image = NULL;
    char *partition_script = NULL;
    bool source_mounted = false;
    bool target_mounted = false;
    bool persistence_mounted = false;
    bool uses_casper = false;
    int descriptor = -1;
    int result = 1;
    struct stat source_details;
    uint64_t sectors;
    uint64_t loader_start;
    uint64_t persistence_sectors;
    uint64_t persistence_start;
    uint64_t main_size;
    CopyProgress copy_progress = {0};
    LufusMediaProgressLayout media_progress =
        lufus_media_progress_layout(runtime_validation);
    unsigned int parent_progress_start = operation_progress_start;
    unsigned int parent_progress_end = operation_progress_end;

    if (persistence_size < 256ULL * 1024ULL * 1024ULL ||
        stat(source, &source_details) != 0 || !S_ISREG(source_details.st_mode)) {
        event_message("ERROR", "Persistent Linux mode requires a local ISO and at least 256 MiB");
        return 1;
    }
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount target filesystems");
        goto out;
    }
    source_mount = mkdtemp(source_template);
    target_mount = mkdtemp(target_template);
    persistence_mount = mkdtemp(persistence_template);
    if (source_mount == NULL || target_mount == NULL || persistence_mount == NULL) {
        event_errno("Could not create temporary mount directories");
        goto out;
    }
    set_operation_progress_subrange(parent_progress_start, parent_progress_end,
                                    media_progress.media_start,
                                    media_progress.media_end);
    event_message("STARTED", "Creating persistent Linux media");
    event_stage(0, 50, "Inspecting Linux live media");
    event_indeterminate_progress();
    {
        char *mount_source[] = {"mount", "-o", "loop,ro,unhide", "--", (char *)source, source_mount, NULL};
        if (run_command_input(mount_source, NULL) != 0) {
            event_message("ERROR", "Could not mount the Linux ISO");
            goto out;
        }
        source_mounted = true;
    }
    {
        char *casper = g_build_filename(source_mount, "casper", NULL);
        char *live = g_build_filename(source_mount, "live", NULL);
        uses_casper = access(casper, R_OK) == 0;
        if (!uses_casper && access(live, R_OK) != 0) {
            event_message("ERROR", "ISO is not a supported Ubuntu casper or Debian live image");
            g_free(casper);
            g_free(live);
            goto out;
        }
        g_free(casper);
        g_free(live);
    }
    if (copy_tree_size(source_mount, false, &copy_progress.total, NULL) != 0) {
        event_errno("Could not calculate Linux live-media size");
        goto out;
    }
    sectors = target.capacity / 512;
    persistence_sectors = ((persistence_size + 1048575) / 1048576) * 2048;
    loader_start = (sectors - 34 - 2048) & ~UINT64_C(2047);
    if (loader_start <= persistence_sectors + 4096) {
        event_message("ERROR", "Target is too small for the requested persistence size");
        goto out;
    }
    persistence_start = (loader_start - persistence_sectors) & ~UINT64_C(2047);
    main_size = persistence_start - 2048;
    if (main_size * 512 < 256ULL * 1024ULL * 1024ULL ||
        copy_progress.total > main_size * 512 - 256ULL * 1024ULL * 1024ULL) {
        event_message("ERROR", "Target is too small for the ISO and requested persistence size");
        goto out;
    }
    partition_script = g_strdup_printf(
        "label: gpt\nunit: sectors\nfirst-lba: 2048\n\n"
        "start=2048, size=%" PRIu64 ", type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7, name=LUFUS-LIVE\n"
        "start=%" PRIu64 ", size=%" PRIu64 ", type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name=LUFUS-PERSISTENCE\n"
        "start=%" PRIu64 ", size=2048, type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7, name=UEFI-NTFS\n",
        main_size, persistence_start, persistence_sectors, loader_start);
    {
        char *sfdisk[] = {"sfdisk", "--wipe", "always", target.path, NULL};
        event_stage(50, 120, "Creating persistent-media partitions");
        event_indeterminate_progress();
        if (run_command_input(sfdisk, partition_script) != 0) {
            event_message("ERROR", "Could not create persistent-media partitions");
            goto out;
        }
    }
    descriptor = open(target.path, O_RDONLY | O_CLOEXEC | O_EXCL);
    if (descriptor >= 0) {
        ioctl(descriptor, BLKRRPART, 0);
        close(descriptor);
        descriptor = -1;
    }
    main_partition = find_partition_path(target.path, 1);
    persistence_partition = find_partition_path(target.path, 2);
    loader_partition = find_partition_path(target.path, 3);
    loader_image = bundled_data_path("uefi-ntfs.img");
    if (main_partition == NULL || persistence_partition == NULL ||
        loader_partition == NULL || loader_image == NULL) {
        event_message("ERROR", "Could not locate persistent-media partitions or loader");
        goto out;
    }
    {
        char *format_main[] = {"mkfs.ntfs", "-F", "-Q", "-L", "LUFUS-LIVE", main_partition, NULL};
        char *format_persistence[] = {"mkfs.ext4", "-F", "-L",
                                      uses_casper ? "casper-rw" : "persistence",
                                      persistence_partition, NULL};
        event_stage(120, 200, "Formatting persistent-media filesystems");
        event_indeterminate_progress();
        if (run_command_input(format_main, NULL) != 0 ||
            run_command_input(format_persistence, NULL) != 0) {
            event_message("ERROR", "Could not format persistent-media partitions");
            goto out;
        }
    }
    {
        char *input = g_strdup_printf("if=%s", loader_image);
        char *output = g_strdup_printf("of=%s", loader_partition);
        char *dd[] = {"dd", input, output, "bs=1M", "conv=fsync,notrunc", "status=none", NULL};
        int status = run_command_input(dd, NULL);
        g_free(input);
        g_free(output);
        if (status != 0) {
            event_message("ERROR", "Could not write the UEFI:NTFS loader");
            goto out;
        }
    }
    {
        event_stage(200, 220, "Mounting persistent live-media filesystem");
        event_indeterminate_progress();
        if (mount_target_partition(main_partition, target_mount, target.path, true) != 0) {
            event_message("ERROR", "Could not mount live-media partition");
            goto out;
        }
        target_mounted = true;
    }
    event_stage(220, 820, "Copying Linux live-media files");
    if (copy_progress.total == 0)
        event_indeterminate_progress();
    if (copy_tree(source_mount, target_mount, NULL, NULL, &copy_progress) != 0) {
        event_errno("Could not copy persistent Linux media");
        goto out;
    }
    if (copy_progress_commit_path(&copy_progress, target_mount) != 0) {
        event_errno("Could not commit copied Linux live-media files to the device");
        goto out;
    }
    if (add_persistence_arguments(target_mount) != 0) {
        event_errno("Could not configure persistent Linux media");
        goto out;
    }
    if (!uses_casper) {
        char *configuration;
        event_stage(820, 875, "Configuring Debian persistence");
        event_indeterminate_progress();
        if (mount_target_partition(persistence_partition, persistence_mount,
                                   target.path, false) != 0) {
            event_message("ERROR", "Could not mount Debian persistence partition");
            goto out;
        }
        persistence_mounted = true;
        configuration = g_build_filename(persistence_mount, "persistence.conf", NULL);
        if (!g_file_set_contents(configuration, "/ union\n", -1, NULL)) {
            g_free(configuration);
            event_message("ERROR", "Could not create persistence.conf");
            goto out;
        }
        g_free(configuration);
    }
    if (uses_casper) {
        event_stage(820, 875, "Configuring Ubuntu persistence");
        event_indeterminate_progress();
    }
    event_stage_text(875, 1000, LUFUS_RUFUS_STATUS_FINALIZE);
    event_percent_progress(0.0);
    if (sync_mounted_filesystem(target_mount) != 0 ||
        (persistence_mounted && sync_mounted_filesystem(persistence_mount) != 0)) {
        event_errno("Could not commit persistent Linux media filesystems");
        goto out;
    }
    event_percent_progress(100.0);
    if (runtime_validation) {
        set_operation_progress_subrange(parent_progress_start, parent_progress_end,
                                        media_progress.verification_start,
                                        media_progress.verification_end);
        event_stage_percent(0, 1000, LUFUS_STATUS_VERIFY_MEDIA);
        event_indeterminate_progress();
        if (enable_runtime_validation(target_mount) != 0) {
            event_errno("Could not install runtime UEFI media validation");
            goto out;
        }
    }
    set_operation_progress_subrange(parent_progress_start, parent_progress_end,
                                    media_progress.finalize_start,
                                    media_progress.finalize_end);
    event_stage_text(0, 1000, LUFUS_RUFUS_STATUS_FINALIZE);
    event_percent_progress(0.0);
    if (sync_mounted_filesystem(target_mount) != 0 ||
        (persistence_mounted && sync_mounted_filesystem(persistence_mount) != 0)) {
        event_errno("Could not flush persistent Linux media filesystems");
        goto out;
    }
    result = 0;
out:
    if (persistence_mounted) {
        char *unmount[] = {"umount", "--", persistence_mount, NULL};
        if (run_command_input(unmount, NULL) != 0 && result == 0)
            result = 1;
    }
    if (target_mounted) {
        char *unmount[] = {"umount", "--", target_mount, NULL};
        if (run_command_input(unmount, NULL) != 0 && result == 0)
            result = 1;
    }
    if (source_mounted) {
        char *unmount[] = {"umount", "--", source_mount, NULL};
        run_command_input(unmount, NULL);
    }
    if (result == 0)
        event_message("COMPLETE", "Persistent Linux media created");
    if (persistence_mount != NULL)
        rmdir(persistence_mount);
    if (target_mount != NULL)
        rmdir(target_mount);
    if (source_mount != NULL)
        rmdir(source_mount);
    g_free(main_partition);
    g_free(persistence_partition);
    g_free(loader_partition);
    g_free(loader_image);
    g_free(partition_script);
    target_info_clear(&target);
    operation_progress_start = parent_progress_start;
    operation_progress_end = parent_progress_end;
    return result;
}

static int device_test_command(const char *target_argument, const char *expected_identity,
                               uint64_t expected_capacity, guint passes,
                               bool complete_job)
{
    TargetInfo target = {0};
    guint pass;
    int result = 1;
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount target filesystems");
        goto out;
    }
    event_message("STARTED", "Running destructive fake-drive and bad-block test");
    for (pass = 1; pass <= passes; pass++) {
        char message[128];
        char *probe[] = {"f3probe", "--destructive", "--time-ops", target.path, NULL};
        snprintf(message, sizeof(message), "Fake-drive test pass %u of %u", pass, passes);
        event_message("LOG", message);
        event_stage((pass - 1U) * 1000U / passes, pass * 1000U / passes, message);
        if (run_command_progress(probe) != 0) {
            event_message("ERROR", cancelled ? "Device test cancelled" :
                                             "Device failed the fake-drive/bad-block test");
            goto out;
        }
    }
    result = 0;
    event_message(complete_job ? "COMPLETE" : "LOG",
                  "Device capacity and blocks passed");
out:
    target_info_clear(&target);
    return result;
}

static int capture_command(const char *output, const char *target_argument,
                           const char *expected_identity, uint64_t expected_capacity,
                           const char *format)
{
    TargetInfo target = {0};
    struct stat output_details;
    int source_fd = -1;
    int output_fd = -1;
    int result = 1;
    uint64_t written = 0;
    if (output == NULL || !g_path_is_absolute(output) ||
        (strcmp(format, "raw") != 0 && strcmp(format, "vhd") != 0 &&
         strcmp(format, "vhdx") != 0)) {
        event_message("ERROR", "Invalid capture output or format");
        return 1;
    }
    if (lstat(output, &output_details) == 0 || errno != ENOENT) {
        event_message("ERROR", "Capture output already exists or cannot be inspected");
        return 1;
    }
    if (validate_target(target_argument, expected_identity, expected_capacity, &target) != 0)
        return 1;
    if (unmount_children(target.path) != 0) {
        event_errno("Could not unmount source filesystems for a consistent capture");
        goto out;
    }
    event_message("STARTED", "Capturing device image");
    if (strcmp(format, "raw") == 0) {
        event_stage(0, 930, "Reading source device");
        source_fd = open(target.path, O_RDONLY | O_CLOEXEC | O_EXCL);
        output_fd = open(output, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (source_fd < 0 || output_fd < 0 ||
            copy_image(source_fd, output_fd, target.capacity, target.capacity, &written) != 0) {
            event_errno("Could not capture raw device image");
            goto out;
        }
        event_stage(930, 1000, "Flushing captured image");
        event_indeterminate_progress();
        if (fsync(output_fd) != 0) {
            event_errno("Could not flush captured image");
            goto out;
        }
    } else {
        char *size = g_strdup_printf("%llu", (unsigned long long)target.capacity);
        char *output_format = strcmp(format, "vhd") == 0 ? "vpc" : "vhdx";
        char *options = strcmp(format, "vhd") == 0 ? "subformat=fixed,force_size=on" : "subformat=dynamic";
        char *create[] = {"qemu-img", "create", "-f", output_format, "-o", options,
                          (char *)output, size, NULL};
        char *convert[] = {"qemu-img", "convert", "-p", "-n", "-f", "raw", "-O",
                           output_format, target.path, (char *)output, NULL};
        event_stage(0, 40, "Creating virtual disk container");
        event_indeterminate_progress();
        if (run_command_input(create, NULL) != 0) {
            g_free(size);
            event_message("ERROR", "Could not create virtual disk container");
            goto out;
        }
        event_stage(40, 1000, "Capturing device into virtual disk");
        if (run_command_progress(convert) != 0) {
            g_free(size);
            event_message("ERROR", "Could not capture virtual disk image");
            goto out;
        }
        g_free(size);
    }
    {
        const char *uid_text = g_getenv("PKEXEC_UID");
        if (uid_text != NULL) {
            char *end = NULL;
            unsigned long uid = strtoul(uid_text, &end, 10);
            if (end != uid_text && *end == '\0' && uid <= UINT_MAX)
                if (chown(output, (uid_t)uid, (gid_t)-1) != 0)
                    event_errno("Could not transfer capture ownership to the invoking user");
        }
    }
    event_message("COMPLETE", "Device image captured");
    result = 0;
out:
    if (source_fd >= 0)
        close(source_fd);
    if (output_fd >= 0)
        close(output_fd);
    target_info_clear(&target);
    return result;
}

static void print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage: tuxflash-helper write --source IMAGE --target DEVICE "
            "--major-minor N:N --size BYTES [--disk-seq N] [--verify]\n"
            "       tuxflash-helper verify --source IMAGE --target DEVICE "
            "--major-minor N:N --size BYTES\n"
            "       tuxflash-helper windows-installer --source ISO --target DEVICE "
            "--major-minor N:N --size BYTES --windows-flags N "
            "--architecture ARCH --windows-index N "
            "--target-system uefi|bios|bios-uefi "
            "[--uefi-ntfs] [--runtime-validation] [--local-account NAME]\n"
            "       tuxflash-helper windows-to-go --source ISO --target DEVICE "
            "--major-minor N:N --size BYTES --windows-index N "
            "--windows-flags N --architecture ARCH [--local-account NAME]\n"
            "       tuxflash-helper linux-persistent --source ISO --target DEVICE "
            "--major-minor N:N --size BYTES --persistence-size BYTES "
            "[--runtime-validation]\n"
            "       tuxflash-helper device-test --target DEVICE --major-minor N:N "
            "--size BYTES --passes 1..4\n"
            "       tuxflash-helper capture --target DEVICE --major-minor N:N "
            "--size BYTES --output PATH --capture-format raw|vhd|vhdx\n");
}

int main(int argc, char **argv)
{
    const char *source = NULL;
    const char *target = NULL;
    const char *identity = NULL;
    uint64_t capacity = 0;
    bool verify = false;
    bool uefi_ntfs = false;
    bool runtime_validation = false;
    guint windows_flags = 0;
    const char *architecture = NULL;
    const char *local_account = NULL;
    guint windows_index = 1;
    uint64_t persistence_size = 0;
    guint passes = 0;
    const char *target_system = "uefi";
    const char *capture_format = NULL;
    int index;

    configure_bundled_environment();

    if (argc == 2 && strcmp(argv[1], "--check-tools") == 0)
        return check_tools();
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }
    if (argc < 3 || (strcmp(argv[1], "write") != 0 &&
                     strcmp(argv[1], "verify") != 0 &&
                     strcmp(argv[1], "windows-installer") != 0 &&
                     strcmp(argv[1], "windows-to-go") != 0 &&
                     strcmp(argv[1], "linux-persistent") != 0 &&
                     strcmp(argv[1], "device-test") != 0 &&
                     strcmp(argv[1], "capture") != 0)) {
        print_usage(stderr);
        return 64;
    }
    read_only_operation = strcmp(argv[1], "verify") == 0 || strcmp(argv[1], "capture") == 0;
    for (index = 2; index < argc;) {
        if (strcmp(argv[index], "--verify") == 0) {
            verify = true;
            index++;
            continue;
        }
        if (strcmp(argv[index], "--uefi-ntfs") == 0) {
            uefi_ntfs = true;
            index++;
            continue;
        }
        if (strcmp(argv[index], "--runtime-validation") == 0) {
            runtime_validation = true;
            index++;
            continue;
        }
        if (index + 1 >= argc) {
            print_usage(stderr);
            return 64;
        }
        if (strcmp(argv[index], "--source") == 0)
            source = argv[index + 1];
        else if (strcmp(argv[index], "--output") == 0)
            source = argv[index + 1];
        else if (strcmp(argv[index], "--target") == 0)
            target = argv[index + 1];
        else if (strcmp(argv[index], "--major-minor") == 0)
            identity = argv[index + 1];
        else if (strcmp(argv[index], "--disk-seq") == 0) {
            char *end = NULL;
            errno = 0;
            expected_disk_sequence = strtoull(argv[index + 1], &end, 10);
            if (errno || !g_ascii_isdigit(argv[index + 1][0]) || !end || *end || !expected_disk_sequence) {
                print_usage(stderr); return 64;
            }
        } else if (strcmp(argv[index], "--size") == 0) {
            char *end = NULL;
            errno = 0;
            capacity = strtoull(argv[index + 1], &end, 10);
            if (errno != 0 || !g_ascii_isdigit(argv[index + 1][0]) || end == argv[index + 1] || *end != '\0')
                capacity = 0;
        } else if (strcmp(argv[index], "--windows-flags") == 0) {
            char *end = NULL;
            unsigned long value;
            errno = 0;
            value = strtoul(argv[index + 1], &end, 10);
            if (errno != 0 || end == argv[index + 1] || *end != '\0' || value > UINT_MAX) {
                print_usage(stderr);
                return 64;
            }
            windows_flags = (guint)value;
        } else if (strcmp(argv[index], "--architecture") == 0) {
            architecture = argv[index + 1];
        } else if (strcmp(argv[index], "--local-account") == 0) {
            local_account = argv[index + 1];
        } else if (strcmp(argv[index], "--windows-index") == 0) {
            char *end = NULL;
            unsigned long value;
            errno = 0;
            value = strtoul(argv[index + 1], &end, 10);
            if (errno != 0 || end == argv[index + 1] || *end != '\0' ||
                value == 0 || value > UINT_MAX) {
                print_usage(stderr);
                return 64;
            }
            windows_index = (guint)value;
        } else if (strcmp(argv[index], "--persistence-size") == 0) {
            char *end = NULL;
            errno = 0;
            persistence_size = strtoull(argv[index + 1], &end, 10);
            if (errno != 0 || end == argv[index + 1] || *end != '\0')
                persistence_size = 0;
        } else if (strcmp(argv[index], "--passes") == 0) {
            char *end = NULL;
            unsigned long value;
            errno = 0;
            value = strtoul(argv[index + 1], &end, 10);
            if (errno != 0 || end == argv[index + 1] || *end != '\0' ||
                value < 1 || value > 4) {
                print_usage(stderr);
                return 64;
            }
            passes = (guint)value;
        } else if (strcmp(argv[index], "--target-system") == 0) {
            target_system = argv[index + 1];
        } else if (strcmp(argv[index], "--capture-format") == 0) {
            capture_format = argv[index + 1];
        } else {
            print_usage(stderr);
            return 64;
        }
        index += 2;
    }
    if ((source == NULL && strcmp(argv[1], "device-test") != 0) ||
        target == NULL || identity == NULL || capacity == 0) {
        print_usage(stderr);
        return 64;
    }
    if (uefi_ntfs && strcmp(argv[1], "windows-installer") != 0) {
        print_usage(stderr);
        return 64;
    }
    if (runtime_validation && strcmp(argv[1], "windows-installer") != 0 &&
        strcmp(argv[1], "linux-persistent") != 0) {
        print_usage(stderr);
        return 64;
    }
    if ((strcmp(target_system, "uefi") != 0 && strcmp(target_system, "bios") != 0 &&
         strcmp(target_system, "bios-uefi") != 0) ||
        (strcmp(argv[1], "capture") == 0 && (capture_format == NULL || passes != 0)) ||
        (strcmp(argv[1], "device-test") == 0 && passes == 0) ||
        (strcmp(argv[1], "linux-persistent") == 0 && persistence_size == 0)) {
        print_usage(stderr);
        return 64;
    }
    if (strcmp(argv[1], "verify") == 0 &&
        (passes != 0 || verify || uefi_ntfs || runtime_validation ||
         windows_flags != 0 || capture_format != NULL || persistence_size != 0)) {
        print_usage(stderr);
        return 64;
    }
    const char *basic_tools[] = {"lsblk", "findmnt", "umount", NULL};
    const char *filesystem_tools[] = {"mount", "wipefs", "sfdisk", NULL};
    const char *windows_tools[] = {"wimlib-imagex", "mkfs.vfat", NULL};
    const char *togo_tools[] = {"mkfs.ntfs", "hivexsh", "uuidgen", "bash", "hexdump", "xxd", NULL};
    if (!require_tools(basic_tools))
        return 69;
    if ((strcmp(argv[1], "windows-installer") == 0 ||
         strcmp(argv[1], "windows-to-go") == 0 ||
         strcmp(argv[1], "linux-persistent") == 0) && !require_tools(filesystem_tools))
        return 69;
    if ((strcmp(argv[1], "windows-installer") == 0 ||
         strcmp(argv[1], "windows-to-go") == 0) && !require_tools(windows_tools))
        return 69;
    if (strcmp(argv[1], "windows-to-go") == 0 && !require_tools(togo_tools))
        return 69;
    if (((strcmp(argv[1], "windows-installer") == 0 &&
          (uefi_ntfs || strcmp(target_system, "uefi") != 0)) ||
         strcmp(argv[1], "linux-persistent") == 0) && !tool_available("mkfs.ntfs")) {
        event_message("ERROR", "Required tool is missing: mkfs.ntfs");
        return 69;
    }
    if (strcmp(argv[1], "linux-persistent") == 0 && !tool_available("mkfs.ext4")) {
        event_message("ERROR", "Required tool is missing: mkfs.ext4");
        return 69;
    }
    if (passes > 0 && !tool_available("f3probe")) {
        event_message("ERROR", "Required tool is missing: f3probe");
        return 69;
    }
    if (geteuid() != 0) {
        event_message("ERROR", "The TuxFlash helper must run as root");
        return 77;
    }
    if (install_signal_handlers() != 0) {
        event_errno("Could not install cancellation handlers");
        return 1;
    }
    if (passes > 0 && strcmp(argv[1], "device-test") != 0) {
        operation_progress_start = 0;
        operation_progress_end = 200;
        if (device_test_command(target, identity, capacity, passes, false) != 0)
            return 1;
        operation_progress_start = 200;
        operation_progress_end = 1000;
    } else {
        operation_progress_start = 0;
        operation_progress_end = 1000;
    }
    if (strcmp(argv[1], "verify") == 0)
        return verify_command(source, target, identity, capacity);
    if (strcmp(argv[1], "write") == 0)
        return write_command(source, target, identity, capacity, verify);
    if (strcmp(argv[1], "windows-installer") == 0)
        return windows_installer_command(source, target, identity, capacity,
                                         uefi_ntfs, target_system, runtime_validation,
                                         windows_flags, architecture, local_account,
                                         windows_index);
    if (strcmp(argv[1], "windows-to-go") == 0)
        return windows_to_go_command(source, target, identity, capacity,
                                     windows_flags, architecture, local_account,
                                     windows_index);
    if (strcmp(argv[1], "linux-persistent") == 0)
        return linux_persistent_command(source, target, identity, capacity,
                                        persistence_size, runtime_validation);
    if (strcmp(argv[1], "device-test") == 0)
        return device_test_command(target, identity, capacity, passes, true);
    return capture_command(source, target, identity, capacity, capture_format);
}
