# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise canonical launcher precedence, errors and argument preservation."""
from pathlib import Path
import shutil,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='tuxflash-launchers-') as d:
 project=Path(d)/'project with spaces';project.mkdir()
 for name in ['tuxflash','tuxflash.sh','scripts/run.sh']:
  (project/name).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,project/name)
 launch=project/'tuxflash'
 r=subprocess.run([str(launch)],capture_output=True,text=True)
 assert r.returncode==127 and 'runtime not found' in r.stderr
 local=project/'linux/runtime/bin/tuxflash';local.parent.mkdir(parents=True)
 local.write_text('#!/bin/sh\nprintf "LOCAL|%s|%s" "$1" "$2"\nexit 31\n');local.chmod(0o700)
 r=subprocess.run([str(launch),'one space','--two'],capture_output=True,text=True)
 assert r.returncode==31 and r.stdout=='LOCAL|one space|--two'
 portable=project/'dist/portable/tuxflash';portable.parent.mkdir(parents=True)
 portable.write_text('#!/bin/sh\nprintf "PORTABLE|%s|%s" "$1" "$2"\nexit 32\n');portable.chmod(0o700)
 r=subprocess.run([str(launch),'one space','--two'],capture_output=True,text=True)
 assert r.returncode==32 and r.stdout=='PORTABLE|one space|--two'
 print('PASS: missing runtime, fallback, portable precedence and spaced arguments')
