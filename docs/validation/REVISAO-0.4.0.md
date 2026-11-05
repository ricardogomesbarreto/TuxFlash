# TuxFlash 0.4.0 — qualificação / qualification

Revisão / Review: **2026-11-05**. Base: `8140ee905415eba9169e560f47e898e2fa97c563` (0.4.0-rc1).

## Português Brasil

A 0.4.0 fecha o marco de mídias completas no escopo abaixo. Não afirma
compatibilidade universal, equivalência integral ao Rufus Windows ou ausência
de erros em modos não qualificados. GUI, ícone, créditos e GPL-3.0-or-later
foram preservados, sem novas dependências de runtime.

### Correção e segurança

A opção de instalação silenciosa que solicita apagar o Disco 0 agora exige
**Windows 11 ou posterior**, assim como a condição existente no Rufus herdado
(`upstream/rufus/src/rufus.c`, bloco `IS_WINDOWS_11`). O Windows 10 continua
suportado pela instalação padrão. O helper consulta a edição selecionada,
valida campos numéricos completos e únicos e recusa metadados ausentes,
ambíguos, negativos, com lixo ou overflow. Windows 11 usa versão NT 10 e
build a partir de 22000. A consulta mantém limites de saída/cancelamento e
fixa o idioma dos nomes dos campos; a interface explica a restrição nos quatro idiomas.

A validação ocorre **antes de limpar MBR/GPT ou formatar o destino**. Com a
ISO real Windows 10 e o helper final, a recusa esperada deixou o SHA-256 dos
16 GiB do destino inalterado. A proteção do Disco 1 no arquivo de resposta não
foi removida. A configuração silenciosa anterior do Windows 10 chegava ao erro
Setup `0x80300025`; o modo padrão passou na instalação completa.

Ensaios exploratórios UEFI:NTFS com essa configuração silenciosa Windows 10
não suportada também falharam (incluindo `0xc000007b` com validação de runtime).
Eles não qualificam NTFS padrão, nem demonstram uma falha de todo NTFS. NTFS,
BIOS, Windows 11 silencioso, WIM dividido e Windows To Go precisam de testes
completos próprios antes de serem considerados qualificados.

### Resultados com imagens reais

| Imagem / modo | Gravação e comparação | Boot / instalação observada |
| --- | --- | --- |
| SteamOS 3.8.14 `.img.bz2`, RAW | 8.120.172.544 bytes; verificação automática, cache invalidado e SHA-256 independente igual à referência expandida | Desktop KDE de recuperação em UEFI, com ferramentas Steam Deck |
| Kali 2026.2 installer amd64, RAW | 4.802.531.328 bytes; leitura e SHA-256 completos | Instalador gráfico na seleção de idioma em BIOS e UEFI |
| Zorin 18.1 Core x86-64, RAW | 3.909.091.328 bytes; leitura e SHA-256 completos | Sessão live com janela Try/Install Zorin em BIOS e UEFI |
| Windows 10 Pro, índice 4, instalação padrão | GPT/FAT32 com helper final, flags Windows 0 e validação de runtime habilitada | Instalação em disco virtual de 64 GiB, OOBE offline e desktop concluídos em UEFI |

O helper final tem SHA-256
`27b5007ba6e83835332f1adfc68c4ef586ade579d64332329f5baeee662b931b`.
Todos os três ensaios RAW foram repetidos com esse binário, em VMs KVM mínimas
com 512 MiB, 1 vCPU e destino regular novo de 16 GiB. A referência SteamOS foi
expandida separadamente. As imagens de boot Linux foram iniciadas antes da
correção exclusiva de compatibilidade Windows; a comparação SHA-256 dos
**16 GiB completos**, incluindo padding, confirmou que as novas saídas são
idênticas aos discos iniciados. Os discos base não foram modificados pelos
boots, que usaram snapshots. Assim a origem da evidência fica explícita.

Os boots gráficos usaram KVM, 2 vCPUs e 2–2,5 GiB, OVMF/SeaBIOS conforme o
caso, sem rede e com Secure Boot desativado. Windows usou 2 GiB e um único
disco de instalação descartável, além do USB virtual. A sessão foi retomada
a partir do disco instalado após encerrar o limite da primeira execução.
O usuário da VM é genérico; não foram usados dados ou credenciais pessoais.
A ISO Windows local contém Windows 10 Pro, confirmado no desktop como 21H1 build 19043.928; seu SHA-256 é
`1119a177f3a4fc8a8fe6c1c1ef59afcf94ed6af0adcff445993ac4fdddd13768`.
Hashes locais provam igualdade, **não autenticidade do fornecedor**.
Nenhum dispositivo físico foi acessado nesta revisão.

### Regressões e interface

- **24 suítes sem GUI** e **10 regressões** do roteiro de mídias reais aprovadas.
- **69 casos de backend em VM root**, incluindo os quatro novos casos de versão Windows e a verificação entre UIDs.
- **55 casos ASan/UBSan do helper** aprovados; um caso entre UIDs omitido no host e executado na VM. Checks independentes de hashes, probe e plataforma aprovados.
- **24 configurações GUI**: pt_BR/en/zh_CN/es, 800×600/1024×768/1920×1080, nativas e com runtime portátil. Em cada configuração, seis cenários simulados de conclusão/cancelamento/erro passaram, sem I/O físico ou Polkit.
- Rodapé e ações visíveis, sem barras de rolagem visíveis e sem overflow horizontal. Xvfb agora fixa renderização Cairo: a tentativa anterior completou os casos simulados, mas excedeu o prazo ao encerrar o renderizador sem aceleração.

A política Polkit não mudou. Os 13 casos de GUI/Polkit reais da 0.3.9 são
históricos e não foram repetidos neste ciclo. Não confundir esses resultados
com o teste de backend root ou os processos simulados da GUI.

[Evidências sanitizadas](evidence/linux/tuxflash/qualification-0.4.0/summary.json),
[instalação Windows](evidence/linux/tuxflash/qualification-0.4.0/windows-installation.json),
[desktop](evidence/linux/tuxflash/qualification-0.4.0/windows-desktop.png),
[versão Windows](evidence/linux/tuxflash/qualification-0.4.0/windows-version.png),
[recusa sem escrita](evidence/linux/tuxflash/qualification-0.4.0/windows-preflight.json),
[equivalência dos discos Linux](evidence/linux/tuxflash/qualification-0.4.0/boot-target-equivalence.json),
[backend](evidence/linux/tuxflash/qualification-0.4.0/backend-regression.json).
Os demais arquivos da mesma pasta registram gravações, boots e matrizes GUI.

### Limites e próxima etapa

Recuperação física Steam Deck, Secure Boot, persistência entre reinicializações,
Windows To Go até desktop, modos Windows avançados e execução em mais famílias
Linux permanecem no [roteiro 1.0](../development/ROADMAP-1.0.md).
O pacote pronto é Linux x86-64. FFU permanece fora do backend Linux.
Reprodução: [roteiro de mídias reais](../development/REAL-MEDIA-QUALIFICATION.md).
O histórico [0.4.0-rc1](REVISAO-0.4.0-rc1.md) conserva os resultados e as
interrupções daquela candidata, sem reclassificá-los como aprovações.

## English

Version 0.4.0 completes the full-media milestone within the matrix above.
It does not establish universal Linux/ISO/firmware support, complete Rufus
Windows parity, or qualification of every advanced mode.

Silent Disk 0 installation now requires Windows 11 or later, matching the
inherited Rufus condition. Standard Windows 10 installation remains available.
The helper validates complete, unique numeric version fields before clearing
or formatting the target. Missing, duplicate, signed, malformed or overflowing
metadata is rejected. The real Windows 10 preflight rejection left the entire
16 GiB disk unchanged. The Disk 1 guard was retained. Four-language labels,
errors and release notes explain the restriction. No runtime dependency was added.

The final helper wrote and read back all three real RAW images. Independent
full-payload SHA-256 matched the separately expanded SteamOS reference and
Kali/Zorin ISOs. Earlier Linux boot disks and the final helper outputs also
matched over their complete 16 GiB, including padding. Snapshot boots reached
SteamOS recovery KDE (UEFI), Kali graphical installation (BIOS/UEFI), and Zorin
live welcome (BIOS/UEFI). This states the evidence provenance explicitly.

A newly created standard Windows 10 Pro UEFI/FAT32 installer, with runtime
validation enabled and Windows flags 0, completed installation, offline OOBE
and first desktop on a disposable 64 GiB virtual disk. The first VM session
was resumed from that installed disk after its test time limit ended. No
physical disk, network, personal account or activation key was used. Local
hashes establish equality, not vendor authenticity. Secure Boot was disabled.

Passed: 24 headless suites, 69 root backend VM cases, 55 helper ASan/UBSan
cases (one host cross-UID skip exercised in the VM), independent hash/platform
checks, 10 real-media harness guards and 24 GUI configurations with six mock
writer lifecycle cases each. Xvfb uses Cairo software rendering to avoid the
previous renderer shutdown timeout. Real Polkit GUI results remain historical
0.3.9 evidence; they were not rerun in this cycle.

Exploratory Windows 10 silent-install tests failed, including NTFS variants;
they are unsupported configurations now rejected before writing. They neither
qualify normal NTFS nor establish that all NTFS modes fail. Windows 11 silent
installation, NTFS, BIOS, split WIM, Windows To Go, persistence across reboots,
physical Steam Deck recovery, Secure Boot and broader Linux execution remain
separate gates. The ready-to-run package is x86-64. Sources, GPL and upstream
credits are preserved. Sanitized evidence and reproduction links are above.
