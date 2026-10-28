#!/bin/bash
if [[ ! -f /etc/tuxflash-disposable-qa ]] || [[ $(cat /proc/cmdline) != *tuxflash_qa=1* ]]; then
    echo "This script requires the disposable guest" >&2
    exit 64
fi
set -u
ulimit -c 0
export DISPLAY=:55 GDK_BACKEND=x11 GTK_A11Y=none GSETTINGS_BACKEND=memory LANGUAGE=en LANG=C.UTF-8 LC_ALL=C.UTF-8
export XDG_CONFIG_HOME=/home/tuxqa/.config XDG_CACHE_HOME=/home/tuxqa/.cache
Xvfb :55 -screen 0 1024x768x24 -nolisten tcp -ac > /var/lib/tuxflash-qa/xvfb.log 2>&1 &
xvfb=$!
sleep 1
# Direct ELF invocation preserves /proc/self/exe for the installed helper path.
dbus-run-session -- env QA_LAYOUT=installed LD_LIBRARY_PATH=/opt/tuxflash/runtime/lib GSK_RENDERER=cairo FONTCONFIG_FILE=/opt/tuxflash/runtime/share/tuxflash/fonts/fonts.conf XDG_DATA_DIRS=/opt/tuxflash/runtime/share:/usr/share /usr/bin/tuxflash-polkit-test > /var/lib/tuxflash-qa/user.log 2>&1
code=$?
if [ "$code" -eq 0 ]; then
 QA_LAYOUT=portable dbus-run-session -- /opt/tuxflash/runtime/run-elf tuxflash-gui-test /usr/bin/tuxflash-polkit-test >> /var/lib/tuxflash-qa/user.log 2>&1
 code=$?
fi
printf '%s\n' "$code" > /var/lib/tuxflash-qa/user.exit
kill "$xvfb" 2>/dev/null || true
printf 'done\n' > /var/lib/tuxflash-qa/user.done
exit "$code"
