# Pendrive físico — 20/10/2026

> Este relatório registra o teste anterior com Mint. Depois, o mesmo pendrive
> recebeu Windows 10 UEFI/FAT32 e passou na conferência de 906 arquivos.
> O estado mais recente está em [TESTE-WINDOWS-FISICO.md](TESTE-WINDOWS-FISICO.md).

**Gravação RAW do Linux Mint e conferência física dos dados aprovadas.**
O usuário autorizou explicitamente apagar e testar o pendrive. A autenticação
administrativa foi concluída pelo `pkexec` do Linux.

## Dispositivo e resultado

| Campo | Valor |
| --- | --- |
| Destino | Mídia USB de teste; identificadores locais removidos |
| Destino | Mídia USB de teste; identificadores locais removidos |
| Destino | Mídia USB de teste; identificadores locais removidos |
| Imagem gravada | Linux Mint 22.3 MATE, 3.134.275.584 bytes |
| Estado final | Conectado, desmontado, contendo a imagem Mint |

A identidade, capacidade, origem e hash do helper foram conferidos antes das
operações. O disco do sistema foi excluído do inventário de destinos. Não foram
alterados permissões de dispositivos, Polkit, sudoers ou arquivos do sistema.

## Operações concluídas

1. Antes do apagamento, os 1.294 arquivos do manifesto existente, totalizando
   3.084.975.100 bytes, passaram em `md5sum --check --quiet md5sum.txt`.
2. Uma passagem `f3probe` aprovou a capacidade anunciada, igual à
   capacidade anunciada, sem necessidade de reset. O probe levou 5 min 59 s.
   **É um teste de capacidade; não é uma varredura completa da superfície.**
3. O aplicativo nativo gravou a ISO Mint em modo RAW com `--verify`; houve
   comparação de toda a imagem, evento COMPLETE e saída 0. O processo completo,
   incluindo autenticação, F3, gravação e primeira conferência, levou 1.136,9 s.
4. O helper atualizado repetiu a leitura dos 3.134.275.584 bytes depois de
   invalidar o cache do dispositivo com `BLKFLSBUF`. A comparação passou em
   98,3 s, sem regravar a imagem.
5. As duas partições reconhecidas pelo kernel coincidiram com os setores e
   tamanhos da ISO. A mídia foi montada somente para leitura: foram conferidos
   os executáveis EFI IA32/x64, GRUB e o carregador BIOS `isolinux.bin`.
   A montagem foi removida ao terminar.

A ISO tem SHA-256
`7609294da613b75eea89bb918292125e9f06418a368136fb190466e15bf8c373`, igual ao
[checksum publicado pelo espelho da distribuição](https://mirrors.edge.kernel.org/linuxmint/stable/22.3/sha256sum.txt).
Tamanho, inode e data de modificação ficaram estáveis durante o cálculo.

O kernel registrou uma tentativa malsucedida de abrir `/dev/sda1` durante a
verificação exclusiva do disco. A montagem posterior somente para leitura
funcionou e foi desmontada. Os logs coletados não registram erros de leitura,
gravação ou de mídia; a causa daquela tentativa isolada não foi determinada.

## Correções entregues e verificadas

O helper agora rejeita fonte encurtada ou ampliada durante a cópia, fluxo vazio
e dados maiores que o destino. Sete casos cobrem esses limites; a fonte curta
reproduziu a falha antes da correção. A verificação RAW/VHD/VHDX exige invalidar
o cache antes da comparação. O novo subcomando interno `lufus-helper verify`
confere RAW/ISO já gravado e recusa opções destrutivas como `--passes`.
Ele não foi adicionado à CLI pública do lançador.

Passaram dez suítes Meson, os dez casos de durabilidade/cópia com ASan/UBSan,
34 verificações do backend e três boots mínimos BIOS/UEFI em QEMU. Esse helper foi inspecionado pelo REA 6.5.0. A interface GTK foi preservada.

A gravação física usou o helper
`d89bf28d660cc4e6bd188d00e29ba6287c2561918a452f7fcf17ff33f912ce5e`.
Depois da conclusão, foi instalado o helper de conferência
`6ad916b5244dbfaa7fcc6d30ed34fdaa0e9e77c4ae95629f638c8212c17af115`,
responsável pela segunda comparação com cache invalidado e pelos 34 casos em VM.
Os hashes distintos estão registrados para não atribuir a gravação ao binário
que só foi instalado posteriormente.

## Limites e próxima validação

Não foi reiniciado o computador para testar o boot físico. A presença de
partições e arquivos de boot não comprova a inicialização pelo firmware.
Secure Boot, persistência entre boots e equivalência integral com Rufus Windows
continuam sem comprovação; FFU não tem backend Linux.

Foi encontrada `Windows 10 PRO 2021.iso` em Downloads, com 4.393.009.152 bytes.
O probe reconheceu instalador Windows, UEFI e suporte a Windows To Go. Seu hash
foi registrado; não foi fornecido um checksum oficial para autenticar a origem.
A preparação e a inicialização em VM foram exercitadas e estão descritas em
[TESTE-WINDOWS.md](TESTE-WINDOWS.md). Essa etapa corrigiu a omissão de arquivos
UDF ocultos. O helper entregue depois dessa correção tem outro hash; as
conferências físicas acima continuam atribuídas aos binários registrados.

## Evidências

Em [`../analysis/linux/lufus/physical-2026-10-20/`](evidence/linux/lufus/physical-2026-10-20):
`physical-execution.json`, `physical-write.log`, `physical-post-verify.json`,
`physical-post-verify.log`, identificação, partições, arquivos de boot e scripts
com as verificações de identidade. Os testes e o REA do helper usado na segunda conferência estão em
`verification/`. A validação do helper entregue está em [TESTE-WINDOWS.md](TESTE-WINDOWS.md). `summary.json` consolida os resultados. Os registros da etapa
anterior continuam em [VALIDACAO.md](VALIDACAO.md).
