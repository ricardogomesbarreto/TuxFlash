# Windows real — 20/10/2026

**Instalador UEFI/FAT32 aprovado até a tela de idiomas; Windows To Go parcial.**
A preparação das duas mídias passou em discos virtuais. O To Go iniciou após a
correção do BCD, mas continuava em “Preparando-se” após 15 minutos de observação.
A configuração inicial e o desktop não foram validados; esse limite de tempo
não determina a causa da demora. Não foi feita instalação Windows completa.

O usuário confirmou posteriormente o boot físico funcional do instalador. O
rótulo foi ajustado para WINDOWS 10; veja [ROTULOS.md](../guides/ROTULOS.md).

## Origem e ambiente

`/caminho/das/imagens/Windows 10 PRO 2021.iso`, 4.393.009.152 bytes.
SHA-256: `1119a177f3a4fc8a8fe6c1c1ef59afcf94ed6af0adcff445993ac4fdddd13768`.
A cópia usada nos testes coincidiu com esse hash. Não foi fornecido checksum
Microsoft para autenticar a origem da ISO.

O ESD contém Home, Home Single Language, Education e Pro, x86-64, pt-BR,
build 19041.928. O teste To Go selecionou **Pro, índice 4**. `wimlib-imagex verify`
aprovou `boot.wim` e `install.esd`. Não há tabela de integridade no ESD; a
verificação dos dados passou, mas não autentica seu fornecedor.

QEMU/TCG, rede desligada e arquivos regulares de disco na pasta temporária
`.lufus-windows-validation-20261020` do workspace. Nenhum dispositivo físico
foi conectado aos guests. O instalador usa um disco com a capacidade da mídia de teste, cujos detalhes
locais foram omitidos; o To Go usa 32 GiB. Os boots usam snapshots e Secure Boot desativado.

## Resultados

| Verificação | Resultado |
| --- | --- |
| Probe nativo da ISO real | Windows, EFI, instalador e To Go reconhecidos |
| Conteúdo WIM/ESD | Duas verificações aprovadas |
| Hashes originais Rufus | MD5, SHA-1, SHA-256 e SHA-512 da ISO acima de 4 GiB coincidiram com Python hashlib |
| Instalador UEFI/FAT32 | Criação concluída; 906 arquivos conferidos por SHA-256 após invalidar o cache do disco virtual; fsck aprovado |
| Arquivo oculto `sources/ws.dat` | Incluído e hash igual à extração independente do 7-Zip |
| To Go Pro | ESD aplicado em NTFS; ESP, Boot Manager e BCD preparados; BCD legível e SanPolicy=4 |
| Boot do instalador | WinPE/Windows Setup abriu a seleção de idioma em português (Brasil); instalação não iniciada |
| Boot To Go | Windows 10 Pro iniciou e avançou para Preparando-se, onde permanecia após 15 minutos; configuração inicial e desktop não validados |
| Regressão do helper entregue | 10 suítes Meson, 34 casos em VM e 3 marcas mínimas BIOS/UEFI aprovadas |
| Memória | 10 casos de cópia/durabilidade com ASan/UBSan; LSan indisponível no ambiente isolado |
| REA 6.5.0 | Cabeçalhos, seções e símbolos do helper entregue inspecionados; SHA-256 conferido |

## Correções de UDF e BCD

A montagem UDF padrão expunha 905 dos 906 arquivos do catálogo. O helper copiava
essa árvore e omitira `sources/ws.dat`, de 111 bytes. A comparação somente com
a árvore montada não detectava a omissão. Um teste separado mostrou 905 arquivos
sem `unhide` e 906 com essa opção, cujo comportamento está na
[documentação do kernel](https://docs.kernel.org/filesystems/udf.html).

As três montagens de ISO do helper agora incluem `unhide`. Um novo instalador
foi criado sem montagem prévia da ISO, garantindo que a própria aplicação
ativou a opção; depois os 906 arquivos foram comparados com a origem completa.
Também foi corrigida a lista de GUIDs gerada pelo BCD-SYS. Antes, o hive era
legível, mas `displayorder` continha 40 bytes ASCII no valor REG_MULTI_SZ; o Boot
Manager real rejeitou-o com `0xc0000098`. Agora a lista ocupa 80 bytes UTF-16LE,
referencia o carregador padrão existente, cujo objeto tem tipo Windows OS Loader.
A regressão em VM confere essas relações, além de abrir o hive. O BCD do To Go
real foi regenerado pelo componente corrigido, sem reaplicar o ESD, e o boot foi
repetido. O patch reproduzível está em `patches/bcd-sys-guid-list.patch`.

Nenhuma mudança foi feita na interface GTK.

O helper usado nessa etapa tem SHA-256 `afb9b9543bda34330c622e512ee57d741586470a877ffa3649543f19c7d1e157`.
O To Go real foi preparado anteriormente pelo helper `6ad916b5244dbfaa7fcc6d30ed34fdaa0e9e77c4ae95629f638c8212c17af115`;
o BCD desse disco foi depois regenerado pelo BCD-SYS corrigido. A correção do
helper altera a visibilidade de arquivos da ISO. O novo helper
passou também no caso To Go sintético da regressão. Os logs identificam cada
binário, sem atribuir a preparação anterior ao binário novo.

## Escopo ainda pendente

Em continuação, o pendrive físico recebeu este instalador Windows UEFI/FAT32:
906 arquivos passaram na leitura após invalidar o cache, sem divergências.
Consulte [TESTE-WINDOWS-FISICO.md](TESTE-WINDOWS-FISICO.md). Os testes com Mint
em [TESTE-FISICO.md](TESTE-FISICO.md) registram a mídia anterior. O boot físico do instalador foi confirmado posteriormente pelo usuário. Faltam instalação completa em outro disco, desktop
To Go com reinicialização/persistência, variantes reais BIOS/NTFS e Secure Boot.
FFU continua sem backend Linux; não há equivalência integral com Rufus Windows.

Evidências, comandos, scripts e capturas estão em
[`../analysis/linux/lufus/windows-2026-10-20/`](evidence/linux/lufus/windows-2026-10-20).
Os scripts documentam o ambiente local e seus caminhos; exigem os runtimes,
kernel e ferramentas indicados para repetição.
