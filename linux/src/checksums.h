/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <gio/gio.h>

char *lufus_checksums_file(const char *path, GCancellable *cancellable, GError **error);
