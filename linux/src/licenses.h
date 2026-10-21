/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stddef.h>

typedef enum {
    LUFUS_LEGAL_UPSTREAM,
    LUFUS_LEGAL_BUNDLED,
    LUFUS_LEGAL_SYSTEM,
} LufusLegalScope;

typedef struct {
    const char *title;
    const char *copyright;
    const char *license;
    LufusLegalScope scope;
} LufusLegalComponent;

const LufusLegalComponent *lufus_legal_components(size_t *count);

const char *lufus_project_license_notice(void);
