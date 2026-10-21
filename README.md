<p align="center">
  <img src="linux/assets/icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png" width="190" alt="TuxFlash — pinguim com pendrive azul / penguin holding a blue USB drive">
</p>

# TuxFlash

**Criador de pendrives inicializáveis nativo para Linux**<br>
**Native bootable USB creator for Linux**

Versão / Version **0.3.4** · **GPL-3.0-or-later** · **Linux x86-64**

[![Build and tests](https://github.com/ricardogomesbarreto/TuxFlash/actions/workflows/ci.yml/badge.svg)](https://github.com/ricardogomesbarreto/TuxFlash/actions/workflows/ci.yml)

[GitHub](https://github.com/ricardogomesbarreto/TuxFlash) · [Downloads](https://github.com/ricardogomesbarreto/TuxFlash/releases) · [Contribuir / Contributing](docs/development/CONTRIBUTING.md)

[Português (Brasil)](#portugues-brasil) · [English](#english) · [Estrutura / Layout](#layout)

<a id="layout"></a>
## Organização / Repository layout

```text
TuxFlash/
├── README.md               # Documentação principal PT-BR / English
├── LICENSE.txt             # Licença da aplicação / Application license
├── SECURITY.md             # Relato de problemas / Reporting issues
├── Makefile                # Compilação e testes / Build and checks
├── tuxflash.sh             # Entrada principal / Main launcher
├── scripts/                # Execução e atalho do menu / Run and menu helpers
├── linux/                  # Aplicação nativa, recursos, testes e runtime
│   ├── src/                # Interface, CLI e helper / GUI, CLI and helper
│   ├── tests/              # Testes nativos e VM / Native and VM tests
│   ├── assets/             # Ícone e identidade / Icon and branding
│   ├── po/                 # Catálogos de tradução / Translation catalogs
│   ├── packaging/          # Desktop, Polkit e metadados / Packaging metadata
│   ├── runtime/            # Runtime local e componentes / Local runtime
│   ├── third_party/        # Componentes e avisos preservados / Vendored components
│   └── tools/              # Empacotamento e utilitários / Packaging tools
├── upstream/rufus/         # Fontes e recursos herdados / Inherited source and resources
├── tests/linux/            # Verificação independente de hashes / Hash-core checks
├── tools/linux/            # Análise REA e ferramentas de desenvolvimento
├── docs/                   # Guias, licenças, histórico e evidências
│   ├── guides/             # Uso e configuração / Usage and configuration
│   ├── development/        # Integração, idiomas e VM / Developer guides
│   ├── legal/              # Licenças e proveniência / Licenses and provenance
│   ├── releases/           # Notas da versão / Release notes
│   ├── history/            # Relatórios históricos / Historical reports
│   └── validation/         # Escopo, evidências e integridade / Validation evidence
├── build/                  # Saídas locais de compilação / Local build outputs
├── dist/                   # Pacote atual e portátil / Current archive and portable app
└── archive/                # Backups e pacotes antigos / Backups and older packages
```

Os arquivos de componentes mantêm os avisos e conteúdos originais em documentos
com nomes próprios. Há um único `README.md` do projeto. `build/`, `dist/` e
`archive/` ficam separados dos fontes e são ignorados pelo Git.

Component notices and original documentation are preserved under descriptive
filenames. This is the project's single `README.md`. Generated outputs and
local archives are separated from source and excluded from Git.

<a id="portugues-brasil"></a>
## Português (Brasil)

Transforme imagens ISO em pendrives inicializáveis com simplicidade e segurança.
Aplicação C/GTK 4/libadwaita e CLI, livre sob GPL-3.0-or-later.
Desenvolvimento desta versão: [Ricardo Barreto](https://github.com/ricardogomesbarreto) e colaboradores.

### Instalação e código-fonte

Baixe o pacote **Linux x86-64** e seu SHA-256 na página de
[lançamentos](https://github.com/ricardogomesbarreto/TuxFlash/releases).
Na pasta do download:

```sh
sha256sum -c TuxFlash-0.3.4-linux-x86_64.tar.xz.sha256
tar -xf TuxFlash-0.3.4-linux-x86_64.tar.xz
```

Entre na pasta extraída e execute `./tuxflash`. O pacote inclui o runtime e o
código correspondente; não é necessário compilar para usá-lo em x86-64 compatível.

Para desenvolver, clone o código e compile antes de executar:

```sh
git clone https://github.com/ricardogomesbarreto/TuxFlash.git
cd TuxFlash
make
./tuxflash.sh
```

O clone não contém `dist/`, backups ou os executáveis gerados da aplicação.
Dependências de compilação e mídia precisam estar instaladas: consulte
[contribuição e ambiente de desenvolvimento](docs/development/CONTRIBUTING.md).
O inventário local `docs/validation/integrity.json` cobre também artefatos
ignorados e não integra o Git; o download publicado tem seu próprio SHA-256.

### Executar

Na pasta completa do projeto, depois da compilação:

```sh
./tuxflash.sh
./tuxflash.sh --version
./tuxflash.sh doctor
./tuxflash.sh list
./tuxflash.sh probe --image /caminho/imagem.iso
./tuxflash.sh checksum --image /caminho/imagem.iso
python3 scripts/install-menu.py
```

O ponto de entrada principal é `tuxflash.sh`. `scripts/run.sh` delega ao
pacote portátil quando ele existe e usa o runtime local como alternativa.
No pacote extraído de `dist/TuxFlash-0.3.4-linux-x86_64.tar.xz`, execute `./tuxflash`.
Extraia o pacote inteiro: o executável depende dos arquivos que o acompanham.
A janela já aberta usa o binário antigo; feche e reabra para atualizar.

No cabeçalho, escolha Português (Brasil), English, 中文（简体） ou Español,
salve e reabra. Sobre contém créditos clicáveis, novidades e licenças.
A frase da janela Sobre agora descreve a aplicação nativa para Linux.

### Rufus, Lufus e as mudanças do TuxFlash

| Projeto | Contribuição e escopo |
| --- | --- |
| [Rufus](https://github.com/pbatard/rufus) | Aplicação original Windows, algoritmos e recursos herdados. Os fontes Windows originais e suas atribuições permanecem preservados. |
| [Lufus](https://github.com/openresearchtools/lufus) | Base nativa Linux C/GTK e helper separado. Gravação RAW, imagens comprimidas, VHD/VHDX, mídias Windows, persistência e F3 já pertenciam a essa base. |
| TuxFlash | Integração do núcleo de hashes do Rufus, pacote portátil x86-64, rótulo Windows derivado do nome da ISO, correções de integração, identidade/ícone próprios, seletor de quatro idiomas, créditos e notas traduzidas. A versão 0.3.3 reforça a segurança e o tratamento de erros descritos abaixo. |

O Linux nativo é uma característica herdada do Lufus; não é uma novidade
exclusiva do TuxFlash. Tampouco há evidência de superioridade geral em velocidade,
compatibilidade ou número de recursos sobre o Rufus. As melhorias aqui são
correções concretas e integração mais fácil de executar no ambiente validado.
O projeto é independente e não é uma versão oficial Linux do Rufus.

### Correção e testes da versão 0.3.4

A mídia persistente do Zorin OS 18.1 falhava nos links internos de
`dists/stable` e `dists/unstable`. O helper agora materializa aliases internos
como arquivos/diretórios, recusa saídas da árvore da ISO, ciclos e arquivos
especiais, e verifica a árvore e seu tamanho expandido antes de criar partições.
Há sete novos cenários de regressão e 17 suítes nativas.

Os testes com SteamOS/Steam Deck, Kali Installer e Zorin OS estão no
[relatório de imagens reais](docs/validation/IMAGENS-REAIS-2026-10-21.md).
SteamOS recovery é `.img.bz2`, usa RAW/DD e precisa de espaço para os
**8.120.172.544 bytes descomprimidos**; o tamanho de 3,36 GB é do arquivo comprimido.
Os resultados diferenciam comparação de bytes, menus de firmware, persistência
configurada e uma sessão real depois de reiniciar.

### Melhorias técnicas da versão 0.3.3

| Prioridade | Implementação e verificação |
| --- | --- |
| Crítica: disco do sistema | GUI e helper protegem também submontagens de `/boot`, `/usr`, `/var`, `/home`, `/etc`, `/root`, `/opt` e `/srv`. O helper percorre dependências sysfs de volumes LVM/RAID/device-mapper e verifica swap ativo; a listagem segue todos os vínculos do volume. Erros de inspeção e swap sem origem resolvível recusam a operação. |
| Crítica: revalidar USB | Capacidade deve ser exatamente igual à confirmada. GUI guarda identidade/capacidade/`diskseq` na confirmação; jobs enviam a sequência quando o kernel a fornece. Helper repete verificações antes de ferramentas externas e confere o descritor de RAW/leitura posterior com `fstat`, capacidade e `BLKROGET`. |
| Alta: integridade e cancelamento | Progresso continua associado a commits duráveis. Leitura posterior invalida cache e compara os bytes gravados. Esperas de filhos repetem `waitpid` quando interrompidas. A GUI só informa sucesso depois de ler a saída e confirmar término correto do helper, e a CLI trata saída por sinal. |
| Alta: helper e Polkit | GUI permanece sem privilégios. As políticas atual e legada exigem `auth_admin` na sessão ativa e negam sessões inativas/remotas. O pacote portátil usa o `pkexec` do sistema; não instala políticas nem inclui executáveis setuid. |
| Alta: portabilidade | Carregador e bibliotecas próprios; sem Wine ou FUSE. O empacotamento deixa de copiar ícones de aplicações instaladas no computador. BCD-SYS invoca os geradores com Bash e interrompe a preparação se eles falharem, evitando falso sucesso. |
| Média: lançadores e erros | Entrada principal única, delegação preservada e mensagens explícitas para runtime ausente/incompleto. |
| Média: idiomas | Textos novos, créditos Lufus, descrição e notas de versão em pt_BR, en, zh_CN e es; testes gráficos exercitam os quatro idiomas e a conclusão das operações. |

A sequência do disco depende do suporte do kernel. Sem ela, permanecem as
verificações de dispositivo, capacidade, estado e montagens; não há a mesma
capacidade de identificar uma reconexão no mesmo número de dispositivo.
As ferramentas externas ainda abrem seus próprios caminhos: as revalidações
reduzem a janela de troca, mas não constituem prova de ausência de todas as
corridas em qualquer kernel, namespace ou configuração de armazenamento.
Leitura de verificação e captura podem usar um dispositivo somente leitura;
gravações são recusadas nesse estado.

### Funcionalidades e limites

O backend possui gravação RAW/DD, descompressão em fluxo, conversão VHD/VHDX,
captura de dispositivo, hashes MD5/SHA-1/SHA-256/SHA-512, preparação de instalador
Windows FAT32/NTFS, modos de persistência Ubuntu/Debian e teste destrutivo F3.
A GUI e a CLI usam o mesmo modelo de job e helper. Consulte `./tuxflash.sh --help`
e a [documentação técnica Linux](docs/guides/CLI.md).

**Gravar ou testar blocos apaga os dados do destino.** A opção de instalação
silenciosa também cria uma mídia que pode apagar o Disco 0 do computador onde
ela for iniciada. Use apenas em um destino escolhido e confirmado conscientemente.

O pacote pronto é **Linux x86-64**, requer `/bin/sh`, `/proc`, `/sys`, kernel
compatível, acesso a dispositivos e sessão X11/Wayland para a GUI. A GUI usa
Polkit/pkexec do sistema. A CLI pode ser executada por administrador. Outras
arquiteturas precisam de compilação e validação separadas.

Não há garantia para qualquer Linux ou ISO. DD preserva a estrutura da imagem:
ISO apenas óptica pode não iniciar por USB. BIOS/UEFI, arquitetura e Secure Boot
dependem da imagem e do firmware. FFU não é suportado. Windows To Go até o desktop,
Secure Boot e persistência real entre reinicializações não têm validação completa.
A janela GTK conserva a organização do Lufus, sem equivalência integral à Win32.

### Testes e construção

```sh
make
make check
make check-sanitize
```

Compilação: Meson/Ninja, pkg-config, compilador C, GTK >= 4.14, libadwaita >= 1.5,
GLib/GIO, libsoup >= 3.4, JSON-GLib >= 1.8, hivex e gettext. As ferramentas de mídia
necessárias acompanham o portátil; na compilação sem pacote precisam estar instaladas.

Veja [revisão e testes 0.3.3](docs/validation/REVISAO-0.3.3.md),
[cenários em discos virtuais](docs/development/VM-TESTING.md),
[compatibilidade](docs/validation/COMPATIBILIDADE.md) e [histórico Windows físico](docs/validation/TESTE-WINDOWS-FISICO.md).
Os registros históricos mantêm o escopo e os binários de cada etapa; um teste
sintético de firmware não equivale a iniciar Windows ou uma distribuição completa.


#### Organização atualizada em 21/10/2026

Este README reúne Português (Brasil) e English. Guias e relatórios estão em
`docs/`; o código herdado, em `upstream/rufus/`; scripts, em `scripts/`;
o aplicativo portátil, em `dist/portable/`. O lançamento continua por
`./tuxflash.sh`. Consulte o [registro da reorganização](docs/validation/reorganization/validation.json).

### Créditos e licença

[Pete Batard e colaboradores do Rufus](https://github.com/pbatard/rufus),
[Open Research Tools e Lufus](https://github.com/openresearchtools/lufus),
[Ricardo Barreto](https://github.com/ricardogomesbarreto) e demais colaboradores.
O repositório Lufus tem uma entrada própria nos créditos da aplicação.

Aplicação GPL-3.0-or-later, com código correspondente no pacote e no projeto.
[LICENSE.txt](LICENSE.txt) e as licenças originais continuam preservados;
dependências e ativos mantêm suas próprias condições. Consulte os
[avisos de terceiros](docs/legal/THIRD_PARTY_NOTICES.md). Nenhum dado de hardware,
conta ou caminho pessoal deve integrar os registros públicos de validação.

---

<a id="english"></a>
## English

A native C/GTK 4/libadwaita application and CLI, developed by
[Ricardo Barreto](https://github.com/ricardogomesbarreto) and contributors.

### Installation and source

Download the **Linux x86-64** archive and its SHA-256 file from
[Releases](https://github.com/ricardogomesbarreto/TuxFlash/releases).
In your download directory:

```sh
sha256sum -c TuxFlash-0.3.4-linux-x86_64.tar.xz.sha256
tar -xf TuxFlash-0.3.4-linux-x86_64.tar.xz
```

Enter the extracted directory and run `./tuxflash`. The archive includes its
runtime and corresponding application source; compatible x86-64 users do not
need to compile it.

For development, clone and build before running:

```sh
git clone https://github.com/ricardogomesbarreto/TuxFlash.git
cd TuxFlash
make
./tuxflash.sh
```

The clone excludes `dist/`, backups and generated application executables.
Install the build and media dependencies described in the
[contribution guide](docs/development/CONTRIBUTING.md).
The local `docs/validation/integrity.json` also inventories ignored delivery
artifacts and is excluded from Git; release downloads have separate SHA-256 files.

### Run

From the complete project directory, after building:

```sh
./tuxflash.sh
./tuxflash.sh --version
./tuxflash.sh doctor
./tuxflash.sh list
./tuxflash.sh probe --image /path/image.iso
./tuxflash.sh checksum --image /path/image.iso
python3 scripts/install-menu.py
```

`tuxflash.sh` is the main entry point. `scripts/run.sh` selects the portable
bundle when present, with the local runtime as a fallback. When using
`dist/TuxFlash-0.3.4-linux-x86_64.tar.xz`, extract the entire archive and run
`./tuxflash`. Restart an already open application to load the updated binary.

Choose Brazilian Portuguese, English, Simplified Chinese or Spanish from the
header language button, save and restart. About provides credits, release notes
and license summaries. Its new description identifies the native Linux application.

### Rufus, Lufus and TuxFlash

| Project | Contribution |
| --- | --- |
| [Rufus](https://github.com/pbatard/rufus) | Original Windows application, inherited algorithms and resources. Original Windows sources and notices remain preserved. |
| [Lufus](https://github.com/openresearchtools/lufus) | Native C/GTK Linux foundation and separate helper. RAW/compressed writing, VHD/VHDX, Windows media, persistence and F3 already existed in this base. |
| TuxFlash | Rufus hash-core integration, relocatable x86-64 runtime, ISO-filename Windows labels, integration fixes, identity/icon, four-language selector, translated notes and credits. Version 0.3.3 adds the safety and error-handling changes below. |

Native Linux support is inherited from Lufus. There is no evidence of general
speed, compatibility or feature superiority over Rufus. TuxFlash is an independent
derivative, with concrete fixes and packaging for the validated environment.

### Fix and tests in 0.3.4

Zorin OS 18.1 persistent-media creation failed on its internal `dists/stable`
and `dists/unstable` links. The helper now materializes internal aliases as
files/directories, rejects escapes, cycles and special files, and checks the
tree and expanded size before partitioning. Seven regression scenarios were
added, bringing the native suite count to 17.

See the [real-image report](docs/validation/IMAGENS-REAIS-2026-10-21.md) for
SteamOS/Steam Deck, Kali Installer and Zorin OS results. SteamOS recovery is an
`.img.bz2` disk image for RAW/DD; the target must fit **8,120,172,544 decompressed
bytes**, rather than just the 3.36 GB compressed file. The report distinguishes
byte comparisons, firmware menus, persistence layout and a real session after reboot.

### Changes in 0.3.3

| Priority | Implementation |
| --- | --- |
| Critical: system disks | Protect system submounts under `/boot`, `/usr`, `/var`, `/home`, `/etc`, `/root`, `/opt` and `/srv`; inspect sysfs dependencies for stacked LVM/RAID/device-mapper volumes and active swap. Device discovery follows all volume parents. Inspection errors and unresolvable swap backing fail closed. |
| Critical: target revalidation | Exact capacity matching; confirmation snapshots retain identity, capacity and kernel `diskseq`. Jobs pass that sequence when available. The helper rechecks before external tools and validates the actual RAW/read-back descriptor, including `fstat`, capacity and `BLKROGET`. |
| High: integrity and cancellation | Durable progress and cache-invalidated read-back remain available. Interrupted child waits are retried. GUI success requires both drained output and successful helper exit; CLI handles signal termination. |
| High: helper/Polkit | Current and legacy policy sources require active-session `auth_admin` and deny inactive/remote sessions. The portable application uses host `pkexec`, installs no policy and bundles no setuid authentication programs. |
| High: portability | Private loader/libraries, without Wine or FUSE. Packaging excludes unrelated installed application icons. BCD-SYS invokes generators through Bash and propagates generator failures. |
| Medium: launchers/errors | One main entry point, existing delegation and explicit missing/incomplete-runtime errors. |
| Medium: translations | New text, Lufus repository credit, native Linux description and notes in all four languages. GUI tests cover credits, languages and completion/error states. |

Disk sequence validation depends on kernel support. Without it, identity,
capacity, read-only state and mount checks remain, but cannot identify every
same-number reconnect. External tools still open their own paths: repeated
checks reduce the replacement window and are not proof against every possible
race or storage/namespace configuration. Read-only targets remain usable for
capture and verification; writing to them is rejected.

### Capabilities and limits

The backend implements RAW/DD, streaming decompression, VHD/VHDX conversion,
device capture, MD5/SHA-1/SHA-256/SHA-512, Windows installer FAT32/NTFS layouts,
Ubuntu/Debian persistence and destructive F3 testing. GUI and CLI share the job
model and helper. See `./tuxflash.sh --help` and [Linux details](docs/guides/CLI.md).

**Writing or block-testing erases the target.** Silent-install media can also
erase Disk 0 on the computer where that USB is subsequently booted.

The ready-to-run package targets **Linux x86-64**. It needs `/bin/sh`, `/proc`,
`/sys`, a compatible kernel, device access and X11/Wayland for the GUI. GUI
operations use host Polkit/pkexec; administrators can use the CLI. Other
architectures require a separate build and validation.

Compatibility with every Linux or ISO is not guaranteed. DD preserves the
image's disk layout; an optical-only ISO may not boot from USB. Firmware,
architecture and BIOS/UEFI/Secure Boot support remain image-dependent. FFU is
unsupported. A full Windows To Go desktop, Secure Boot and real persistence
across reboots remain unqualified. The GTK layout follows Lufus, without full
Win32 Rufus feature parity.

### Build and test

```sh
make
make check
make check-sanitize
```

Build dependencies include Meson/Ninja, pkg-config, C compiler, GTK >= 4.14,
libadwaita >= 1.5, GLib/GIO, libsoup >= 3.4, JSON-GLib >= 1.8, hivex and gettext.
The portable bundle contains media tools; unbundled builds require them locally.

See [0.3.3 review](docs/validation/REVISAO-0.3.3.md), [virtual disk scenarios](docs/development/VM-TESTING.md),
[compatibility](docs/validation/COMPATIBILIDADE.md) and [historical Windows USB validation](docs/validation/TESTE-WINDOWS-FISICO.md).
Historical evidence remains tied to its own scope and binary; synthetic firmware
tests do not qualify a complete operating-system boot.


#### Repository organization updated on 2026-10-21

This single README contains Brazilian Portuguese and English. Guides and
reports live in `docs/`, inherited code in `upstream/rufus/`, helper scripts
in `scripts/` and the portable application in `dist/portable/`. Launch with
`./tuxflash.sh`. See the [organization checks](docs/validation/reorganization/validation.json).

### Credits and license

[Pete Batard and Rufus contributors](https://github.com/pbatard/rufus),
[Open Research Tools and Lufus](https://github.com/openresearchtools/lufus),
[Ricardo Barreto](https://github.com/ricardogomesbarreto) and contributors.
The application includes a dedicated credit linking to the Lufus repository.

GPL-3.0-or-later, with corresponding application source included in the bundle.
[Original license](LICENSE.txt) and third-party notices remain preserved;
dependencies and artwork retain their own conditions. See
[component notices](docs/legal/THIRD_PARTY_NOTICES.md). Public validation records
exclude local account names, private paths and hardware identifiers.
