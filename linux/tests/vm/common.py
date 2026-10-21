# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import os,stat,tempfile
BASE=Path(os.environ['LUFUS_VM_WORK']).resolve()
PROJECT=Path(os.environ.get('LUFUS_VM_PROJECT',str(Path(__file__).resolve().parents[3]))).resolve()
QEMU_ROOT=Path(os.environ['LUFUS_QEMU_ROOT']).resolve()
KERNEL=Path(os.environ['LUFUS_VM_KERNEL']).resolve()
KVER=os.environ['LUFUS_VM_KERNEL_VERSION']
DEVROOT=Path(os.environ.get('LUFUS_VM_DEVROOT','/')).resolve()
if Path(tempfile.gettempdir()).resolve() not in BASE.parents:
    raise ValueError('The disposable VM work directory must be below /tmp')
BASE.mkdir(parents=True,exist_ok=True)
marker=BASE/'.lufus-vm-validation'
if not marker.exists():
    if any(BASE.iterdir()):
        raise ValueError('Use an empty work directory')
    marker.write_text('Disposable Lufus VM validation files only.\n')
def require_virtual_disk(path):
    if path.resolve().parent != BASE or not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError('Only regular disposable disk files inside the work directory are allowed')
