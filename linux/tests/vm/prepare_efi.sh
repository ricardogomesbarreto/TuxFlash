#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
: "${LUFUS_VM_WORK:?Set the disposable work directory}"
gnu_root=${LUFUS_GNU_EFI_ROOT:-/}
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
gcc -c -Os -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fpic "$script_dir/efi_fixture.c" -o "$LUFUS_VM_WORK/efi-fixture.o"
ld -nostdlib -z defs -znocombreloc -T "$gnu_root/usr/lib/elf_x86_64_efi.lds" -shared -Bsymbolic "$gnu_root/usr/lib/crt0-efi-x86_64.o" "$LUFUS_VM_WORK/efi-fixture.o" -L "$gnu_root/usr/lib" -lgnuefi -o "$LUFUS_VM_WORK/efi-fixture.so"
objcopy -j .text -j .sdata -j .rodata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc -O efi-app-x86_64 "$LUFUS_VM_WORK/efi-fixture.so" "$LUFUS_VM_WORK/BOOTX64.EFI"
