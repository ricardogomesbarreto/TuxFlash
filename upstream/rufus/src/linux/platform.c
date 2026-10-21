/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"

#include <errno.h>
#include <fcntl.h>
#include "hash_core.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHUNK (1024U * 1024U)

static int open_input(const char *path, struct stat *snapshot)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return -1;
    if (fstat(fd, snapshot) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    if (!S_ISREG(snapshot->st_mode) || snapshot->st_size < 0) {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    return fd;
}

static int unchanged(int fd, const struct stat *before)
{
    struct stat after;
    if (fstat(fd, &after) < 0)
        return -1;
    if (before->st_size != after.st_size ||
        before->st_mtim.tv_sec != after.st_mtim.tv_sec ||
        before->st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before->st_ctim.tv_sec != after.st_ctim.tv_sec ||
        before->st_ctim.tv_nsec != after.st_ctim.tv_nsec) {
        errno = ESTALE;
        return -1;
    }
    return 0;
}

static int report(rf_progress_fn fn, uint64_t done, uint64_t total, void *ctx)
{
    if (fn != NULL && fn(done, total, ctx) != 0) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

/* pread/write can return partial results or EINTR even on regular files. */
static int read_exact(int fd, void *buffer, size_t length, off_t offset)
{
    unsigned char *p = buffer;
    while (length != 0) {
        ssize_t n = pread(fd, p, length, offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0)
                errno = EIO;
            return -1;
        }
        length -= (size_t)n;
        p += n;
        offset += n;
    }
    return 0;
}

static int write_exact(int fd, const void *buffer, size_t length)
{
    const unsigned char *p = buffer;
    while (length != 0) {
        ssize_t n = write(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0)
                errno = EIO;
            return -1;
        }
        length -= (size_t)n;
        p += n;
    }
    return 0;
}

int rf_hash_image(const char *path, struct rf_hashes *out,
                  rf_progress_fn progress, void *context)
{
    HASH_CONTEXT hash[HASH_MAX];
    char *outputs[4];
    struct rf_hashes result = {0};
    struct stat before;
    unsigned char *buffer = NULL;
    uint64_t done = 0;
    int status = -1, fd, saved;
    if (out == NULL) {
        errno = EINVAL;
        return -1;
    }
    fd = open_input(path, &before);
    if (fd < 0)
        return -1;
    outputs[0] = result.md5;
    outputs[1] = result.sha1;
    outputs[2] = result.sha256;
    outputs[3] = result.sha512;
    buffer = malloc(CHUNK);
    if (buffer == NULL)
        goto finish;
    RufusHashInitialize();
    for (size_t i = 0; i < HASH_MAX; i++)
        hash_init[i](&hash[i]);
    if (report(progress, 0, (uint64_t)before.st_size, context) < 0)
        goto finish;
    while (done < (uint64_t)before.st_size) {
        uint64_t remaining = (uint64_t)before.st_size - done;
        size_t n = remaining > CHUNK ? CHUNK : (size_t)remaining;
        if (read_exact(fd, buffer, n, (off_t)done) < 0)
            goto finish;
        for (size_t i = 0; i < HASH_MAX; i++)
            hash_write[i](&hash[i], buffer, n);
        done += n;
        if (report(progress, done, (uint64_t)before.st_size, context) < 0)
            goto finish;
    }
    if (unchanged(fd, &before) < 0)
        goto finish;
    for (size_t i = 0; i < 4; i++) {
        static const char hex[] = "0123456789abcdef";
        unsigned int length = hash_count[i];
        hash_final[i](&hash[i]);
        for (unsigned int j = 0; j < length; j++) {
            outputs[i][2 * j] = hex[hash[i].buf[j] >> 4];
            outputs[i][2 * j + 1] = hex[hash[i].buf[j] & 15];
        }
        outputs[i][2 * length] = '\0';
    }
    status = 0;
finish:
    saved = errno;
    free(buffer);
    if (close(fd) < 0 && status == 0) {
        saved = errno;
        status = -1;
    }
    if (status == 0)
        *out = result;
    errno = saved;
    return status;
}

int rf_inspect_image(const char *path, struct rf_image_info *out)
{
    struct stat before;
    struct rf_image_info result = {0};
    unsigned char b[8];
    int fd, status = -1, saved;
    if (out == NULL) {
        errno = EINVAL;
        return -1;
    }
    fd = open_input(path, &before);
    if (fd < 0)
        return -1;
    result.size = (uint64_t)before.st_size;
    if (result.size >= 512) {
        if (read_exact(fd, b, 2, 510) < 0)
            goto finish;
        result.mbr_signature = b[0] == 0x55 && b[1] == 0xaa;
    }
    if (result.size >= 520) {
        if (read_exact(fd, b, 8, 512) < 0)
            goto finish;
        result.gpt_header_512 = memcmp(b, "EFI PART", 8) == 0;
    }
    if (result.size >= 4104) {
        if (read_exact(fd, b, 8, 4096) < 0)
            goto finish;
        result.gpt_header_4096 = memcmp(b, "EFI PART", 8) == 0;
    }
    /* Only the first ISO9660 descriptor is observed here. This is a signature
     * probe, not full ISO parsing or a claim of bootability/ISOHybrid support. */
    if (result.size >= 32775) {
        if (read_exact(fd, b, 7, 32768) < 0)
            goto finish;
        result.iso9660 = memcmp(b + 1, "CD001", 5) == 0 && b[6] == 1;
    }
    status = unchanged(fd, &before);
finish:
    saved = errno;
    if (close(fd) < 0 && status == 0) {
        saved = errno;
        status = -1;
    }
    if (status == 0)
        *out = result;
    errno = saved;
    return status;
}

int rf_copy_new_image(const char *source, const char *destination,
                      rf_progress_fn progress, void *context)
{
    struct stat before, after;
    unsigned char *buffer = NULL, *verify = NULL;
    uint64_t done = 0;
    int output = -1, status = -1, saved;
    int input = open_input(source, &before);
    if (input < 0)
        return -1;
    if (report(progress, 0, (uint64_t)before.st_size, context) < 0)
        goto finish;
    output = open(destination, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (output < 0)
        goto finish;
    if (fstat(output, &after) < 0)
        goto finish;
    if (!S_ISREG(after.st_mode)) {
        errno = EINVAL;
        goto finish;
    }
    buffer = malloc(CHUNK);
    verify = malloc(CHUNK);
    if (buffer == NULL || verify == NULL)
        goto finish;
    while (done < (uint64_t)before.st_size) {
        uint64_t remaining = (uint64_t)before.st_size - done;
        size_t n = remaining > CHUNK ? CHUNK : (size_t)remaining;
        if (read_exact(input, buffer, n, (off_t)done) < 0 ||
            write_exact(output, buffer, n) < 0)
            goto finish;
        done += n;
        if (report(progress, done, (uint64_t)before.st_size, context) < 0)
            goto finish;
    }
    if (unchanged(input, &before) < 0 || fsync(output) < 0)
        goto finish;
    done = 0;
    while (done < (uint64_t)before.st_size) {
        uint64_t remaining = (uint64_t)before.st_size - done;
        size_t n = remaining > CHUNK ? CHUNK : (size_t)remaining;
        if (read_exact(input, buffer, n, (off_t)done) < 0 ||
            read_exact(output, verify, n, (off_t)done) < 0)
            goto finish;
        if (memcmp(buffer, verify, n) != 0) {
            errno = EIO;
            goto finish;
        }
        done += n;
        if (report(progress, (uint64_t)before.st_size,
                   (uint64_t)before.st_size, context) < 0)
            goto finish;
    }
    if (unchanged(input, &before) < 0 || fstat(output, &after) < 0)
        goto finish;
    if (after.st_size != before.st_size) {
        errno = EIO;
        goto finish;
    }
    status = 0;
finish:
    saved = errno;
    free(buffer);
    free(verify);
    if (output >= 0 && close(output) < 0 && status == 0) {
        saved = errno;
        status = -1;
    }
    if (close(input) < 0 && status == 0) {
        saved = errno;
        status = -1;
    }
    errno = saved;
    return status;
}
