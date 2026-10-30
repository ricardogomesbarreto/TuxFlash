# TuxFlash 0.3.8 — revisão e validação / review and validation

Publicação / Release: **2026-10-30**. Base: `243bf4af1586de7bd1ef1ac30390362dcd4c7eb1` (0.3.7).

## Português Brasil

A 0.3.8 corrige travamentos de cancelamento e limpeza de ferramentas auxiliares,
melhora a identificação de falhas e mantém a interface, ícone, créditos e licença
GPL-3.0-or-later. Não foram acessados pendrives físicos neste ciclo.

### O que mudou

- Ferramentas dos executores de entrada/progresso e descompressores recebem grupos isolados. Cancelamento e falhas abrangem descendentes que permaneçam nesses grupos, sem sinalizar um processo externo usado como controle nos testes.
- O helper pede SIGTERM, permite dois segundos para encerramento cooperante e depois pede SIGKILL. Mantém o líder recolhível até o último sinal para não usar seu identificador depois da liberação. SIGCHLD é restaurado ao padrão, incluindo quando o chamador o ignorava.
- Grupos de comandos que falham são encerrados; serviços em segundo plano são preservados após sucesso. Isso evita interromper serviços necessários a sistemas de arquivos.
- O executor de progresso verifica a saída do líder mesmo quando um descendente mantém stdout aberto. Uma falha não fica oculta pela ausência de EOF.
- Saídas anormais de ferramentas retornam EIO quando não existe erro mais específico, evitando mensagens baseadas em um errno zerado ou antigo. Pipes de progresso usam close-on-exec, com redirecionamento verificado.
- Notas na aplicação traduzidas para Português Brasil, English, 中文（简体） e Español.

Oito cenários falham na base 0.3.7 e passam na candidata: ferramenta resistente
com progresso, entrada bloqueada, descompressor resistente, descendente que
sobrevive ao líder, errno incorreto, serviço deixado por comando com falha,
pipe aberto após falha e SIGCHLD ignorado herdado. O controle de serviço após
sucesso passa em ambas. Os códigos 134 registrados são falhas das asserções/
watchdogs de regressão; não indicam que toda execução comum do aplicativo abortava.

### Resultados

| Ensaio | Resultado e alcance |
| --- | --- |
| Suítes nativas | 23 verificações aprovadas, agregando os testes sem GUI e a execução gráfica isolada; as oito suítes afetadas pelo helper foram repetidas após a última correção |
| Memória do helper | 43 casos ASan/UBSan aprovados; um caso entre UIDs é omitido no host e exercitado na VM |
| Hashes/plataforma | Checks independentes e ASan/UBSan aprovados |
| Backend root em VM | 57 casos aprovados com o helper exato; ferramentas resistentes, isolamento, falha com descendente e signal handling integram a bateria |
| Interface | 12 configurações nativas + 12 portáteis: quatro idiomas × 800×600, 1024×768 e 1920×1080; básico/avançado e seis cenários simulados por configuração |
| Polkit/PAM real | 13 casos GUI e recusa sem sessão aprovados: autorização, recusa, senha incorreta, cancelamento, sessão instalada inativa e saída abrupta durante RAW nos dois layouts |

[Evidências sanitizadas e hashes](evidence/linux/tuxflash/stability-0.3.8/summary.json):
[suítes nativas](evidence/linux/tuxflash/stability-0.3.8/native-suites.json),
[reprodução das falhas](evidence/linux/tuxflash/stability-0.3.8/reproduced-regressions.json),
[sanitizadores](evidence/linux/tuxflash/stability-0.3.8/helper-sanitizers.log),
[backend](evidence/linux/tuxflash/stability-0.3.8/backend-vm.log),
[GUI nativa](evidence/linux/tuxflash/stability-0.3.8/gui-native.json),
[GUI portátil](evidence/linux/tuxflash/stability-0.3.8/gui-portable.json) e
[Polkit/proveniência](evidence/linux/tuxflash/stability-0.3.8/polkit-vm.json).

### Alcance

O teste gráfico não abriu display dentro da sandbox comum; foi concluído
separadamente em Xvfb isolado. O resultado agregado diferencia essa execução.
O backend usa QEMU/TCG x86-64, kernel 7.0.0-38, 2 vCPUs, 1536 MiB e três arquivos
de disco descartáveis. Os casos Windows/persistência usam imagens sintéticas,
sem instalação real de Windows nem sessão persistente entre reinícios.

O Polkit usa Ubuntu Minimal 24.04 com kernel 7.0.0-38, QEMU/TCG, 1408 MiB,
2 vCPUs, Xvfb e runtime privado. Origem oficial assinada e hash fixado; rede do
guest desativada antes da autenticação. Polkit/PAM e helper root são reais;
agente e seleção/confirmação inicial são preparados pelo fixture. O layout
instalado usa wrapper root e a política original; o portátil usa a ação genérica
do Polkit. Isso não qualifica um pacote Debian/RPM nem agentes visuais GNOME/KDE.
Recusa inativa é qualificada apenas no layout instalado. Casos de perda da GUI
usam `_exit(0)` durante RAW e exigem saída normal 1 do helper, sem sinal.

### Limites mantidos

O período de dois segundos antecede o pedido SIGKILL; não garante prazo absoluto
quando kernel/hardware bloqueiam a espera. Não existe rollback de bytes escritos.
Processos que mudam de grupo/sessão escapam do escopo. Consultas por GSubprocess
e descoberta inicial de montagens não foram convertidas para o novo executor.
O teste de serviço em segundo plano é um controle sintético, não uma qualificação
de todo driver FUSE. Limpeza de desmontagem continua permitida após cancelamento.

Instalação Windows completa, persistência real entre boots, Windows To Go até o
desktop, Secure Boot, retirada física de USB e uma matriz maior de Linux/arquiteturas
continuam no [roteiro da 1.0](../development/ROADMAP-1.0.md). Os testes não demonstram
compatibilidade universal nem equivalência integral ao Rufus Windows.
Veja [o contrato de cancelamento](../development/PROCESS-CANCELLATION.md),
[o backend em VM](../development/VM-TESTING.md) e [o Polkit em VM](../development/POLKIT-VM.md).

## English

Version 0.3.8 fixes managed-tool cancellation hangs, failed-command descendants,
incorrect error reporting and inherited ignored SIGCHLD. UI, icon, credits and
GPL-3.0-or-later are preserved, with in-app notes in all four languages. No physical
USB was accessed.

Command-input/progress tools and decoders get isolated groups. Cancellation or
failure sends SIGTERM, allows two seconds and requests SIGKILL. The unreaped
leader anchors the identifier until the last group signal. Successful background
filesystem services survive. The progress reader detects a failed leader even
when descendants keep stdout open. Nonzero exits report EIO when no more specific
error exists. Eight baseline regression scenarios fail and pass after the fix;
the successful-service control passes on both versions.

Passed: **23 native checks** (aggregated headless suites and separate isolated
Xvfb execution), **43 helper ASan/UBSan cases**, independent hash/platform checks,
**57 root-backend VM cases**, **24 GUI configurations**, and **13 real-Polkit GUI
cases plus sessionless denial**. The host skips one cross-UID case that runs in
the root VM. The eight helper suites were repeated after the final code change.
Evidence and exact application/helper fingerprints are linked above.

The root backend uses x86-64 QEMU/TCG, kernel 7.0.0-38, 2 vCPUs and 1536 MiB.
Synthetic provisioning does not establish a complete Windows installation or
persistence across real reboots. The Polkit VM uses verified Ubuntu 24.04 userspace,
the same kernel, 1408 MiB, Xvfb and private libraries. Actual PAM/Polkit and helper
code are used; the agent and initial selection/confirmation are fixture-driven.
Installed-layout tests use a root wrapper rather than a qualified native package;
portable execution uses the generic Polkit exec action. Inactive-session rejection
is qualified only for the installed layout. Guest networking is disabled before
authorization cases. Abrupt RAW frontend exit requires normal helper exit 1, not
a signal. Visual GNOME/KDE agents are not covered.

The grace period precedes a SIGKILL request; it is not an absolute completion
bound for kernel/hardware waits. No written bytes are rolled back. Processes that
leave the group/session escape this scope. GSubprocess metadata probes and initial
mount discovery are not converted. The background-service control is synthetic,
not every FUSE driver. Cleanup unmounts remain allowed after cancellation.

Full Windows installation, persistent reboots, WTG desktop, Secure Boot, physical
removal and broader Linux/architecture qualification remain open. These results
are not universal compatibility or a 1.0 certification. The linked guides describe
reproduction and the exact cancellation contract.
