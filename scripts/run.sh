#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -x "$project_dir/dist/portable/tuxflash" ]; then
    exec "$project_dir/dist/portable/tuxflash" "$@"
fi
runtime_dir="$project_dir/linux/runtime"
if [ ! -x "$runtime_dir/bin/tuxflash" ]; then
    printf '%s\n' "TuxFlash runtime not found. Restore dist/portable or build it with linux/build-local.sh." >&2
    exit 127
fi
# Do not load the IDE's GTK 3/GIO modules or Snap locale tree into this GTK 4 app.
unset GTK_PATH GTK_EXE_PREFIX GTK_IM_MODULE_FILE GIO_MODULE_DIR LOCPATH
unset GSETTINGS_SCHEMA_DIR GDK_PIXBUF_MODULE_FILE GDK_PIXBUF_MODULEDIR
export PATH="$runtime_dir/bin:$runtime_dir/sbin:/usr/sbin:/usr/bin:/sbin:/bin"
export LD_LIBRARY_PATH="$runtime_dir/lib"
export XDG_DATA_DIRS="$runtime_dir/share:/usr/local/share:/usr/share"
exec "$runtime_dir/bin/tuxflash" "$@"
