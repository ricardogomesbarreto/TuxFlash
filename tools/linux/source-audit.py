#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-derived references only: this is not a decompiler or a REA substitute."""
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve()
destination = Path(sys.argv[2]).resolve()
patterns = {
    "windows_headers": r'^\s*#\s*include\s*[<"](?:windows|winioctl|setupapi|vds|virtdisk|wimgapi)\.h[>"]',
    "gui": r'\b(?:WinMain|MyCreateDialog|CreateDialogParamW|SendMessageW|DialogBoxParamW)\s*\(',
    "disk_io": r'\b(?:DeviceIoControl|CreateFileW|CreateFileU|SetFilePointerEx)\s*\(',
    "device_discovery": r'\b(?:SetupDiGetClassDevs[A-Z]*|SetupDiEnumDeviceInfo|SetupDiEnumDeviceInterfaces|CM_Get_Parent)\s*\(',
    "formatting": r'\b(?:pfFormatEx|IVdsVolumeMF3_FormatEx2|CoCreateInstance)\s*\(',
    "wimlib_references": r'\b(?:wimlib_[A-Za-z_]+)\b',
    "virtual_disks": r'\b(?:OpenVirtualDisk|CreateVirtualDisk|AttachVirtualDisk|pfOpenVirtualDisk|pfCreateVirtualDisk|pfAttachVirtualDisk)\b',
    "windows_network": r'\b(?:InternetOpen[A-Z]*|HttpOpenRequest[A-Z]*|InternetReadFile|WinHttp[A-Za-z]+)\s*\(',
}
compiled = {key: re.compile(pattern) for key, pattern in patterns.items()}
files = sorted((root / "upstream/rufus/src").glob("*.c")) + sorted((root / "upstream/rufus/src").glob("*.h"))
references = {key: [] for key in patterns}
for source in files:
    for number, line in enumerate(source.read_text(errors="replace").splitlines(), 1):
        for category, pattern in compiled.items():
            if pattern.search(line):
                references[category].append({
                    "path": str(source.relative_to(root)), "line": number,
                    "text": line.strip(),
                })
report = {
    "method": "lexical source references, not a complete call graph",
    "scope": "upstream/rufus/src/*.c and *.h; third-party trees and new Linux layer excluded",
    "file_count": len(files),
    "counts": {key: len(items) for key, items in references.items()},
    "references": references,
    "source_digests": {
        str(source.relative_to(root)): hashlib.sha256(source.read_bytes()).hexdigest()
        for source in files
    },
}
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(report["counts"], indent=2))
