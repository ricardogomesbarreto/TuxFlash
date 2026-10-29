#!/bin/bash
if [[ ! -f /etc/tuxflash-disposable-qa ]] || [[ $(cat /proc/cmdline) != *tuxflash_qa=1* ]]; then
    echo "This script requires the disposable guest" >&2
    exit 64
fi
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
exec >> /var/log/tuxflash-qa-setup.log 2>&1
trap 'code=$?; echo "POLKIT SETUP FAIL line=$LINENO code=$code"; systemctl poweroff' ERR
echo 'POLKIT SETUP starting' > /dev/ttyS0
depmod -a || true
modprobe xhci-pci || true
modprobe usb-storage
sleep 2
find /var/lib/tuxflash-qa -maxdepth 1 -type f ! -name setup.done -delete
if [ ! -e /var/lib/tuxflash-qa/setup.done ]; then
 # Official mirrors over HTTPS also work on networks that reject plain HTTP.
 sed -i -e 's|http://|https://|g' -e 's|https://archive.ubuntu.com/ubuntu|https://us.archive.ubuntu.com/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources
 apt-get -o Acquire::ForceIPv4=true -o Acquire::Languages=none -o Acquire::Retries=1 -o Acquire::https::Timeout=20 -o APT::Update::Error-Mode=any update -qq
 apt-get -o Acquire::ForceIPv4=true -o Acquire::Languages=none -o Acquire::Retries=1 -o Acquire::https::Timeout=20 install -y --no-install-recommends kbd dbus dbus-user-session polkitd pkexec policykit-1 libpam-systemd xvfb xauth python3
 useradd -m -u 1000 -U -G sudo -s /bin/bash tuxqa
 python3 - <<'PY'
from pathlib import Path
import subprocess,shutil,os
secret=Path('/root/.qa-secret').read_text().strip()
subprocess.run(['chpasswd'],input='tuxqa:'+secret+'\n',text=True,check=True)
p=Path('/home/tuxqa/.qa-secret');p.write_text(secret+'\n');p.chmod(0o600);os.chown(p,1000,1000)
Path('/home/tuxqa/.profile').write_text('exec /usr/local/libexec/qa-user-runner\n')
os.chown('/home/tuxqa/.profile',1000,1000)
PY
 touch /var/lib/tuxflash-qa/setup.done
fi
systemctl daemon-reload
systemctl start dbus.service systemd-logind.service polkit.service
for p in /sys/class/net/*; do n=${p##*/}; [ "$n" = lo ] || ip link set "$n" down; done
chown tuxqa:tuxqa /var/lib/tuxflash-qa
for p in /sys/class/block/sd?; do
 [ -e "$p/size" ] || continue
 if [ "$(cat "$p/size")" = 131072 ]; then echo /dev/${p##*/} > /var/lib/tuxflash-qa/target; fi
done
[ -f /var/lib/tuxflash-qa/target ]
# A caller without a local login must not receive the application's active-session policy.
if setpriv --reuid=1000 --regid=1000 --init-groups pkexec --disable-internal-agent /usr/libexec/tuxflash-helper --help > /var/lib/tuxflash-qa/sessionless.log 2>&1; then
 code=0
else
 code=$?
fi
[ "$code" -ne 0 ]
printf '{"passed":true,"exit_code":%d}\n' "$code" > /var/lib/tuxflash-qa/sessionless.json
echo 'POLKIT SETUP packages ready, guest network link disabled, sessionless caller denied' > /dev/ttyS0
