#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Rebuild the native programs; keep the staged boot assets and auxiliary tools.
set -euo pipefail
linux_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
meson_cmd=(meson)
if [[ $# == 2 && $1 == --sysroot ]]; then
    dev_root=$(realpath -- "$2")
    export PATH="$dev_root/usr/bin:$PATH"
    export PYTHONPATH="$dev_root/usr/lib/python3/dist-packages${PYTHONPATH:+:$PYTHONPATH}"
    export PKG_CONFIG_SYSROOT_DIR="$dev_root"
    export PKG_CONFIG_LIBDIR="$dev_root/usr/lib/x86_64-linux-gnu/pkgconfig:$dev_root/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
    export C_INCLUDE_PATH="$dev_root/usr/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
    export LIBRARY_PATH="$dev_root/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
    export LD_LIBRARY_PATH="$dev_root/usr/lib/x86_64-linux-gnu"
    meson_cmd=(python3 "$dev_root/usr/bin/meson")
elif [[ $# != 0 ]]; then
    echo "Usage: $0 [--sysroot EXTRACTED_DEVELOPMENT_PACKAGES]" >&2
    exit 2
fi
project_dir=$(CDPATH= cd -- "$linux_dir/.." && pwd)
build_dir="${TUXFLASH_BUILD_DIR:-$project_dir/build/native}"
if [[ -f "$build_dir/build.ninja" ]]; then
    "${meson_cmd[@]}" setup --reconfigure "$build_dir" "$linux_dir"
else
    "${meson_cmd[@]}" setup "$build_dir" "$linux_dir" --prefix=/usr --libexecdir=libexec --buildtype=release
fi
"${meson_cmd[@]}" compile -C "$build_dir" -j 4
"${meson_cmd[@]}" test -C "$build_dir" --no-rebuild --print-errorlogs
"${meson_cmd[@]}" install -C "$build_dir" --no-rebuild --destdir "$build_dir/stage"
mkdir -p "$linux_dir/runtime/bin" "$linux_dir/runtime/libexec/bcd-sys"
cp "$build_dir/stage/usr/bin/tuxflash" "$linux_dir/runtime/bin/tuxflash"
cp "$build_dir/stage/usr/libexec/tuxflash-helper" "$linux_dir/runtime/libexec/tuxflash-helper"
cp "$build_dir/stage/usr/libexec/bcd-sys/set-windows-san-policy" "$linux_dir/runtime/libexec/bcd-sys/set-windows-san-policy"
# Compatibility names are retained for existing automation and test fixtures.
cp "$linux_dir/runtime/bin/tuxflash" "$linux_dir/runtime/bin/lufus"
cp "$linux_dir/runtime/libexec/tuxflash-helper" "$linux_dir/runtime/libexec/lufus-helper"
mkdir -p "$linux_dir/runtime/share/applications" "$linux_dir/runtime/share/metainfo" "$linux_dir/runtime/share/icons/hicolor/512x512/apps" "$linux_dir/runtime/share/doc/tuxflash"
cp "$linux_dir/packaging/org.tuxflash.TuxFlash.desktop" "$linux_dir/runtime/share/applications/"
cp "$linux_dir/packaging/org.tuxflash.TuxFlash.metainfo.xml" "$linux_dir/runtime/share/metainfo/"
cp "$linux_dir/assets/icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png" "$linux_dir/runtime/share/icons/hicolor/512x512/apps/"
cp "$project_dir/docs/legal/THIRD_PARTY_NOTICES.md" "$project_dir/docs/legal/UPSTREAM-LICENSE.txt" "$linux_dir/runtime/share/doc/tuxflash/"
for language in pt_BR en zh_CN es; do
    mkdir -p "$linux_dir/runtime/share/locale/$language/LC_MESSAGES"
    for domain in tuxflash libadwaita gtk40 glib20; do
        catalog="$build_dir/stage/usr/share/locale/$language/LC_MESSAGES/$domain.mo"
        if [[ -f "$catalog" ]]; then cp "$catalog" "$linux_dir/runtime/share/locale/$language/LC_MESSAGES/"; fi
    done
done
echo "Native programs rebuilt in $linux_dir/runtime"
