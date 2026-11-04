# Qualificação de mídias reais / Real-media qualification

## Português Brasil

`linux/tests/vm/real_media.py` executa o helper sem alterações dentro de uma VM.
O destino é sempre um arquivo regular **novo**, criado em uma pasta vazia.
Não há opção de dispositivo físico. Uma ISO/imagem é copiada e conferida por
SHA-256 antes de entrar em um disco ext4 somente para leitura no guest.
O guest não tem rede nem acesso a discos do computador.

Pré-requisitos de desenvolvimento: Python 3, cp, find, cpio, gzip, mke2fs,
QEMU x86-64, SeaBIOS, kernel Linux e um userspace/initramfs preparado.
O template precisa de USB storage, virtio block, ext4, ferramentas usuais de
montagem/blocos e os módulos de filesystem e NLS dos modos testados. UDF,
ISO9660, NTFS e tabelas de caracteres são carregados explicitamente porque o
guest mínimo não executa udev. O runtime é a pasta `runtime/` do pacote exato
a qualificar. KVM é opcional; TCG é aceito, com tempo maior.

Exemplo com caminhos genéricos (substitua pelos artefatos de teste):

```sh
python3 linux/tests/vm/real_media.py \
  --work /tmp/tuxflash-media-qa-new \
  --image /images/system.img.bz2 \
  --expanded-reference /images/system.img \
  --guest-template /qa/guest \
  --runtime /qa/package/runtime \
  --qemu-root /qa/qemu \
  --kernel /qa/vmlinuz --accel kvm
```

`--expanded-reference` é obrigatória para RAW comprimido: prepare a referência
separadamente. `--expected-sha256` permite conferir um hash obtido de uma fonte
confiável. Um hash calculado localmente comprova igualdade, não autenticidade
do fornecedor. A capacidade padrão é 16 GiB; `--capacity-gib` pode aumentá-la.
Reserve espaço para origem, filesystem de entrada, initramfs e destino.

O roteiro exige saída zero da VM e do helper, exatamente uma conclusão dentro
do log final e nenhuma falha fatal. RAW também exige contagem exata, verificação
automática com invalidação de cache e SHA-256 independente de todos os bytes
úteis do destino. Não compara o espaço livre após o fim da imagem.
Os nove testes em `linux/tests/test_real_media_harness.py` recusam entradas
inseguras, leitura curta, protocolo incompleto/duplicado e falso sucesso.

`--mode windows-installer --windows-index N` prepara mídia Windows. Concluir
esse comando comprova criação pelo helper; **não comprova instalação Windows**.
`--unattended-vm-setup` é uma opção explícita de teste: a mídia resultante solicita
**apagar o Disco 0 ao iniciar Windows Setup**. Ela só deve ser iniciada numa VM
isolada com disco descartável. A opção fica desativada por padrão.

A pasta de trabalho conserva `result.json`, `serial.log` e os discos. Trabalhos
existentes são recusados para preservar evidências. Não publique logs sem remover
caminhos, identificadores e informações locais. Uma execução interrompida ou que
excede o timeout não conta como aprovação. Separar sempre: gravação, comparação,
menu de boot, instalador/sessão completa e reinicialização.

## English

`linux/tests/vm/real_media.py` runs the unmodified helper in a networkless VM.
It only reserves a **new regular target file** in an empty work directory;
there is no physical-device argument. The source copy is SHA-256 checked and
placed on a read-only ext4 input disk. No host disks are passed to the guest.

Development prerequisites: Python 3, cp, find, cpio, gzip, mke2fs, x86-64 QEMU,
SeaBIOS, Linux kernel and a prepared guest userspace. The template needs USB
storage, virtio block, ext4 and the tools/filesystem/NLS modules used by the
selected mode. ISO9660, UDF, NTFS and character tables are loaded explicitly in
the minimal guest. Supply the exact package runtime; KVM is optional, TCG is
slower. The generic command above illustrates the required paths.

Compressed RAW requires an independently expanded reference. Use
`--expected-sha256` to check a digest from a trusted source; local equality is
not vendor authentication. The default target capacity is 16 GiB and can be
increased with `--capacity-gib`. Budget space for input, filesystem, initramfs
and target. Existing work directories are rejected to preserve evidence.

Success requires zero VM/helper exits, exactly one completed helper log and
no fatal record. RAW additionally requires exact byte count, automatic
read-back with cache invalidation and an independent full-payload SHA-256
comparison. Target padding is excluded. Nine guard tests cover unsafe inputs,
short reads, incomplete/duplicate completion and false success.

Windows mode only qualifies helper media creation. Full installation needs a
separate disposable-disk boot test. `--unattended-vm-setup` explicitly enables
media that **erases Windows Disk 0 when booted**; it is off by default and is
strictly for isolated disposable VMs. Keep result/log/disk evidence private
until sanitized. Interrupted and timed-out runs never count as passes.
Distinguish writing, comparison, boot menus, complete sessions/installations
and reboot evidence.
