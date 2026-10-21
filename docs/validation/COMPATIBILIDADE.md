# Compatibilidade e interface — escopo comprovado

Não foi atingida equivalência integral com o Rufus Windows. A aplicação é o
TuxFlash 0.3.5 nativo (derivado do Lufus 0.2.2), integrado ao núcleo de hashes do Rufus e às correções locais.

## ISOs Linux

Não há garantia para qualquer distribuição ou ISO. O modo DD/RAW copia a imagem
inteira; não converte automaticamente uma ISO apenas óptica em imagem inicializável
por USB. O sucesso da cópia não comprova que o firmware ou o sistema iniciará.
Também dependem da imagem a arquitetura, o suporte a BIOS/UEFI e Secure Boot.

| Caso | Evidência local |
| --- | --- |
| Linux Mint 22.3 MATE x86-64 | Gravação física RAW e duas comparações integrais aprovadas; boot físico pendente |
| Persistência casper e Debian-live | Partições, arquivos e configurações aprovados com ISOs sintéticas; sessão Linux real e persistência após reiniciar pendentes |
| SteamOS recovery, Kali Installer e Zorin OS | Gravação/conferência e etapas de firmware em VM; Zorin persistente corrigido. Veja [imagens reais](IMAGENS-REAIS-2026-10-21.md). Sem recuperação física de Steam Deck ou sessão persistente entre boots |
| Debian 13.7 netinst amd64 | RAW/leitura integral e SHA-256 independente em VM; menus BIOS/UEFI e instalador UEFI na seleção de idioma |
| Ubuntu 26.04.1 Desktop amd64 | RAW/leitura integral e SHA-256 independente em VM; menus BIOS/UEFI, sem qualificação do desktop completo |
| Outras distribuições e ISOs | Sem qualificação física local; detecção pelo probe não equivale a boot aprovado |
| Secure Boot | Sem validação local; os boots registrados usaram Secure Boot desativado |

## Interface e configurações

A janela nativa usa GTK 4/libadwaita e pertence ao Lufus fornecido. Não é a janela
Win32 original do Rufus executando nativamente. Os fontes e recursos Win32
continuam preservados no projeto; preservá-los não significa que sejam usados
pela aplicação GTK.

A comparação inicial de `linux/src/main.c` com o ZIP Lufus identificou três
alterações de integração: localização relativa do helper, localização relativa
dos catálogos e despacho do comando CLI `probe`. Na continuação, o campo de rótulo passou a acompanhar a ISO, a conta padrão
passou a ser genérica e o log passou a usar somente o nome do arquivo. A
organização e o estilo da janela GTK foram mantidos; veja [ROTULOS.md](../guides/ROTULOS.md). A auditoria está em
[`../analysis/linux/lufus/windows-physical-2026-10-20/interface-audit.json`](evidence/linux/lufus/windows-physical-2026-10-20/interface-audit.json).

As configurações não têm equivalência completa com Rufus Windows. Rótulo,
sistema de arquivos, tamanho de cluster, formatação rápida e criação de
rótulo/ícone estendidos estão desabilitados na GUI atual. O rótulo é calculado a partir do nome da ISO para mídias Windows, sem edição
manual. O backend define o layout conforme o modo de gravação; DD preserva a
estrutura da imagem de origem.
A presença visual de opções adicionais não comprova sua implementação. FFU
continua sem backend Linux. Não foi comprovada importação de preferências do
Rufus Windows.

## Windows

A ISO `Windows 10 PRO 2021.iso` foi testada: instalador UEFI/FAT32 com 906 arquivos
comparados e boot até a seleção de idioma em VM. Windows To Go Pro permanece
parcial: iniciou e chegou a “Preparando-se”, sem comprovação de OOBE/desktop.
O pendrive físico recebeu o instalador UEFI/FAT32 e passou na conferência dos
906 arquivos após invalidar o cache. O usuário confirmou o boot físico do instalador; o boot depois da renomeação
não foi repetido. Veja
[TESTE-WINDOWS-FISICO.md](TESTE-WINDOWS-FISICO.md) e
[TESTE-WINDOWS.md](TESTE-WINDOWS.md).

Os relatos recebidos em `PARITY.md` e `E2E.md` não substituem a validação local.
Os testes desta integração estão em `TESTE-FISICO.md`, `TESTE-WINDOWS.md` e
`VALIDACAO.md`, com o escopo de cada etapa indicado.

## Atualização TuxFlash

O nome, cabeçalho, ícone da janela, identificação do processo e metadados de
menu foram atualizados. O campo de particionamento e de sistema de arquivos
agora acompanha o modo escolhido. A opção de conferência está ativa também
para DD/RAW. A opção antiga de correções BIOS fica desabilitada porque não
possuía implementação. O pacote x86-64 inclui carregador glibc e bibliotecas
próprias; isso reduz dependências, sem provar suporte universal a kernels,
desktops ou arquiteturas. Consulte [TUXFLASH.md](../history/TUXFLASH.md).

## Revisão 0.3.3

Proteção de sistema/swap, identificação USB, conclusão/cancelamento e integração
do pacote foram reforçadas. Consulte [revisão e evidências](REVISAO-0.3.3.md).
Isso não altera os limites de boot, firmware e sistemas operacionais acima.

## Revisão 0.3.5

Conferência RAW/comprimida com tamanho exato, tráfego de progresso limitado e
cancelamento de leituras bloqueadas. A revisão de regressão não transforma os
resultados históricos de firmware em qualificação física desta versão.
Veja [evidências e critérios de estabilidade](REVISAO-0.3.5.md).

## Pendrive Windows e novas ISOs em 21/10/2026

906 arquivos do instalador existente, `boot.wim` e BCD conferidos somente para
leitura. A rotina administrativa posterior validou os dois GPTs e o FAT32 e
repetiu a comparação integral após invalidar o cache de blocos, sem alterar
os metadados. Debian e Ubuntu validados em discos virtuais. Veja o
[relatório atualizado e seus limites](USB-E-NOVAS-ISOS-2026-10-21.md).
