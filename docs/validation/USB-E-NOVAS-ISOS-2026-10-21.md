# Pendrive Windows 10 e novas ISOs — 21/10/2026

## Português (Brasil)

**Instalador Windows 10 existente preservado e conferido. Debian 13.7 e Ubuntu
26.04.1 passaram na gravação e leitura integral em discos virtuais com o helper
TuxFlash 0.3.5.** Esta revisão acrescenta evidência; não altera o código do aplicativo.

### Pendrive físico

| Teste | Resultado |
| --- | --- |
| Descoberta pela aplicação | USB removível de 15,5 GB identificado |
| Rótulo / estrutura observada pelo sistema | WINDOWS 10 / GPT com uma partição FAT32 |
| Comparação com a extração de referência | 906 arquivos, 4.387.198.448 bytes, nenhum ausente ou divergente de tamanho/SHA-256 |
| Arquivo adicional | `System Volume Information/IndexerVolumeGuid`, 76 bytes; conteúdo/identificador não publicado |
| `boot.wim` | Verificação interna pelo wimlib aprovada; cerca de 1017 MiB de dados descomprimidos conferidos |
| `install.esd` | Metadados e catálogo de imagens lidos com sucesso |
| BCD BIOS e UEFI | Dois hives lidos em modo somente leitura, nove objetos em cada um |
| Gravação / apagamento do USB | Não realizados |

A comparação inicial usou a montagem FAT32 **somente leitura**, autorizada pelo
serviço de discos da sessão. Foram solicitados descartes de cache por arquivo
com `POSIX_FADV_DONTNEED`; isso não equivale a comprovar invalidação do cache de
blocos com `BLKFLSBUF`. A ISO de referência coincidiu novamente com o SHA-256
histórico `1119a177f3a4fc8a8fe6c1c1ef59afcf94ed6af0adcff445993ac4fdddd13768`.
O manifesto vem da extração independente de 7-Zip registrada na validação anterior.

A conferência administrativa também passou: GPT primário e de backup com CRC válido, tabelas de partições iguais, `fsck.vfat -n` com saída zero, cache de blocos invalidado e nova comparação dos 906 arquivos. Os metadados GPT/boot permaneceram iguais. A tentativa inicial de boot em VM encerrou ao selecionar o driver de disco do QEMU; a configuração foi corrigida e a retomada somente leitura está separada das verificações já aprovadas.

A mídia física foi criada em uma versão anterior e seu boot físico já havia
sido confirmado pelo usuário. Esta conferência valida o conteúdo existente;
nenhuma nova instalação Windows no SSD do computador foi iniciada.

### Novas imagens

| Imagem | Tamanho | Escrita / leitura / SHA-256 independente | Firmware em VM |
| --- | ---: | --- | --- |
| Debian 13.7.0 netinst amd64 | 792.723.456 bytes | Aprovados | Menus BIOS e UEFI; instalador gráfico UEFI na seleção de idioma |
| Ubuntu 26.04.1 Desktop amd64 | 6.482.409.472 bytes | Aprovados | Menus GRUB BIOS e UEFI; desktop completo não qualificado |

As imagens coincidiram com os SHA-256 publicados por
[Debian](https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/SHA256SUMS) e
[Ubuntu](https://releases.ubuntu.com/26.04.1/SHA256SUMS), obtidos por HTTPS.
As assinaturas PGP desses arquivos não foram verificadas nesta execução.
Os valores exatos e resultados do probe estão em [new-images.json](evidence/linux/tuxflash/usb-and-isos-2026-10-21/new-images.json).

O helper real escreveu cada imagem em um **arquivo de disco virtual de 16 GiB**,
fez flush e conferência com invalidação de cache dentro do guest. Uma segunda
comparação SHA-256 do payload foi executada independentemente no host.
Os boots seguintes usaram snapshots desses arquivos, sem discos internos do
host nem rede. Secure Boot estava desligado. Não foi feita instalação completa,
sessão persistente ou teste físico de gravação de Debian/Ubuntu.

O probe reconheceu Debian netinst sem persistência e Ubuntu como imagem com
persistência disponível. Isso descreve a capacidade detectada; não comprova
preservação de dados de uma sessão Ubuntu após reiniciar.

### Capturas

![Debian BIOS](evidence/linux/tuxflash/usb-and-isos-2026-10-21/debian-bios-8s.png)
![Instalador Debian em UEFI](evidence/linux/tuxflash/usb-and-isos-2026-10-21/debian-uefi-installer-language.png)
![Ubuntu BIOS](evidence/linux/tuxflash/usb-and-isos-2026-10-21/ubuntu-bios-8s.png)
![Ubuntu UEFI](evidence/linux/tuxflash/usb-and-isos-2026-10-21/ubuntu-uefi-8s.png)

[Resultado estruturado](evidence/linux/tuxflash/usb-and-isos-2026-10-21/validation.json).
Os registros públicos omitem caminhos pessoais, números de série, nomes do
computador e valores dos identificadores de volume.

## English

The existing Windows 10 USB installer was preserved. All **906 original files
(4,387,198,448 bytes)** match the independent extraction reference by size and
SHA-256. The only additional file is a 76-byte Windows indexing metadata file;
its contents/identifier are not published. Internal `boot.wim` verification,
`install.esd` metadata reading and read-only parsing of both nine-object BCD
stores passed. The volume label remains **WINDOWS 10**.

The initial file comparison used a read-only desktop mount and per-file cache
discard advisories; it does not by itself prove block-cache invalidation.
Administrative checks also passed: primary/backup GPT CRCs and matching arrays, zero-exit read-only FAT check, block-cache invalidation and another complete 906-file comparison. GPT/boot metadata remained unchanged. The first VM boot attempt stopped at QEMU disk-driver selection; the configuration was corrected, and the read-only retry is tracked separately from completed checks.

Debian 13.7 netinst and Ubuntu 26.04.1 Desktop match their official HTTPS
checksum lists. Both passed full RAW writing, helper read-back and independent
payload SHA-256 comparisons using regular virtual disk files. BIOS/UEFI menus
were visually confirmed; Debian’s UEFI graphical installer reached language
selection. Ubuntu’s complete live desktop was not qualified. Network and host
internal disks were absent; Secure Boot was off. No full installation,
persistence across reboots or physical Linux USB write was performed.

The Windows USB was created by an earlier version; this is fresh validation of
its existing contents, not a new physical write with 0.3.5. No application-code
changes were needed by these tests. See the linked evidence and screenshots.
