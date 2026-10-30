# Cancelamento de ferramentas / Tool cancellation

## Português Brasil

`run_command_input`, `run_command_progress_range_internal` e `open_decompressor`
usam um grupo isolado por ferramenta. O filho cria o grupo antes de executar o
programa; o pai confirma essa associação antes de publicar o identificador para
o handler de cancelamento. Somente IDs maiores que 1 são usados como grupos.

No cancelamento ou falha, o helper pede SIGTERM ao grupo, espera um período de
dois segundos e pede SIGKILL aos integrantes restantes. O líder fica recolhível
até o último sinal, evitando sinalizar um grupo depois de liberar seu identificador.
O registro é limpo antes de recolher o líder. SIGCHLD é restaurado ao padrão na
entrada do helper para não herdar SIG_IGN/SA_NOCLDWAIT de quem o iniciou.

Comandos concluídos com sucesso preservam serviços de arquivos em segundo plano,
como os necessários a montagens FUSE. Uma falha do líder é detectada mesmo quando
um descendente mantém seu pipe de saída aberto. Há testes separados para sucesso,
falha, cancelamento, descompressão, processos que ignoram SIGTERM e um processo
externo ao grupo que precisa continuar vivo.

O período de dois segundos é uma tolerância antes do pedido SIGKILL, não um prazo
absoluto de retorno. O kernel/hardware pode continuar bloqueando a espera; não há
rollback de bytes escritos. Processos que escapam do grupo por outra sessão/grupo
não são abrangidos. A rotina de desmontagem durante limpeza continua permitida
após cancelamento. Na 0.3.9, as consultas qemu-img/wimlib do helper também usam o coletor gerenciado
([contrato de metadados](METADATA-CAPTURE.md)); a descoberta inicial de montagens
ainda não foi convertida. Não há promessa de controle
universal de toda árvore de processos do sistema.

## English

The command-input runner, progress runner and decoder launcher create isolated
per-tool process groups. The child creates the group before exec; its parent
checks membership before publishing the identifier. Group signals use IDs above 1.

Cancellation or failure sends SIGTERM, allows a two-second grace period and sends
SIGKILL to remaining group members. The leader stays waitable through the last
signal, and its registration is cleared before reaping. Helper entry restores
default SIGCHLD behavior rather than inheriting SIG_IGN/SA_NOCLDWAIT.

Successful commands retain background filesystem services, including FUSE-style
services. Failed leaders are detected even if descendants keep stdout open.
Regression tests check success, failure, cancellation, decoders, SIGTERM-ignoring
tools and an unrelated process that must survive.

The grace period is not an absolute completion deadline: kernel/hardware waits
can persist and no written bytes are rolled back. Processes that move to another
group/session escape this scope. Cleanup unmounts remain allowed after cancellation.
Version 0.3.9 also routes helper qemu-img/wimlib metadata queries through managed
capture; see the [metadata contract](METADATA-CAPTURE.md). Initial mount discovery
remains unchanged. This is not universal process-tree supervision.

## Referências / References

Linux API contracts: [setpgid/getpgid](https://man7.org/linux/man-pages/man2/setpgid.2.html),
[group signals via kill](https://man7.org/linux/man-pages/man2/kill.2.html) and
[waitid/WNOWAIT](https://man7.org/linux/man-pages/man2/waitpid.2.html).
The regression evidence and actual qualification scope are in the
[0.3.8 review](../validation/REVISAO-0.3.8.md) and
[0.3.9 metadata review](../validation/REVISAO-0.3.9.md).
