/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hash_core.h"
#include <assert.h>
#include <stdlib.h>

int main(void)
{
    HASH_CONTEXT context;
    unsigned char *one = malloc(1);
    assert(one != NULL);
    *one = 'a';
    for (unsigned type = 0; type < HASH_MAX; type++) {
        hash_init[type](&context);
        /* Each pointer is exactly one accessible byte long. The second write
         * must not read the rest of the algorithm's pending 64/128-byte block. */
        hash_write[type](&context, one, 1);
        hash_write[type](&context, one, 1);
        hash_final[type](&context);
    }
    free(one);
    return 0;
}
