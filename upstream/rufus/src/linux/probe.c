/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t interrupted;

static void cancel(int signum)
{
    (void)signum;
    interrupted = 1;
}

static int progress(uint64_t done, uint64_t total, void *ctx)
{
    (void)done;
    (void)total;
    (void)ctx;
    return interrupted != 0;
}

static void json_string(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p != 0; p++) {
        if (*p == '"' || *p == '\\')
            printf("\\%c", *p);
        else if (*p < 32 || *p >= 127)
            printf("\\u%04x", (unsigned int)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

static int attribute(const char *name, const char *key, char *out, size_t cap)
{
    char path[PATH_MAX];
    FILE *file;
    int n = snprintf(path, sizeof(path), "/sys/class/block/%s/%s", name, key);
    if (n < 0 || (size_t)n >= sizeof(path))
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;
    if (fgets(out, (int)cap, file) == NULL) {
        fclose(file);
        return -1;
    }
    fclose(file);
    out[strcspn(out, "\r\n")] = 0;
    return 0;
}

static int list_devices(void)
{
    DIR *directory = opendir("/sys/class/block");
    struct dirent *entry;
    int first = 1;
    if (directory == NULL)
        return -1;
    puts("{\"mode\":\"read_only_inventory\",\"devices\":[");
    while ((entry = readdir(directory)) != NULL) {
        char sectors[64], removable[16], model[256] = "";
        char partition[32], logical_sector[32];
        char *end;
        unsigned long long count;
        if (entry->d_name[0] == '.' ||
            strncmp(entry->d_name, "loop", 4) == 0 ||
            strncmp(entry->d_name, "ram", 3) == 0 ||
            strncmp(entry->d_name, "zram", 4) == 0 ||
            attribute(entry->d_name, "partition", partition, sizeof(partition)) == 0)
            continue;
        if (attribute(entry->d_name, "size", sectors, sizeof(sectors)) < 0)
            continue;
        errno = 0;
        count = strtoull(sectors, &end, 10);
        if (errno != 0 || *end != 0 || count > UINT64_MAX / 512)
            continue;
        if (!first)
            puts(",");
        first = 0;
        printf("{\"device\":");
        char device[PATH_MAX];
        snprintf(device, sizeof(device), "/dev/%s", entry->d_name);
        json_string(device);
        printf(",\"size_bytes\":%" PRIu64 ",\"removable\":", (uint64_t)count * 512);
        if (attribute(entry->d_name, "removable", removable, sizeof(removable)) < 0)
            printf("null");
        else
            printf("%s", strcmp(removable, "1") == 0 ? "true" : "false");
        printf(",\"logical_sector_bytes\":");
        if (attribute(entry->d_name, "queue/logical_block_size", logical_sector,
                      sizeof(logical_sector)) < 0)
            printf("null");
        else {
            unsigned long value = strtoul(logical_sector, &end, 10);
            if (*end != 0 || value == 0)
                printf("null");
            else
                printf("%lu", value);
        }
        printf(",\"model\":");
        if (attribute(entry->d_name, "device/model", model, sizeof(model)) < 0)
            printf("null");
        else
            json_string(model);
        printf("}");
    }
    puts("\n]}");
    closedir(directory);
    return 0;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "Camada Linux em desenvolvimento; ferramenta de validacao, sem interface Rufus.\n"
        "Uso: %s list | inspect ARQUIVO | hash ARQUIVO | copy-image ORIGEM NOVO_ARQUIVO\n"
        "list apenas consulta sysfs. Os demais comandos aceitam arquivos regulares.\n"
        "copy-image nao sobrescreve destinos e nao grava em dispositivos de bloco.\n",
        program);
}

int main(int argc, char **argv)
{
    int status;
    struct sigaction action = {0};
    action.sa_handler = cancel;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 || sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "list") == 0) {
        status = list_devices();
    } else if (argc == 3 && strcmp(argv[1], "inspect") == 0) {
        struct rf_image_info info;
        status = rf_inspect_image(argv[2], &info);
        if (status == 0)
            printf("{\"size_bytes\":%" PRIu64 ",\"mbr_signature\":%s,"
                   "\"gpt_header_512\":%s,\"gpt_header_4096\":%s,\"iso9660_signature\":%s}\n",
                   info.size, info.mbr_signature ? "true" : "false",
                   info.gpt_header_512 ? "true" : "false",
                   info.gpt_header_4096 ? "true" : "false", info.iso9660 ? "true" : "false");
    } else if (argc == 3 && strcmp(argv[1], "hash") == 0) {
        struct rf_hashes hashes;
        status = rf_hash_image(argv[2], &hashes, progress, NULL);
        if (status == 0)
            printf("{\"md5\":\"%s\",\"sha1\":\"%s\",\"sha256\":\"%s\",\"sha512\":\"%s\"}\n",
                   hashes.md5, hashes.sha1, hashes.sha256, hashes.sha512);
    } else if (argc == 4 && strcmp(argv[1], "copy-image") == 0) {
        status = rf_copy_new_image(argv[2], argv[3], progress, NULL);
        if (status == 0)
            puts("{\"copied\":true,\"byte_comparison\":true,\"fsync\":true}");
    } else {
        usage(argv[0]);
        return argc == 2 && strcmp(argv[1], "--help") == 0 ? 0 : 2;
    }
    if (status < 0) {
        int saved = errno;
        fprintf(stderr, "%s: %s\n", argv[1], strerror(saved));
        return saved == ECANCELED ? 130 : 1;
    }
    return ferror(stdout) ? 1 : 0;
}
