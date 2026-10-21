A validação atual do TuxFlash está em [TUXFLASH.md](../history/TUXFLASH.md).

# Validação local — 20/10/2026

A continuação mais recente inclui onze suítes, rótulos automáticos e anonimização:
[ROTULOS.md](../guides/ROTULOS.md) e [PRIVACIDADE.md](../guides/PRIVACIDADE.md).

Continuação com gravação e conferência física aprovadas, cache invalidado e 34 casos em VM:
[TESTE-FISICO.md](TESTE-FISICO.md). Os testes com a ISO Windows real estão em [TESTE-WINDOWS.md](TESTE-WINDOWS.md).
Os registros desta página correspondem à
etapa anterior; as evidências do helper atual estão no relatório de continuação.

**10 suítes Meson, 31 verificações do backend e três boots de teste aprovados.**
As correções estão compiladas em `runtime/`. Os fontes da interface GTK
`src/main.c` e os recursos visuais não foram alterados nesta etapa.

## Resultados

| Área | Verificação executada | Resultado |
| --- | --- | --- |
| Compilação | GUI/CLI, helper, ferramentas e dez suítes Meson | Aprovado |
| Núcleo Rufus | Plataforma C, sete testes do probe, cinco grupos de hashes e memória com ASan/UBSan | Aprovado |
| Gravação | RAW, gzip, xz, bzip2 e zstd em dispositivo USB emulado, com leitura de conferência | Aprovado |
| Recusas | Identidade/capacidade incorretas, montagem protegida, partição, tamanho excedido, gzip corrompido/truncado, FFU, captura sobre arquivo existente | Aprovado |
| Interrupção | SIGTERM interrompe a gravação e não produz evento COMPLETE | Aprovado |
| Blocos | Quatro passagens e fluxo f3probe no disco virtual de 64 MiB | Aprovado |
| Captura | RAW, VHD e VHDX; comparação e restauração no mesmo disco virtual | Aprovado |
| Persistência | Layouts casper e Debian, NTFS/ext4, argumentos de boot, `persistence.conf`, rótulos e e2fsck | Aprovado com ISOs sintéticas |
| Instalador Windows | GPT/FAT32, GPT/NTFS com UEFI:NTFS, MBR/NTFS e híbrido | Aprovado com ISO sintética |
| Personalização | Injeção em boot.wim índice 2, bypass, conta local, resposta OEM | Aprovado com WIM sintético |
| Windows To Go | Aplicação WIM em NTFS, ESP, BCD legível, SanPolicy=4 e arquivos de boot | Aprovado com WIM/hives sintéticos |
| Validação de mídia | Backup do carregador, geração do manifesto e conferência de todos os MD5 | Aprovado |
| Firmware | Marca BIOS no RAW e marca EFI no FAT32 e no layout To Go | Três boots aprovados; Secure Boot desativado |
| Rede | Catálogo Microsoft real e download UEFI Shell oficial com SHA-256 fixado | Aprovado |
| CLI | UEFI Shell `--url-only` com caminho de saída fornecido | URL retornada; nenhum arquivo criado |
| Inventário | Disco interno excluído de `list --all` | Aprovado |
| REA 6.5.0 | Cabeçalhos, seções e símbolos dos dois executáveis ELF atuais | Inspeções completas; hashes conferidos |

O download UEFI Shell tem SHA-256
`2802f4bbd87be88f366d3fe1556a7cfaf41102e3b167224b69a261391ada8ccf`.
Os quatro hashes calculados pelo aplicativo coincidiram com o Python.

## Falhas corrigidas

1. O leitor de capacidade VHDX procurava a primeira ocorrência textual de
   `virtual-size` no JSON do QEMU. Esse campo podia pertencer ao arquivo filho.
   Agora lê a propriedade inteira positiva do objeto raiz com JSON-GLib.
2. VHD fixo arredondava a capacidade pela geometria CHS, podendo impedir a
   restauração no mesmo dispositivo. A captura preserva o tamanho exato;
   inspeção/conversão especificam o formato e não recriam o destino existente.
3. BCD-SYS podia receber FSTYPE vazio logo após formatar. A cópia preparada usa
   leitura direta com `blkid -p`, sem depender do cache de inventário.
4. A CLI ignorava `--url-only` para UEFI Shell. Agora retorna a URL e não inicia
   download, mesmo quando recebe `--output`.

A nova suíte `helper-images` verifica JSON com campos filhos, dados inválidos,
capacidade acima de 4 GiB e contêineres VHD/VHDX reais. O QEMU estava disponível
na execução registrada; esse subteste não foi pulado.

## Escopo e pendências

QEMU 10.2.1/TCG iniciou Linux 7.0.0-38 com initramfs próprio. Foram conectados
somente três arquivos regulares sob `/tmp` — 64 MiB, 1 GiB e 8 GiB — como USBs.
Não houve acesso de gravação a dispositivo físico, instalação de pacote ou
alteração do boot do computador. O sistema de arquivos do guest incluiu os
módulos gconv e o helper mount.ntfs presentes no host; faltá-los causou erros no
ambiente mínimo inicial, corrigidos antes da execução final.

As ISOs casper/Debian não contêm kernel de sistema operacional. A ISO de formato
Windows contém arquivos próprios, WIMs mínimos e hives de teste. O programa EFI
apenas imprime uma marca serial e encerra QEMU; o teste To Go confirma a cadeia
ESP/arquivo EFI, sem executar o carregador Windows ou interpretar seu BCD.
O teste de blocos em emulação não comprova a capacidade de um pendrive físico.

**Permanecem pendentes:** USB físico e seu boot, instalação Windows completa,
Windows To Go real, persistência Linux entre sessões e Secure Boot. Não foi
encontrada ISO Windows completa em `/caminho/das/imagens`; a ISO
completa disponível é a do Linux Mint, anteriormente inspecionada. FFU continua
sem backend e esta integração não demonstra paridade integral com Rufus Windows
nem portabilidade para todas as distribuições/arquiteturas Linux.

## Evidências e repetição

Os logs estão em
[`../analysis/linux/lufus/validation-2026-10-20/`](evidence/linux/lufus/validation-2026-10-20):
`summary.json`, `serial.log`, `boot-results.json`, logs de compilação/testes,
resultados CLI/rede, alterações e inspeções REA. Os logs anteriores às correções
foram preservados como históricos de falha; o resultado vigente é `summary.json`.

O harness executado está em [tests/vm/README.md](../development/VM-TESTING.md).
`E2E.md` e `PARITY.md` conservam relatos do projeto recebido, que têm escopo
mais amplo que esta reprodução local. Os testes não foram usados para afirmar
que a engenharia reversa do executável Windows está completa.
