# Windows no pendrive físico — 20/10/2026

**Criação e leitura integral do instalador Windows 10 aprovadas. Boot físico confirmado posteriormente pelo usuário.**

O pendrive de teste, cujos identificadores locais foram removidos,
foi apagado com a autorização já recebida do usuário. O instalador substituiu
os dados Linux Mint usados no teste físico anterior.

A aplicação nativa, pela CLI e pelo mesmo helper da GUI, preparou um instalador
**UEFI/GPT/FAT32** a partir de `Windows 10 PRO 2021.iso`, SHA-256
`1119a177f3a4fc8a8fe6c1c1ef59afcf94ed6af0adcff445993ac4fdddd13768`.
A origem coincidiu novamente com esse hash antes da gravação. Foi usado o helper
`afb9b9543bda34330c622e512ee57d741586470a877ffa3649543f19c7d1e157`.

Não foram habilitadas personalizações Windows nem instalação automática que
apague o disco de destino. A opção de validação de mídia em tempo de boot foi
desativada neste caso para conferir os arquivos originais, sem adicionar um
carregador intermediário. Essa escolha está no comando registrado e não altera
as opções padrão da aplicação. Portanto, o resultado não qualifica essa opção
ligada em mídia física.

Após a gravação, as partições do USB foram desmontadas, o cache de blocos foi
invalidado com BLKFLSBUF e `fsck.vfat -n` passou. O FAT32 foi montado somente para
leitura. Foram conferidos **906 arquivos e 4.387.198.448 bytes**, sem ausências,
extras ou divergências de tamanho/SHA-256, contra extração independente do
7-Zip. Isso inclui `sources/ws.dat`, oculto no UDF, cuja omissão foi corrigida
na etapa anterior. O volume foi desmontado ao fim.

O usuário confirmou posteriormente que o boot físico do instalador funcionou.
Não foi comprovada uma instalação Windows completa. Depois desse relato, o
rótulo do volume e o nome GPT foram ajustados para WINDOWS 10 sem reformatar;
consulte [ROTULOS.md](../guides/ROTULOS.md). O boot do instalador em VM já havia alcançado a seleção de
idioma, conforme [TESTE-WINDOWS.md](TESTE-WINDOWS.md). Este teste não qualifica
BIOS/NTFS, Secure Boot ou Windows To Go no USB físico.

Comandos, manifesto por arquivo, logs, estado final e auditoria da interface:
[`../analysis/linux/lufus/windows-physical-2026-10-20/`](evidence/linux/lufus/windows-physical-2026-10-20).

## Nova conferência em 21/10/2026

O conteúdo do pendrive existente foi novamente comparado: **906 arquivos e
4.387.198.448 bytes**, sem ausências ou divergências de tamanho/SHA-256.
O rótulo continua **WINDOWS 10**. Há um arquivo adicional de metadados de
indexação do Windows; seu identificador não foi publicado. A verificação
interna de `boot.wim` e a leitura dos dois BCD também passaram.

Esta comparação usou montagem somente leitura e descarte de cache por arquivo;
não implica `BLKFLSBUF` ou nova gravação física com o helper 0.3.5. Os detalhes
da rotina administrativa adicional e das novas imagens Debian/Ubuntu estão no
[relatório de 21/10](USB-E-NOVAS-ISOS-2026-10-21.md).
