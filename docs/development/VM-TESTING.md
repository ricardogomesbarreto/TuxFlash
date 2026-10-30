# Validação em VM descartável

Este harness roda o **helper nativo real**, como root somente dentro de um guest
Linux iniciado por QEMU/TCG. A linha de execução conecta exclusivamente três
arquivos regulares sob `/tmp`: 64 MiB, 1 GiB e 8 GiB. Não aceita um dispositivo
físico como argumento e desabilita a rede do guest. Execute os scripts do host
como usuário normal, sem `sudo`.

São 65 verificações de gravação, leitura, captura, restauração, recusas de
segurança, cancelamento, teste de blocos, layouts de persistência e preparação
de instaladores, incluindo regressões de perda da interface, entrada de comandos, grupos, ferramentas resistentes e oito fixtures de captura de metadados. O teste Windows To Go usa um WIM sintético com arquivos EFI e
hives mínimos; **não contém Windows nem demonstra uma instalação Windows**.
Os testes de firmware executam pequenos programas criados para retornar uma
marca serial, sem iniciar um sistema operacional. Secure Boot fica desativado.

## Dependências

Python 3, BusyBox estático, GCC/binutils, GNU-EFI, QEMU x86, SeaBIOS, OVMF,
`cpio`, `depmod`, `modprobe`, `zstd`, `xorriso`, util-linux, coreutils, dosfstools,
e2fsprogs, ntfs-3g, gzip, xz e bzip2. É preciso um kernel x86-64 legível e seus
módulos correspondentes em `/lib/modules`. O kernel usado aqui tinha virtio,
SCSI, ext4, vfat e controladores USB integrados.

O runtime preparado do aplicativo deve existir e `linux/build-local.sh` deve
ter produzido `build/native/lufus-reg-import`, usado para gerar hives
sintéticos. Bibliotecas adicionais do desenvolvimento podem ser fornecidas
via `LUFUS_VM_DEVROOT`. Os fixtures `test-helper-io`, `test-helper-lifecycle` e `test-helper-metadata`
são copiados de `build/native`; use `TUXFLASH_BUILD_DIR` para outro build.

## Repetir

Na raiz do projeto, ajuste os caminhos conforme o ambiente:

```bash
export LUFUS_VM_WORK="$(mktemp -d /tmp/lufus-vm.XXXXXX)"
export LUFUS_VM_PROJECT="$PWD"
export LUFUS_QEMU_ROOT=/       # /usr/bin/qemu-system-x86_64 e /usr/share/OVMF
export LUFUS_GNU_EFI_ROOT=/    # /usr/lib/elf_x86_64_efi.lds
export LUFUS_VM_KERNEL=/caminho/vmlinuz-legivel
export LUFUS_VM_KERNEL_VERSION=versao-exata-dos-modulos
# Se necessário: export LUFUS_VM_DEVROOT=/caminho/pacotes-extraidos

python3 linux/tests/vm/prepare_guest.py
cp linux/tests/vm/guest-init.sh "$LUFUS_VM_WORK/guest/init"
bash linux/tests/vm/prepare_efi.sh
python3 linux/tests/vm/prepare_fixtures.py
python3 linux/tests/vm/run_guest.py
python3 linux/tests/vm/boot_check.py
```

O diretório de trabalho precisa começar vazio; recebe um marcador antes da
criação dos arquivos de teste. Kernels com outras opções podem exigir ajustes
na lista de módulos do preparador. O harness atual destina-se a Linux x86-64
com a disposição de ferramentas Debian/Ubuntu usada nesta validação.

`run_guest.py` retorna falha se faltar o resumo ou houver qualquer reprovação.
`boot_check.py` exige a marca serial e o código 33 do dispositivo de saída QEMU.
Os registros ficam no diretório temporário. Não apague essa pasta antes de
revisar `serial.log` e `boot-results.json`.

A atualização 0.3.3 acrescenta recusas por diferença de 512 bytes na capacidade,
sequência de disco divergente, somente leitura, montagem abaixo de /home e swap
ativo, além de uma gravação com a sequência correta.

## Polkit em SO completo / Full-OS Polkit

A bateria de 45 casos acima executa o helper diretamente como root. A [qualificação Polkit](POLKIT-VM.md) é separada: conta comum, sessão real, `pkexec`, PAM, GUI e helper, com 11 cenários GUI e recusa sem sessão. Ela não é iniciada por `make test`.

The 45-case backend battery runs the helper directly as root. The linked full-OS Polkit fixture separately tests an ordinary user, real session/authorization, GUI callbacks and helper. It is opt-in and not executed by `make test`.
