<p align="center">
  <img src="linux/assets/icons/hicolor/512x512/apps/org.tuxflash.TuxFlash.png" alt="TuxFlash logo — Linux penguin and USB drive" width="170">
</p>

<h1 align="center">TuxFlash</h1>

<p align="center">
  <strong>Criador de pendrives inicializáveis para Linux · Bootable USB Creator for Linux</strong>
</p>

<p align="center">
  <a href="https://github.com/ricardogomesbarreto/TuxFlash/actions/workflows/ci.yml"><img alt="Build and tests" src="https://github.com/ricardogomesbarreto/TuxFlash/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="License GPL-3.0-or-later" src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue">
  <img alt="Platform Linux x86-64" src="https://img.shields.io/badge/platform-Linux%20x86--64-informational">
</p>

<p align="center">
  <a href="#portugues-brasil">Português (Brasil)</a> ·
  <a href="#english">English</a> ·
  <a href="https://github.com/ricardogomesbarreto/TuxFlash/releases">Downloads / Releases</a> ·
  <a href="docs/development/CONTRIBUTING.md">Contribuir / Contribute</a>
</p>

> **Release status / Estado das versões:** **[0.3.6](https://github.com/ricardogomesbarreto/TuxFlash/releases/tag/v0.3.6)** é a versão pública atual, com pacote portátil Linux x86-64 e SHA-256. / **[0.3.6](https://github.com/ricardogomesbarreto/TuxFlash/releases/tag/v0.3.6)** is the current public release, with a Linux x86-64 portable archive and SHA-256.

---

<a id="portugues-brasil"></a>
## Português (Brasil)

O **TuxFlash** é um aplicativo nativo em **C, GTK 4 e libadwaita**, com interface gráfica e linha de comando, para criar mídias USB inicializáveis no Linux. É um projeto independente construído a partir do [Lufus](https://github.com/openresearchtools/lufus), com recursos e código herdados do [Rufus](https://github.com/pbatard/rufus). Não é uma edição oficial do Rufus para Linux.

### Instalação rápida: baixar, extrair e executar

**Não é necessário compilar nem instalar o aplicativo no sistema para utilizar o pacote portátil.** Ele inclui executável, runtime e arquivos de apoio. É preciso **extrair a pasta inteira**: `tuxflash` é o arquivo de inicialização, mas **não funciona sozinho se for separado do diretório `runtime/`**.

**1. Baixe os dois arquivos da versão publicada para Linux x86-64:**

- [TuxFlash 0.3.6 — pacote portátil (.tar.xz)](https://github.com/ricardogomesbarreto/TuxFlash/releases/download/v0.3.6/TuxFlash-0.3.6-linux-x86_64.tar.xz)
- [SHA-256 — arquivo de verificação](https://github.com/ricardogomesbarreto/TuxFlash/releases/download/v0.3.6/TuxFlash-0.3.6-linux-x86_64.tar.xz.sha256)

Também estão disponíveis na página de [Releases](https://github.com/ricardogomesbarreto/TuxFlash/releases).

**2. Abra um terminal na pasta em que os dois arquivos foram baixados** e execute:

```bash
# Confirme a integridade do pacote (recomendado).
sha256sum -c TuxFlash-0.3.6-linux-x86_64.tar.xz.sha256

# Extraia o pacote.
tar -xf TuxFlash-0.3.6-linux-x86_64.tar.xz

# Entre na pasta que foi criada.
cd TuxFlash-0.3.6-linux-x86_64

# Inicie a interface gráfica.
./tuxflash
```

**Pronto:** para abrir novamente, entre na pasta extraída e execute `./tuxflash`. Em gerenciadores de arquivos que permitem executar scripts, também é possível abrir o arquivo `tuxflash` pelo modo gráfico, escolhendo **Executar** quando necessário. O comportamento do duplo clique varia de acordo com o ambiente de trabalho.

**3. (Opcional) Adicione o TuxFlash ao menu de aplicativos do Linux:**

```bash
# Execute dentro da pasta extraída.
python3 scripts/install-menu.py
```

Depois procure **TuxFlash** no menu de aplicativos. O script cria um atalho e instala o ícone **somente na sua conta**, sem instalar um pacote de sistema. **Não mova nem apague a pasta extraída depois de criar o atalho**, pois ele aponta para o executável naquele local. Para remover apenas o atalho:

```bash
python3 scripts/install-menu.py --remove
```

Não é necessário iniciar o aplicativo com `sudo`. Quando uma gravação exigir privilégios, o helper utilizará o **Polkit/pkexec do próprio sistema** para solicitar autorização. O sistema precisa oferecer esse serviço.

### Como criar um pendrive inicializável

1. Conecte um **pendrive que possa ser completamente apagado**. Faça backup antes de continuar.
2. Abra o **TuxFlash** pelo menu ou usando `./tuxflash`.
3. Selecione a imagem ISO/IMG e confira o tipo de gravação disponível para ela.
4. Escolha **cuidadosamente o dispositivo USB de destino**, conferindo nome e capacidade. Nunca selecione seu disco interno.
5. Ajuste apenas as opções compatíveis com a imagem selecionada; em modo RAW/DD, a estrutura da imagem é preservada.
6. Inicie a gravação, leia as confirmações de perda de dados e autorize a operação somente se o dispositivo estiver correto.
7. Espere a conclusão, a sincronização e a verificação, quando habilitada, antes de remover o pendrive.

**Importante:** a gravação e o teste destrutivo de blocos **apagam dados do dispositivo selecionado**. Criar a mídia não garante que toda ISO seja inicializável em qualquer computador; isso depende da imagem, firmware, arquitetura e modo BIOS/UEFI.

### Clonar o repositório e executar a versão de desenvolvimento

**O repositório de código não é o mesmo que o pacote portátil:** um clone novo não inclui os executáveis e artefatos ignorados pelo Git. Para utilizar o código-fonte, é necessário compilar uma vez.

No Ubuntu 24.04 ou ambiente de desenvolvimento compatível:

```bash
sudo apt update
sudo apt install build-essential pkg-config meson ninja-build gettext \
  libgtk-4-dev libadwaita-1-dev libsoup-3.0-dev \
  libjson-glib-dev libhivex-dev xorriso qemu-utils xvfb xauth python3-pil

git clone https://github.com/ricardogomesbarreto/TuxFlash.git
cd TuxFlash
make
./tuxflash
```

Depois da compilação, **basta executar `./tuxflash`** para abrir a interface (o comando antigo `./tuxflash.sh` continua funcionando). O arquivo delega a execução a `scripts/run.sh`, que usa `dist/portable/tuxflash` se estiver presente ou o executável de `linux/runtime/` caso contrário. Se aparecer uma mensagem de runtime ausente, execute `make` e verifique a saída de compilação. Dependências adicionais de mídia variam conforme a operação; veja o [guia de desenvolvimento](docs/development/CONTRIBUTING.md).

Para criar o atalho de menu a partir do clone já compilado:

```bash
python3 scripts/install-menu.py
```

### Comandos úteis e diagnóstico

No **pacote extraído** e no **repositório compilado**, use `./tuxflash`. O alias `./tuxflash.sh` também funciona no clone:

```bash
./tuxflash.sh --version
./tuxflash.sh --help
./tuxflash.sh doctor
./tuxflash.sh list
./tuxflash.sh probe --image /caminho/arquivo.iso
./tuxflash.sh checksum --image /caminho/arquivo.iso
```

Esses comandos permitem consultar a versão, descobrir opções, verificar ferramentas, listar dispositivos e inspecionar uma imagem sem iniciar uma gravação. A [referência completa da CLI](docs/guides/CLI.md) documenta gravação, captura, confirmação obrigatória de destino e modos destrutivos.

### Recursos

| Recurso | Descrição |
| --- | --- |
| Interface e CLI | Aplicativo nativo GTK 4/libadwaita e linha de comando com backend compartilhado |
| Imagens | Escrita RAW/DD de ISO/IMG/RAW/DD e formatos comprimidos suportados |
| Discos virtuais | Conversão de VHD/VHDX para dispositivos físicos e captura em formatos suportados |
| Integridade | MD5, SHA-1, SHA-256, SHA-512 e modos de leitura posterior |
| Mídias de instalação | Modos específicos para instaladores e opções avançadas disponíveis conforme a imagem |
| Linux Live | Persistência Ubuntu/Debian nos modos suportados |
| Diagnóstico | Inspeção de imagens, seleção de dispositivos e teste F3 destrutivo |
| Idiomas | Português (Brasil), English, 中文（简体） e Español |

Os recursos são herdados e adaptados principalmente do Lufus; não há promessa de paridade completa com a versão Windows do Rufus. Algumas opções dependem da imagem e do firmware ou têm validação limitada. Consulte a [compatibilidade](docs/validation/COMPATIBILIDADE.md).

### Requisitos e segurança

- **Pacote portátil:** Linux **x86-64**, kernel compatível, `/proc`, `/sys`, `/bin/sh` e sessão **X11/Wayland** para a interface. Não requer Wine nem FUSE.
- **Autorização:** a interface permanece sem privilégios; operações restritas usam `pkexec` do sistema, quando disponível. Instalar o atalho **não** concede privilégios automáticos.
- **Proteção do destino:** o helper revalida o dispositivo, capacidade e estado, identifica montagens protegidas, volumes dependentes e swap ativo, e recusa gravações inseguras conforme as verificações implementadas.
- **Limitações:** nenhuma garantia de funcionamento em toda distribuição, ISO ou firmware; FFU não é suportado. Secure Boot, persistência real após reinicialização e Windows To Go completo não estão integralmente qualificados.
- **Opções avançadas destrutivas:** mídias de instalação silenciosa podem apagar o **Disco 0 de outro computador quando iniciadas**. Consulte a [documentação da CLI](docs/guides/CLI.md) antes de usar esses modos.

Se a interface não abrir, execute o lançador pelo terminal para ler o erro, rode `doctor` e verifique se o pacote está **totalmente extraído**. Se o atalho parar de funcionar após mover a pasta, execute o instalador de menu novamente no novo local.

### Testes adicionais com imagens reais

Debian 13.7 e Ubuntu 26.04.1 tiveram gravação e comparação de imagens
em discos virtuais, com verificações de menus BIOS/UEFI. O instalador Debian
chegou à seleção de idioma em UEFI. Esses resultados não demonstram boot
completo em hardware físico. Veja o
[relatório de validação e seus limites](docs/validation/USB-E-NOVAS-ISOS-2026-10-21.md).

### Desenvolvimento e histórico de versões

O [roteiro para a 1.0](docs/development/ROADMAP-1.0.md) define os critérios ainda pendentes: validação em mais ambientes de autorização, instalação Windows completa, persistência entre boots, Windows To Go e matriz de execução Linux. O [ensaio Polkit em VM](docs/validation/POLKIT-2026-10-28.md) passou em 11 casos GUI e na recusa sem sessão. A versão pública permanece 0.3.6.

- **0.3.3 — disponível publicamente:** endurecimento das verificações de destino e volumes do sistema, melhorias de cancelamento/relato de sucesso e documentação de execução.
- **0.3.6 — versão atual:** janela adaptável sem barras de rolagem visíveis, rodapé com botões fixos, canal privado de cancelamento, origem comprimida fixada por descritor e saída de captura exclusiva. Veja a [revisão e os testes](docs/validation/REVISAO-0.3.6.md).
- **0.3.5:** conferência com tamanho exato da origem, progresso limitado e cancelamento de leituras bloqueadas; [validação anterior](docs/validation/REVISAO-0.3.5.md).
- **0.3.4:** correção de persistência do Zorin e [testes com SteamOS, Kali e Zorin](docs/validation/IMAGENS-REAIS-2026-10-21.md).

Na árvore do repositório, `tuxflash` (ou `tuxflash.sh`) é o lançador de desenvolvimento; `scripts/` contém execução e instalação do atalho; `linux/` abriga código, runtime e recursos; `docs/` reúne os guias, a compatibilidade, licenças e evidências; `upstream/rufus/` conserva material herdado. `build/` e `dist/` são saídas locais e não fazem parte de um clone limpo.

Para compilar e executar os testes **sem usar mídia física**, consulte:

```bash
make
make check
make check-sanitize
make check-helper-sanitize
```

Documentação: [contribuição](docs/development/CONTRIBUTING.md) · [testes em VM](docs/development/VM-TESTING.md) · [CLI](docs/guides/CLI.md) · [notas de versão](docs/releases/NOTAS-DA-VERSAO.md).

### Créditos e licença

Desenvolvimento TuxFlash: [Ricardo Barreto](https://github.com/ricardogomesbarreto). Bases: [Rufus — Pete Batard e colaboradores](https://github.com/pbatard/rufus) e [Lufus — Open Research Tools e colaboradores](https://github.com/openresearchtools/lufus). Projeto independente, licenciado sob **GPL-3.0-or-later**, com avisos de terceiros mantidos em [THIRD_PARTY_NOTICES.md](docs/legal/THIRD_PARTY_NOTICES.md) e licença em [LICENSE.txt](LICENSE.txt).

---

<a id="english"></a>
## English

**TuxFlash** is a native **C / GTK 4 / libadwaita** GUI and CLI for creating bootable USB media on Linux. It is an independent project based on [Lufus](https://github.com/openresearchtools/lufus), with inherited code and resources from [Rufus](https://github.com/pbatard/rufus). It is **not** an official Linux edition of Rufus.

### Quick start: download, extract and run

**No compilation or system-wide installation is needed to use the portable release.** It includes the executable, runtime and supporting files. You **must extract the entire archive**: `tuxflash` is the launcher, but **it is not a standalone binary that works without `runtime/`**.

**1. Download both files for the published Linux x86-64 release:**

- [TuxFlash 0.3.6 — portable archive (.tar.xz)](https://github.com/ricardogomesbarreto/TuxFlash/releases/download/v0.3.6/TuxFlash-0.3.6-linux-x86_64.tar.xz)
- [SHA-256 checksum file](https://github.com/ricardogomesbarreto/TuxFlash/releases/download/v0.3.6/TuxFlash-0.3.6-linux-x86_64.tar.xz.sha256)

Or open the [Releases](https://github.com/ricardogomesbarreto/TuxFlash/releases) page.

**2. Open a terminal in the download directory** and run:

```bash
# Verify the archive (recommended).
sha256sum -c TuxFlash-0.3.6-linux-x86_64.tar.xz.sha256

# Extract and enter its directory.
tar -xf TuxFlash-0.3.6-linux-x86_64.tar.xz
cd TuxFlash-0.3.6-linux-x86_64

# Launch the graphical application.
./tuxflash
```

**That's it.** To launch it again, open the extracted folder and run `./tuxflash`. Some graphical file managers also allow you to launch the `tuxflash` file with **Run / Execute**; double-click behavior depends on the desktop environment.

**3. (Optional) Add TuxFlash to your Linux application menu:**

```bash
# Run from the extracted directory.
python3 scripts/install-menu.py
```

Search for **TuxFlash** in your application menu. This registers a launcher and icon **for the current user only**, not a system package. **Do not move or delete the extracted folder** after installing the shortcut: it points to that location. To remove the shortcut:

```bash
python3 scripts/install-menu.py --remove
```

Do **not** run the application with `sudo`. When writing requires elevated privileges, the helper uses your Linux system's **Polkit/pkexec** authentication. The host must provide that service.

### Create a bootable USB drive

1. Connect a **USB drive whose contents can be fully erased**. Back up any files first.
2. Open **TuxFlash** from the application menu or with `./tuxflash`.
3. Select the ISO/IMG image and an available writing mode appropriate for it.
4. Carefully select the **target USB device**, checking its name and capacity. Never select the system disk.
5. Review supported options. RAW/DD mode preserves the source image's disk layout.
6. Start writing, read the data-loss warnings and confirm only after verifying the target.
7. Wait for completion, synchronization and enabled verification before removing the drive.

**Warning:** writing or destructive block testing **erases the selected target**. Creating the media does not ensure every ISO will boot on every machine; firmware, architecture, image and BIOS/UEFI support matter.

### Clone and run the development version

**A Git clone is not the portable distribution.** Generated binaries and ignored artifacts are not included in a fresh clone. Build it once before running.

On Ubuntu 24.04 or a compatible development environment:

```bash
sudo apt update
sudo apt install build-essential pkg-config meson ninja-build gettext \
  libgtk-4-dev libadwaita-1-dev libsoup-3.0-dev \
  libjson-glib-dev libhivex-dev xorriso qemu-utils xvfb xauth python3-pil

git clone https://github.com/ricardogomesbarreto/TuxFlash.git
cd TuxFlash
make
./tuxflash
```

After building, launch again with **`./tuxflash`** (the existing `./tuxflash.sh` command also works). This launcher delegates to `scripts/run.sh`, which prefers `dist/portable/tuxflash` if present and otherwise uses `linux/runtime/`. A missing-runtime error means the application has not yet been built or the expected files are absent. Media tools vary by operation; see [development setup](docs/development/CONTRIBUTING.md).

To add a menu shortcut from the built checkout:

```bash
python3 scripts/install-menu.py
```

### Commands and troubleshooting

Use `./tuxflash` in both the **extracted portable package** and the **built Git checkout**. The `./tuxflash.sh` alias remains available in a checkout:

```bash
./tuxflash.sh --version
./tuxflash.sh --help
./tuxflash.sh doctor
./tuxflash.sh list
./tuxflash.sh probe --image /path/image.iso
./tuxflash.sh checksum --image /path/image.iso
```

These commands show the version, options, tool diagnostics, devices and image properties **without starting a write**. See the [CLI reference](docs/guides/CLI.md) for media writing, capture, target confirmation and destructive modes.

### Features

| Feature | Description |
| --- | --- |
| GUI and CLI | Native GTK 4/libadwaita interface, shared job model and backend |
| Images | RAW/DD writing of supported ISO/IMG/RAW/DD and compressed images |
| Virtual disks | VHD/VHDX conversion to target devices and device capture |
| Integrity | MD5, SHA-1, SHA-256, SHA-512 and read-back modes |
| Installation media | Installer-specific layouts and advanced image-dependent options |
| Linux live media | Supported Ubuntu/Debian persistence modes |
| Diagnostics | Image inspection, device selection and destructive F3 capacity testing |
| Languages | Português (Brasil), English, 中文（简体） and Español |

Most underlying capabilities originate in Lufus; full feature parity with Windows Rufus is **not** claimed. Some options depend on the source image and firmware, and some still have limited validation. See [compatibility details](docs/validation/COMPATIBILIDADE.md).

### Requirements and safety

- **Portable release:** compatible **Linux x86-64**, `/proc`, `/sys`, `/bin/sh`, and **X11/Wayland** for the GUI. No Wine or FUSE is needed.
- **Authentication:** GUI remains unprivileged; restricted operations use host `pkexec` when available. Installing a menu entry **does not** grant automatic privileges.
- **Target safeguards:** helper rechecks device identity, capacity and state, protected mounts, stacked storage and active swap, rejecting unsafe targets under its implemented checks.
- **Limitations:** no universal distribution, ISO or firmware guarantee; FFU is unsupported. Secure Boot, persistence after real reboots and a complete Windows To Go desktop have not been fully qualified.
- **Advanced destructive modes:** unattended installation media can erase **Disk 0 on another PC when subsequently booted**. Read the [CLI guide](docs/guides/CLI.md) before using those modes.

If the GUI does not open, launch it from a terminal to inspect the error, run `doctor` and verify that the **complete archive** was extracted. If the menu shortcut breaks after relocating the directory, reinstall it from the new path.

### Additional real-image tests

Debian 13.7 and Ubuntu 26.04.1 completed image writing and read-back
in disposable virtual disks, with BIOS/UEFI firmware-menu checks. Debian
reached its UEFI installer language screen. These results do not establish
a complete physical-hardware boot. See the
[validation report and its limits](docs/validation/USB-E-NOVAS-ISOS-2026-10-21.md).

### Development and versions

The [road to 1.0](docs/development/ROADMAP-1.0.md) defines the remaining gates: broader authorization environments, complete Windows installation, persistent reboots, Windows To Go and a Linux execution matrix. The [real Polkit VM test](docs/validation/POLKIT-2026-10-28.md) passed 11 GUI cases and sessionless denial. The public release remains 0.3.6.

- **0.3.3 — public release:** improved system-volume and target verification, cancellation/success reporting and runtime guidance.
- **0.3.6 — current version:** responsive window without visible scrollbars, fixed action footer, private cancellation channel, pinned compressed input and exclusive capture output. See the [review and tests](docs/validation/REVISAO-0.3.6.md).
- **0.3.5:** exact source-length verification, bounded progress and cancellation of blocked reads; [previous validation](docs/validation/REVISAO-0.3.5.md).
- **0.3.4:** Zorin persistence fix and [SteamOS, Kali and Zorin validation](docs/validation/IMAGENS-REAIS-2026-10-21.md).

Repository layout: `tuxflash` (or `tuxflash.sh`) launches a built development checkout; `scripts/` provides runtime selection and menu integration; `linux/` contains source, runtime and resources; `docs/` holds manuals, compatibility, licensing and validation evidence; `upstream/rufus/` preserves inherited material. `build/` and `dist/` are generated locally and absent from a clean clone.

Build and run non-physical media checks:

```bash
make
make check
make check-sanitize
make check-helper-sanitize
```

More: [contributing](docs/development/CONTRIBUTING.md) · [VM tests](docs/development/VM-TESTING.md) · [CLI](docs/guides/CLI.md) · [release notes](docs/releases/NOTAS-DA-VERSAO.md).

### Credits and license

TuxFlash development: [Ricardo Barreto](https://github.com/ricardogomesbarreto). Foundations: [Rufus — Pete Batard and contributors](https://github.com/pbatard/rufus) and [Lufus — Open Research Tools and contributors](https://github.com/openresearchtools/lufus). Independent project licensed under **GPL-3.0-or-later**; original copyrights and component terms remain preserved in [THIRD_PARTY_NOTICES.md](docs/legal/THIRD_PARTY_NOTICES.md) and [LICENSE.txt](LICENSE.txt).
