/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int cancel_after_data(uint64_t done, uint64_t total, void *context)
{
    (void)total;
    (void)context;
    return done != 0;
}

static int cancel_immediately(uint64_t done, uint64_t total, void *context)
{
    (void)done;
    (void)total;
    (void)context;
    return 1;
}

static int change_source(uint64_t done, uint64_t total, void *context)
{
    (void)total;
    if (done == 0) {
        int fd = open((char *)context, O_WRONLY | O_APPEND);
        assert(fd >= 0);
        assert(write(fd, "changed", 7) == 7);
        assert(close(fd) == 0);
    }
    return 0;
}

int main(void)
{
    char directory[] = "/tmp/rufus-linux-test-XXXXXX";
    char input[256], output[256];
    struct rf_hashes result, sentinel;
    struct stat state;
    int fd;
    assert(mkdtemp(directory) != NULL);
    assert(snprintf(input, sizeof(input), "%s/input.img", directory) > 0);
    assert(snprintf(output, sizeof(output), "%s/output.img", directory) > 0);
    fd = open(input, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0);
    /* Multi-chunk sparse source exercises cancellation after partial I/O. */
    assert(ftruncate(fd, 2 * 1024 * 1024 + 13) == 0);
    assert(close(fd) == 0);
    memset(&result, 0x5a, sizeof(result));
    sentinel = result;
    errno = 0;
    assert(rf_hash_image(input, &result, cancel_after_data, NULL) == -1);
    assert(errno == ECANCELED);
    assert(memcmp(&result, &sentinel, sizeof(result)) == 0);
    assert(rf_copy_new_image(input, output, cancel_immediately, NULL) == -1);
    assert(errno == ECANCELED);
    assert(stat(output, &state) == -1 && errno == ENOENT);
    assert(rf_copy_new_image(input, output, cancel_after_data, NULL) == -1);
    assert(errno == ECANCELED);
    assert(stat(output, &state) == 0);
    assert(state.st_size > 0 && state.st_size < 2 * 1024 * 1024 + 13);
    assert((state.st_mode & 0777) == 0600);
    assert(unlink(output) == 0);
    assert(rf_hash_image(input, &result, change_source, input) == -1);
    assert(errno == ESTALE);
    assert(memcmp(&result, &sentinel, sizeof(result)) == 0);
    assert(rf_copy_new_image(input, output, change_source, input) == -1);
    assert(errno == ESTALE);
    assert(unlink(output) == 0);
    assert(unlink(input) == 0);
    assert(rmdir(directory) == 0);
    puts("C: cancellation and concurrent source modification checks passed");
    return 0;
}
