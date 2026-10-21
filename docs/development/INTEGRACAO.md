Atualização 0.3.2: notas da versão e textos legais traduzidos nos quatro idiomas.
Veja [notas da versão](../releases/NOTAS-DA-VERSAO.md) e [avisos de terceiros](../legal/THIRD_PARTY_NOTICES.md).
Os registros abaixo incluem o histórico das versões anteriores.

# Atualização TuxFlash 0.3.1

Créditos de Ricardo Barreto com [GitHub](https://github.com/ricardogomesbarreto)
e escolha de Português (Brasil), English, 中文（简体） e Español.
Clique no ícone de idioma do cabeçalho, salve e reabra a aplicação.
Veja [idiomas e créditos](../guides/IDIOMAS.md). Os registros abaixo descrevem a versão anterior.

# Integração TuxFlash 0.3.0

O nome atual da aplicação é **TuxFlash — Bootable USB Creator for Linux**.
O ícone enviado foi incorporado sem alterar seus pixels. Use `./tuxflash.sh`
na raiz do projeto; `python3 instalar-menu.py` registra o ícone no menu do usuário.
O atalho pode ser removido com `python3 instalar-menu.py --remove`.

O pacote `linux/portable/` contém bibliotecas próprias e é relocável para
Linux x86-64. A CLI `doctor` confere 25 ferramentas. A autenticação continua
sob controle do Polkit do sistema, sem regras que concedam privilégios automáticos.
Os controles ainda sem implementação ficam desativados. Não foi comprovada
paridade total com Rufus Windows nem compatibilidade com todas as distribuições.

As validações desta versão estão em [TUXFLASH.md](../history/TUXFLASH.md).
O restante desta página registra as etapas anteriores, com seus limites.

# Integração local do Lufus com o Rufus

Esta pasta contém a aplicação nativa **Lufus 0.2.2**, integrada aos fontes do
`rufus-master`. O Lufus é um derivado independente do Rufus; não é um lançamento
oficial de Rufus para Linux nem uma implementação comprovada de todas as funções
da versão Windows.

Consulte [COMPATIBILIDADE.md](../validation/COMPATIBILIDADE.md) para o suporte efetivamente
validado e as diferenças de interface e configurações em relação ao Rufus Windows.

## Continuação: rótulo e privacidade

O usuário confirmou o boot físico do instalador Windows. A partição foi
renomeada para WINDOWS 10. Novas mídias Windows recebem o nome derivado da ISO,
com o limite de FAT32/NTFS, e o campo da GUI acompanha esse valor. O layout da
janela foi mantido. Veja [ROTULOS.md](../guides/ROTULOS.md).

Os registros e backups foram anonimizados: [PRIVACIDADE.md](../guides/PRIVACIDADE.md).

## Executar

Na raiz de `rufus-master`:

```sh
./executar-linux.sh
./executar-linux.sh --help
./executar-linux.sh list
./executar-linux.sh checksum --image /caminho/imagem.iso
./executar-linux.sh probe --image /caminho/imagem.iso
```

O executável GTK, a CLI e o helper estão em `linux/runtime/`. A GUI permanece
sem privilégios; ao iniciar uma operação que precisa deles, chama o helper com
o `pkexec` do sistema. Não foi instalado serviço, regra de autorização automática,
pacote, repositório ou arquivo em `/usr` durante esta integração.

O build foi compilado e executado em ambiente de validação Linux x86-64, usando as
bibliotecas GTK 4/libadwaita do sistema. Não é um pacote universal para qualquer
distribuição ou arquitetura. A interface utilizada é a GTK do Lufus fornecido;
os recursos Win32 originais do Rufus continuam preservados no projeto. A janela
GTK não é uma reprodução idêntica da janela Win32.

## Alterações realizadas

* Integração de `linux/` do ZIP fornecido, com fontes, interface GTK e backend.
* Hashes da GUI/CLI conectados ao núcleo original `src/hash.c`, portado nas
  etapas anteriores, em vez dos hashes GLib do Lufus recebido.
* Localização relativa do helper e dos catálogos, para executar da pasta local.
* Lançador que evita carregar módulos GTK/GIO, configurações e locales do
  VS Code Snap dentro da aplicação GTK 4 do sistema. Essas variáveis são
  ajustadas somente no processo iniciado pelo lançador.
* Comando `probe` para inspecionar as capacidades de uma ISO sem montá-la.
* Correção do scanner que aceitava um arquivo de texto como ISO vazia através
  do fallback `xorriso`. Agora exige descritor ISO9660/UDF antes da análise.
* Seleção do 7-Zip disponível no `PATH` local, eliminando a dependência do caminho
  fixo `/usr/bin/7z`, que não existe no ambiente de validação.
* Componentes UEFI:NTFS e uefi-md5sum copiados dos recursos já fornecidos.
* BCD-SYS na revisão indicada pelo Lufus, com bases BCD geradas pelo importador C,
  UUIDs via `uuidgen` e temporários separados por execução. Os fontes originais
  estão em `third_party/bcd-sys`; a cópia preparada está em `runtime/libexec/bcd-sys`.
* Ferramentas auxiliares e bibliotecas que faltavam, extraídas de pacotes oficiais
  Ubuntu em área isolada. Os avisos de licença permanecem em `runtime/share/doc`.
  O leitor `peres` conserva o binário do pacote e usa um lançador com a configuração
  de plugins local que o próprio leitor suporta; não foi modificado o leitor PE.
* Inspeção do tamanho VHD/VHDX pelo campo JSON de nível superior do QEMU,
  com formato explícito; o leitor anterior confundia o tamanho do arquivo filho
  com a capacidade do disco virtual.
* Captura VHD com `force_size=on`, preservando a capacidade exata para restaurar
  no mesmo dispositivo. Conversão para dispositivo existente com `-n`.
* BCD-SYS preparado consulta o sistema de arquivos diretamente com `blkid -p`,
  evitando metadados ausentes/desatualizados do `lsblk` após formatar.
* `download --kind uefi-shell --url-only` retorna a URL sem criar arquivo.
* 37 catálogos originais compilados. Textos adicionais sem tradução no catálogo
  recebido continuam em inglês.

## Continuação com pendrive físico

O pendrive de teste foi apagado após autorização do usuário. O F3 aprovou a
capacidade anunciada da mídia; os identificadores locais foram removidos. O aplicativo gravou
o Linux Mint 22.3 MATE e conferiu toda a imagem. Uma segunda comparação passou
após invalidar o cache do dispositivo. Partições e arquivos BIOS/UEFI foram
conferidos, e naquele momento o pendrive ficou desmontado com Mint. Não foi feito
boot físico. Em continuação, esse USB recebeu o instalador Windows 10 UEFI/FAT32,
com os 906 arquivos comparados após invalidar o cache, sem divergências.
O estado atual é Windows, desmontado: [TESTE-WINDOWS-FISICO.md](../validation/TESTE-WINDOWS-FISICO.md).

Foram corrigidos os limites de cópia e a conferência RAW/VHD/VHDX passou a
exigir invalidação de cache. Passaram sete novos casos de limite, dez suítes,
ASan/UBSan, 34 verificações do backend e três boots mínimos em VM. A interface
foi preservada. Consulte [TESTE-FISICO.md](../validation/TESTE-FISICO.md).

A ISO `Windows 10 PRO 2021.iso` foi validada em VM isolada. O instalador
UEFI/FAT32 passou na conferência dos 906 arquivos. Windows To Go Pro foi aplicado
e configurado em disco virtual de 32 GiB, e os boots foram observados em VM.
A omissão de arquivos UDF ocultos foi corrigida com `unhide` nas montagens do
helper. O BCD-SYS também foi corrigido para gerar listas de GUIDs em UTF-16,
após a falha encontrada no boot real. Consulte [TESTE-WINDOWS.md](../validation/TESTE-WINDOWS.md)
para o escopo e as capturas.

## Validação feita nesta sessão

* Compilação nativa da GUI, CLI, helper e ferramentas C auxiliares.
* Dez suítes Meson aprovadas: helper-help, windows-options, downloads, progress,
  helper-durability, job-policy, licenses, image-probe, checksums-rufus-core
  e helper-images (incluindo contêineres QEMU reais).
* Regressão para arquivos falsos `.iso`, incluindo texto e arquivo zerado.
* Comparação dos quatro hashes da CLI com implementações independentes.
* Leitura da ISO real do Linux Mint 22.3: EFI e suporte a persistência detectados;
  não classificada como instalador Windows.
* Inventário `list --all` sem expor o disco interno como destino elegível.
* Verificação das bases BCD com hivex e do leitor PE com o Rufus 4.15 fornecido.
* Abertura e captura da janela GTK em servidor gráfico isolado, com catálogos
  em português. As mensagens DRI3 restantes são do servidor gráfico virtual.
* Inspeção estática dos executáveis ELF da aplicação e do helper pelo REA 6.5.0,
  com SHA-256 e IDs de evidência. Não houve descompilação completa do `.exe` Windows.

A validação sintética atual reúne **34 verificações do helper real em
QEMU**, usando exclusivamente arquivos de disco descartáveis sob `/tmp`, e
**três boots com programas mínimos de teste**: BIOS, UEFI/FAT32 e UEFI pelo
layout Windows To Go. Gravação RAW/comprimida, captura/restauração RAW/VHD/VHDX,
proteções, cancelamento, teste de blocos, partições persistentes, personalização
WIM, BCD e política SAN passaram. Secure Boot ficou desativado.

A consulta real do catálogo Microsoft retornou os idiomas disponíveis. O download
real da ISO UEFI Shell 26H1 passou pela conferência SHA-256 fixada no aplicativo;
os quatro hashes também coincidiram com o Python. A opção `--url-only` foi
verificada sem criar arquivo.

Os registros anteriores continuam em `../analysis/linux/lufus/`. Os novos logs,
resumos, inspeções REA e comparações estão em
`../analysis/linux/lufus/validation-2026-10-20/`. Consulte
[VALIDACAO.md](../validation/VALIDACAO.md) para o escopo exato e
[tests/vm/README.md](VM-TESTING.md) para repetir a VM.

## O que permanece sem comprovação local

A gravação e a conferência física do Mint passaram, mas ainda não foi feito
boot físico do Mint. Nenhum disco físico foi conectado
às VMs: os testes anteriores usam arquivos descartáveis de 64 MiB, 1 GiB e 8 GiB.
A nova VM Windows também usa somente arquivos de disco e mantém a rede desligada.

A preparação Windows real e os boots iniciais foram exercitados em VM, conforme
[TESTE-WINDOWS.md](../validation/TESTE-WINDOWS.md). Isso não comprova instalação completa em
outro disco, desktop To Go com reinicialização, variantes reais BIOS/NTFS,
Linux com persistência entre sessões ou Secure Boot. O boot do instalador Windows
no USB físico foi confirmado pelo usuário, sem instalação completa comprovada.
Os testes não abrangem todos os firmwares ou mídias USB.

Os relatos adicionais de `E2E.md` e a matriz recebida `PARITY.md` pertencem ao projeto recebido e não devem ser
confundidos com as verificações reproduzidas e registradas nesta integração.

FFU continua sem backend Linux; a igualdade integral com o Rufus Windows não foi
atingida. Consulte `PARITY.md` para as demais limitações e opções desabilitadas
pelo Lufus. A organização e o estilo da interface GTK recebida foram mantidos; o campo de
rótulo e os padrões de privacidade receberam os ajustes descritos em ROTULOS.md.

## Recompilar

Com as dependências de desenvolvimento instaladas pelo gerenciador da sua
distribuição — Meson, Ninja, pkg-config, GTK 4, libadwaita, libsoup 3, JSON-GLib,
GLib/GIO e hivex — use:

```sh
make -f Makefile.linux native
```

O script compila, executa os testes e instala os três programas nativos na
subpasta `runtime`; mantém os recursos de boot e utilitários já preparados.
Ele não instala arquivos no sistema. As bibliotecas de desenvolvimento usadas
nesta sessão foram extraídas em `/tmp/lufus-integration/sysroot`; enquanto essa
pasta existir, é possível repetir o build isolado com:

```sh
make -f Makefile.linux native SYSROOT=/tmp/lufus-integration/sysroot
```

Para recompilar os hashes e seus testes isolados, os comandos anteriores
`make -f Makefile.linux check` e `check-sanitize` continuam disponíveis.

Os direitos e licenças dos projetos originais foram preservados. A origem dos
arquivos e versões auxiliares consta em `../analysis/linux/lufus/provenance.json`.

## TuxFlash 0.3.3 — revisão de segurança e documentação

[Revisão desta etapa](../validation/REVISAO-0.3.3.md): proteção de sistema/swap, tamanho exato,
sequência USB quando disponível, revalidação antes de ferramentas, tratamento
de conclusão/cancelamento, BCD-SYS, políticas Polkit e pacote seletivo.
16 suítes, 40 cenários virtuais, 3 boots mínimos e 12 cenários gráficos aprovados.
Leitura física de 1 GiB repetida sem divergências; nenhuma gravação física nova.
A gravação da ISO Linux Mint aguarda aprovação explícita do payload conforme
a revisão automática. Leia os README atualizados e os limites de compatibilidade.
