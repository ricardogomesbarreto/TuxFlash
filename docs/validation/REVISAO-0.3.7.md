# TuxFlash 0.3.7 — revisão e validação / review and validation

Publicação / Release: **2026-10-29**. Base: `b8ae3597eecf0f79ce28358c2e712a13e29444d3`.

## Português Brasil

Esta versão reforça o encerramento do helper e o resultado dos comandos auxiliares.
Mantém interface, ícone, créditos e GPL-3.0-or-later, com notas traduzidas para
pt_BR, en, zh_CN e es. Não houve gravação nem inspeção de pendrive físico neste ciclo.

### Correções

- O helper ignora SIGPIPE. Quando a GUI desaparece e fecha seus pipes, a escrita de um log não o mata antes do tratamento de cancelamento e da passagem pela limpeza.
- `run_command_input` verifica a entrega integral da entrada, sempre recolhe o processo filho e recusa sucesso após cancelamento, inclusive quando a ferramenta retorna zero. Falhas de entrada e de espera preservam um erro útil; saída anormal é falha.
- O pipe da entrada usa close-on-exec. A rotina de desmontagem continua permitida no caminho de limpeza após cancelamento.
- Três regressões falharam na base anterior e passaram com a correção: perda do leitor de logs com limpeza, entrada recusada pela ferramenta e cancelamento com saída zero do filho.

### Resultados

| Ensaio | Resultado e alcance |
| --- | --- |
| Meson nativo | 23 suítes aprovadas, nenhuma falha |
| Helper ASan/UBSan | 34 casos sem privilégios aprovados; o caso entre UIDs é omitido no host e executado na VM |
| Hashes/plataforma | Checks independentes e ASan/UBSan aprovados |
| Backend root em VM | 48 casos aprovados, incluindo as três novas regressões; alvos são arquivos de disco virtual descartáveis |
| GUI | 12 configurações nativas + 12 portáteis: quatro idiomas × 800×600, 1024×768 e 1920×1080; formulário básico/avançado e seis cenários simulados de ciclo de vida por configuração |
| Polkit/PAM real | 13 casos GUI aprovados e recusa de chamador sem sessão; autorização, recusa, senha incorreta, cancelamento e perda abrupta da GUI nos layouts instalado/portátil; sessão instalada inativa recusada |

[Evidência sanitizada](evidence/linux/tuxflash/stability-0.3.7/summary.json):
[suítes](evidence/linux/tuxflash/stability-0.3.7/native-suites.json),
[regressões reproduzidas](evidence/linux/tuxflash/stability-0.3.7/reproduced-regressions.json),
[sanitizadores](evidence/linux/tuxflash/stability-0.3.7/helper-sanitizers.log),
[backend](evidence/linux/tuxflash/stability-0.3.7/backend-vm.log),
[GUI nativa](evidence/linux/tuxflash/stability-0.3.7/gui-native.json),
[GUI portátil](evidence/linux/tuxflash/stability-0.3.7/gui-portable.json) e
[Polkit real/proveniência](evidence/linux/tuxflash/stability-0.3.7/polkit-vm.json).

### Alcance e limites

A VM Polkit usa Ubuntu Minimal 24.04 com kernel 7.0.0-38, QEMU/TCG x86-64,
2 vCPUs, 1408 MiB, Xvfb e runtime privado. A imagem oficial datada é conferida
por assinatura e SHA-256. Polkit/PAM e a política original são reais; o agente
é automatizado e a seleção/confirmação inicial é preparada pelo fixture. Isso
não qualifica agentes visuais GNOME/KDE nem um pacote Debian/RPM instalado.
O layout instalado usa um wrapper root; o portátil usa a ação genérica do Polkit.
A recusa de sessão inativa foi qualificada apenas no layout instalado.

A perda abrupta usa `_exit(0)` na GUI durante escrita RAW e um supervisor comum
que recolhe o helper root: saída normal 1, sem sinal, em ambos os layouts. Não há
rollback dos bytes já escritos. O teste separado com ferramenta cooperante usa
`umount` falso para comprovar a passagem pela limpeza; não demonstra desmontagem
real após toda falha nem término de árvores arbitrárias de processos ou ferramentas
que ignorem SIGTERM. A rede do guest é desativada antes dos casos de autorização.

O backend usa imagens sintéticas: preparação de Windows/Windows To Go e layouts
de persistência não comprovam Windows instalado, desktop WTG ou persistência
entre reinícios. Os resultados históricos com ISOs reais mantêm a versão e o
alcance originais. Faltam instalação Windows completa, persistência real, WTG
até o desktop, Secure Boot, retirada física do USB e uma matriz maior de Linux,
arquiteturas e agentes de autorização. Estes testes não certificam a versão 1.0.

Para reproduzir: [backend em VM](../development/VM-TESTING.md),
[Polkit em VM](../development/POLKIT-VM.md), `make`, `make check`,
`make check-sanitize` e `make check-helper-sanitize`. O [roteiro da 1.0](../development/ROADMAP-1.0.md)
conserva os critérios ainda abertos. O pacote portátil inclui as fontes correspondentes
e hashes; o SHA-256 do arquivo de download é distribuído ao lado dele.

## English

Version 0.3.7 hardens helper shutdown and command-result handling, preserving the
UI, icon, credits and GPL-3.0-or-later. In-app notes cover all four languages.
No physical USB was read or written during this qualification.

The helper ignores SIGPIPE so a closed frontend log pipe cannot kill it before
cancellation handling and cleanup. Command-input delivery must complete; every
child is reaped and cancellation is a failure even if the tool exits zero.
Input pipes use close-on-exec and cleanup unmounts remain allowed after cancellation.
All three regression cases fail on the baseline and pass on this candidate.

Results: **23 native suites**, **34 nonprivileged helper sanitizer cases** plus
independent hash/platform checks, **48 root-backend VM cases**, **24 GUI
configurations**, and **13 real-Polkit GUI cases plus sessionless denial** passed.
The cross-UID sanitizer case is skipped on the host and exercised in the root VM.
Sanitized evidence is linked above, with exact application/helper fingerprints.

The full-OS VM uses Ubuntu 24.04 userspace, kernel 7.0.0-38, x86-64 QEMU/TCG,
2 vCPUs, 1408 MiB, Xvfb and the private runtime. The dated official root archive
is signature/hash-verified. Actual Polkit/PAM and the original policy are used;
the agent and initial job selection/confirmation are fixture-driven. The installed
layout uses a root wrapper, not a qualified Debian/RPM install. Portable execution
uses generic Polkit exec authorization. Inactive-session rejection is qualified
only for the installed layout. Networking is disabled before authorization tests.

Abrupt-exit cases call `_exit(0)` during RAW writing; an ordinary-user subreaper
observes normal helper exit 1 without a signal in both layouts. This does not
roll back written bytes. A separate cooperating-tool regression uses fake umount
to verify cleanup control flow; arbitrary process trees, SIGTERM-ignoring tools
and actual unmounts after every failure are not qualified.

Synthetic Windows/WTG/persistence cases prove provisioning/layout behavior,
not a complete Windows installation, WTG desktop or persistence across reboots.
Earlier real-image evidence retains its original version/scope. Full installations,
persistent reboots, WTG desktop, Secure Boot, physical removal, broader Linux/
architecture execution and visual desktop agents remain open. These results do
not certify 1.0. Reproduction guides and the 1.0 acceptance gates are linked above.
