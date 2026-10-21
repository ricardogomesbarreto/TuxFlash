/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RUFUS_LINUX_HASH_COMPAT_H
#define RUFUS_LINUX_HASH_COMPAT_H

/* Compile the original hash algorithms without the Windows UI and I/O code.
 * BOOL is the return type of their existing public functions; no Windows API
 * is emulated here. This build currently supports little-endian Linux only. */
#if !defined(__linux__) || !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The portable Rufus hash build currently requires little-endian Linux"
#endif

#include <pthread.h>
#include <string.h>

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#endif

typedef int BOOL;
#define FALSE 0
#define TRUE 1

/* memcpy avoids undefined behavior when a caller supplies unaligned input. */
static inline uint32_t read_swap32(const void *p)
{
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return __builtin_bswap32(value);
}

static inline uint64_t read_swap64(const void *p)
{
    uint64_t value;
    memcpy(&value, p, sizeof(value));
    return __builtin_bswap64(value);
}

static inline void write_swap32(void *p, uint32_t value)
{
    value = __builtin_bswap32(value);
    memcpy(p, &value, sizeof(value));
}

static inline void write_swap64(void *p, uint64_t value)
{
    value = __builtin_bswap64(value);
    memcpy(p, &value, sizeof(value));
}

/* The Windows macro prefetches beyond the current block. Omit the hint in this
 * initial build; it is not required for the algorithm or its acceleration. */
#define PREFETCH64(pointer) ((void)0)
#endif
