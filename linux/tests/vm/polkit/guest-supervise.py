#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Guest-only ordinary-user subreaper: observe the real root helper after GUI death."""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time

if (not Path('/etc/tuxflash-disposable-qa').is_file()
        or 'tuxflash_qa=1' not in Path('/proc/cmdline').read_text()
        or os.getuid() != 1000):
    print('This script requires the disposable guest', file=sys.stderr)
    raise SystemExit(64)

work = Path('/var/lib/tuxflash-qa')
layout = sys.argv[1]
if layout not in ('installed', 'portable'):
    raise SystemExit(64)
libc = ctypes.CDLL(None, use_errno=True)
# Root descendants orphaned by the GUI become our children. Waiting for an
# actual child is allowed across UIDs; the user does not signal a root process.
if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
    raise OSError(ctypes.get_errno(), 'Could not become a child subreaper')
env = dict(os.environ, QA_LAYOUT=layout)
if layout == 'installed':
    env.update(LD_LIBRARY_PATH='/opt/tuxflash/runtime/lib', GSK_RENDERER='cairo',
               FONTCONFIG_FILE='/opt/tuxflash/runtime/share/tuxflash/fonts/fonts.conf',
               XDG_DATA_DIRS='/opt/tuxflash/runtime/share:/usr/share')
    command = ['/usr/bin/tuxflash-polkit-test']
else:
    command = ['/opt/tuxflash/runtime/run-elf', 'tuxflash-gui-test', '/usr/bin/tuxflash-polkit-test']
process = subprocess.Popen(command, env=env)
result = process.wait(timeout=700)
if result != 0:
    raise SystemExit(result)
number = 12 if layout == 'installed' else 13
info = json.loads((work / f'{number:02d}-detached.json').read_text())
pid = info.pop('helper_pid')
if pid <= 1:
    raise RuntimeError('Invalid helper child identity')
deadline = time.monotonic() + 30
status = None
while time.monotonic() < deadline:
    waited, value = os.waitpid(pid, os.WNOHANG)
    if waited == pid:
        status = value
        break
    time.sleep(.01)
if status is None:
    raise RuntimeError('Root helper kept running after abrupt frontend exit')
info.update(helper_exit_code=os.waitstatus_to_exitcode(status),
            helper_signal=os.WTERMSIG(status) if os.WIFSIGNALED(status) else 0,
            abrupt_frontend_exit=True, completed=False, passed=False)
info['passed'] = (os.WIFEXITED(status) and os.WEXITSTATUS(status) == 1
                  and info['pam_authenticated'] > 0)
(work / f'{number:02d}-result.json').write_text(json.dumps(info) + '\n')
(work / f'{number:02d}-after').write_text('ready\n')
if not info['passed']:
    raise RuntimeError('Helper died by signal or did not cancel normally')
while not (work / f'{number:02d}-after.ready').exists():
    if time.monotonic() >= deadline + 60:
        raise TimeoutError('Root audit did not acknowledge frontend-exit case')
    time.sleep(.01)
print('POLKIT SUPERVISOR PASS', number, flush=True)
