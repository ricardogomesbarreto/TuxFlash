# TuxFlash 0.3.9 — revisão e validação / review and validation

Publicação / Release: **2026-10-30**. Base: `d866b8925f5b72d9d704918538a28b4555e4a1f7` (0.3.8).

## Português Brasil

A 0.3.9 corrige consultas de metadados do helper que podiam impedir o
cancelamento ou aceitar saída excessiva. Interface, ícone, créditos e licença
GPL-3.0-or-later foram preservados. Nenhum pendrive físico foi acessado neste ciclo.

### O que mudou

- As consultas de tamanho virtual de VHD/VHDX por qemu-img e de idioma Windows por wimlib-imagex usam o coletor gerenciado do helper, com cancelamento por sinal ou perda do canal privado da interface/CLI.
- stdout e stderr são drenados separadamente. A falha do líder é observada mesmo se um descendente mantiver o pipe aberto. Falhas e cancelamento encerram o grupo gerenciado, conforme o contrato da 0.3.8.
- A captura combinada tem limite de **4 MiB de bytes**. Excesso retorna EFBIG; cancelamento, ECANCELED. Esse limite não restringe a memória/CPU da própria ferramenta externa.
- UTF-8 é validado com comprimento explícito, recusando bytes inválidos e NUL interno. Uma saída truncada por NUL não chega ao parser como um prefixo válido. Diagnósticos WIM válidos são limitados a **2048 caracteres UTF-8**.
- A ferramenta recebe /dev/null em stdin; pipes ficam acima dos descritores 0/1/2, com close-on-exec. A captura funciona mesmo se o chamador fechou stdin/stdout.
- Notas da aplicação atualizadas nos quatro idiomas; sem dependências de runtime novas.

Sete cenários falham na base 0.3.8 e passam na candidata: cancelamento WIM,
cancelamento VHD pelo canal privado, excesso de saída WIM/VHD, falha com pipe
mantido aberto por descendente, bytes inválidos/NUL e canais padrão fechados.
O controle com diagnósticos volumosos e saída válida passa em ambas. Os códigos
134 são falhas das asserções/watchdogs de regressão, não a afirmação de que toda
execução comum do aplicativo abortava. Os fixtures usam ferramentas sintéticas;
a bateria integrada também usa qemu-img/wimlib reais em discos virtuais.

### Resultados

| Ensaio | Resultado e alcance |
| --- | --- |
| Verificações nativas | 24 aprovadas: 23 suítes sem GUI e uma execução gráfica separada em Xvfb isolado |
| Memória do helper | 51 casos ASan/UBSan aprovados; um caso entre UIDs omitido no host e exercitado na VM root |
| Hashes/plataforma | Checks independentes e ASan/UBSan aprovados |
| Backend root em VM | 65 casos aprovados, incluindo os oito fixtures de metadados e o helper de hash exato |
| Interface | 12 configurações nativas + 12 portáteis: quatro idiomas × 800×600, 1024×768 e 1920×1080, básico/avançado e seis cenários simulados por configuração |
| Polkit/PAM real | 13 casos GUI e recusa sem sessão aprovados; autorização, recusa, senha incorreta, cancelamento, sessão instalada inativa e perda abrupta da GUI durante RAW nos dois layouts |

[Evidências e hashes](evidence/linux/tuxflash/stability-0.3.9/summary.json):
[suítes nativas](evidence/linux/tuxflash/stability-0.3.9/native-suites.json),
[regressões](evidence/linux/tuxflash/stability-0.3.9/reproduced-regressions.json),
[sanitizadores](evidence/linux/tuxflash/stability-0.3.9/helper-sanitizers.log),
[backend](evidence/linux/tuxflash/stability-0.3.9/backend-vm.log),
[GUI nativa](evidence/linux/tuxflash/stability-0.3.9/gui-native.json),
[GUI portátil](evidence/linux/tuxflash/stability-0.3.9/gui-portable.json) e
[Polkit/proveniência](evidence/linux/tuxflash/stability-0.3.9/polkit-vm.json).

### Ambiente e limites

O backend usa QEMU/TCG x86-64, kernel 7.0.0-38, 2 vCPUs, 1536 MiB e três arquivos
de disco descartáveis. Os casos Windows/persistência validam provisionamento e
layout sintéticos; não comprovam instalação Windows completa nem persistência
entre boots reais. A execução gráfica foi separada para abrir Xvfb fora da
sandbox comum, sem usar o display pessoal.

O Polkit usa Ubuntu Minimal 24.04 oficial com assinatura e hash conferidos,
kernel 7.0.0-38, 1408 MiB, 2 vCPUs, Xvfb e runtime privado. A rede do guest fica
desativada antes da autenticação. Polkit/PAM e helper root são reais; agente e
seleção/confirmação inicial são preparados pelo fixture. Os 13 casos são de RAW,
não uma qualificação integrada dos novos erros WIM. O layout instalado usa
wrapper root e política original; o portátil usa a ação genérica do Polkit.
Recusa de sessão inativa é qualificada apenas no instalado. Isso não qualifica
pacotes Debian/RPM nem agentes visuais GNOME/KDE. Perda abrupta exige saída
normal 1 do helper, sem sinal.

Não há timeout automático global para consultas ainda em execução: o usuário
pode cancelar. SIGTERM e o pedido SIGKILL após dois segundos não garantem prazo
absoluto para esperas de kernel/hardware. Processos que escapem do grupo/sessão
ficam fora desse controle. Não há rollback de bytes gravados. Consultas da GUI
em outros arquivos e descoberta inicial de montagens não foram convertidas;
a mudança não cria sandbox para parsers QEMU/wimlib.

Instalação Windows completa, persistência real, Windows To Go até o desktop,
Secure Boot, retirada física de USB e uma matriz maior de Linux/arquiteturas
continuam no [roteiro da 1.0](../development/ROADMAP-1.0.md). Os resultados não
significam compatibilidade universal ou equivalência integral ao Rufus Windows.
Veja [captura de metadados](../development/METADATA-CAPTURE.md),
[cancelamento](../development/PROCESS-CANCELLATION.md),
[backend em VM](../development/VM-TESTING.md) e [Polkit em VM](../development/POLKIT-VM.md).

## English

Version 0.3.9 fixes helper metadata queries that could block cancellation or
accept excessive output. UI, icon, credits and GPL-3.0-or-later are preserved;
no physical USB was accessed and no new runtime dependency was added. In-app
notes cover all four supported languages.

VHD/VHDX virtual-size inspection through qemu-img and Windows image-language
queries through wimlib-imagex now use managed capture. Separate stdout/stderr
pipes are drained while cancellation and failed leaders are observed, even if
a descendant holds a pipe open. Failures stop the owned group using the existing
0.3.8 lifecycle. Signal cancellation and private-channel EOF are exercised.

The combined captured output is limited to **4 MiB**, not the external tool's
CPU/memory. Excess returns EFBIG; cancellation ECANCELED. Explicit-length UTF-8
validation rejects invalid bytes and embedded NUL. Valid WIM diagnostics are
bounded at **2048 UTF-8 characters**. Tools receive /dev/null on stdin; capture
FDs stay above stdio and use close-on-exec, including callers with closed stdin/
stdout.

Seven baseline scenarios fail and pass after the fix; the large-diagnostic valid
stream control passes on both. Baseline exit 134 means a regression assertion or
watchdog failed, not every ordinary app invocation crashing. Synthetic metadata
fixtures complement the backend battery that also uses real qemu-img/wimlib.

Passed: **24 native checks** (23 headless suites plus separate isolated Xvfb),
**51 helper ASan/UBSan cases**, independent hash/platform checks, **65 root-backend
VM cases**, **24 GUI configurations**, and **13 real-Polkit GUI cases plus
sessionless denial**. The host skips one cross-UID case exercised in the root VM.
Sanitized evidence and application/helper hashes are linked above.

The backend uses x86-64 QEMU/TCG, kernel 7.0.0-38, 2 vCPUs, 1536 MiB and three
new disposable disk files. Synthetic Windows/persistence provisioning does not
prove a full Windows installation or persistent reboots. GUI checks use isolated
Xvfb rather than the personal display.

The Polkit VM uses signed, checksum-verified Ubuntu Minimal 24.04 userspace,
the same kernel, 1408 MiB, 2 vCPUs, Xvfb and private libraries. Guest networking
is disabled before authorization. PAM/Polkit and root helper are real; the agent
and initial selection/confirmation are fixture-driven. These 13 cases use RAW,
not integrated WIM-failure qualification. The installed layout uses a root wrapper
and original policy; portable uses generic Polkit exec. Inactive-session rejection
is qualified only for installed layout. Native Debian/RPM packages and human
GNOME/KDE agents are not covered. Abrupt frontend loss requires normal helper
exit 1, not termination by signal.

Running queries have no automatic global timeout; the user can cancel. A SIGKILL
request after the two-second grace period is not an absolute bound for kernel/
hardware waits. Escaped groups/sessions are outside this control. Written bytes
are not rolled back. GUI probes in other files and initial mount discovery remain
unchanged; this is not a QEMU/wimlib parser sandbox.

Full Windows installation, real persistence, WTG desktop, Secure Boot, physical
removal and broader distribution/architecture qualification remain open in the
linked 1.0 roadmap. Universal compatibility and full Windows Rufus parity are
not established by these tests. The linked guides document the exact scope.
