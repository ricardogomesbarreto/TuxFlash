# Rótulos das mídias Windows e privacidade — 20/10/2026

O backend e a GUI agora derivam o rótulo do nome do arquivo ISO, sem a extensão,
em vez de usar o rótulo fixo `LUFUS`. FAT32 usa até 11 caracteres ASCII em
maiúsculas; NTFS usa até 32 caracteres. Acentos são transliterados, caracteres
incompatíveis são normalizados e nomes vazios usam `INSTALL`.

Para `Windows 10 PRO 2021.iso`, o resultado é **WINDOWS 10** em FAT32 e
**Windows 10 PRO 2021** em NTFS. O nome GPT da partição principal acompanha o
rótulo. A partição de sistema EFI do To Go permanece separada. DD/RAW continua
preservando o rótulo e a estrutura que já existem na imagem.

A disposição da interface foi mantida. Seu campo de rótulo agora mostra o valor
calculado; continua sem edição manual. O nome de conta padrão é genérico e o log
da GUI registra apenas o nome do arquivo selecionado.

## Verificações

* Onze suítes Meson aprovadas, incluindo os casos de limite, acentos, extensão,
  nomes vazios e caracteres que poderiam interferir na tabela de partições.
* Trinta e quatro casos do helper real em discos virtuais aprovados, incluindo
  comparação dos rótulos FAT32/NTFS e nomes GPT, instalador BIOS/UEFI e To Go.
* Três boots mínimos de teste aprovados: BIOS, UEFI/FAT32 e layout To Go.
* REA 6.5.0: cabeçalhos, seções e símbolos do helper novo inspecionados.
* GUI real exercitada em servidor gráfico isolado: seleção da ISO Windows,
  mudança FAT32/NTFS, rótulo correspondente, conta genérica e log sem `/caminho/local

O usuário confirmou que o pendrive Windows criado na etapa anterior iniciou e
funcionou corretamente. Esse é um relato de boot físico do instalador, não uma
instalação Windows completa verificada automaticamente nem um teste To Go.

O pendrive existente teve somente seu rótulo FAT32 e nome GPT alterados para
**WINDOWS 10**, com geometria, identificadores de partição e arquivos preservados.
Os 906 arquivos originais (4.387.198.448 bytes) passaram novamente na comparação
SHA-256 após invalidar o cache. O arquivo adicional existente foi preservado.
O FAT32 passou na conferência sem reparos. Os detalhes estão no resumo de
evidências desta etapa.
Não foi feito novo boot físico depois da renomeação.

A mídia física foi preparada pelo helper da etapa anterior e renomeada com os
utilitários do sistema. O novo helper foi validado em VM; não se atribui a ele
uma nova gravação física completa. Windows To Go continua parcialmente validado
com a ISO real; não há equivalência integral com Rufus Windows.

Dados pessoais dos documentos, logs e backups foram removidos; veja
[PRIVACIDADE.md](PRIVACIDADE.md).
