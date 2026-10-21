# SPDX-License-Identifier: GPL-3.0-or-later
"""A failed BCD generator must stop provisioning before an empty hive update."""
from pathlib import Path
import os,re,subprocess,tempfile
source=Path(__file__).resolve().parents[1]/'runtime/libexec/bcd-sys/bcd-sys.sh'
s=source.read_text();function=re.search(r'build_stores \(\) \{\n.*?\n\}',s,re.S).group(0)
with tempfile.TemporaryDirectory(prefix='tuxflash-bcd-failure-') as d:
 base=Path(d);scripts=base/'resources with spaces';scripts.mkdir();(scripts/'winload.sh').write_text('exit 19\n')
 marker=base/'unexpected-update';tool=base/'hivexsh';tool.write_text('#!/bin/sh\ntouch "$UNEXPECTED_BCD_UPDATE"\n');tool.chmod(0o700)
 env=dict(os.environ,resdir=str(scripts),tmpdir=str(base),UNEXPECTED_BCD_UPDATE=str(marker),PATH=str(base)+':'+os.environ['PATH'])
 result=subprocess.run(['bash','-c',function+'\nbuild_stores a b uefi d e f g h i j k l'],env=env,capture_output=True,text=True)
 assert result.returncode!=0,'A failed generator was reported as success'
 assert not marker.exists(),'Failed generator output was used to update a BCD store'
 print('PASS: failed generator stops BCD provisioning')
