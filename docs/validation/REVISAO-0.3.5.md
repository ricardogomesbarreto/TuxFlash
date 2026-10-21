# TuxFlash 0.3.5 — revisão de estabilidade

## Correções

A versão 0.3.4 comparava o tamanho esperado, mas não exigia EOF na origem.
Uma origem RAW maior podia ter apenas seu prefixo aprovado. Em uma origem
comprimida que crescesse, aguardar o descompressor antes de ler EOF podia
bloqueá-lo com o pipe cheio.

A versão 0.3.5 exige o fim exato da origem, recusa crescimento/truncamento e
confere o término bem-sucedido do descompressor antes do evento final de
progresso. Preserva o espaço extra legítimo do dispositivo. O erro original é
preservado durante a limpeza de descritores e do processo auxiliar.

O progresso intermediário usa intervalos de 32 MiB ou 500 ms, evitando uma
linha por fragmento pequeno do pipe. Cancelamento usa `sigaction` sem
`SA_RESTART`; leituras interrompidas da origem, destino e EOF retornam
cancelamento. Isso não promete interromper imediatamente uma chamada de kernel
que esteja em estado de I/O não interrompível.

## Validação desta versão

| Verificação | Resultado |
| --- | --- |
| Compilação nativa e suítes Meson | 18 suítes aprovadas |
| Regressões de conferência | 15 casos nomeados aprovados |
| Codecs | gzip, xz, bzip2 e zstd: conteúdo exato, origem maior, menor e final comprimido danificado |
| RAW | Conteúdo exato, padding no destino, origem maior/menor, destino curto, último byte corrompido e entrada vazia |
| Cancelamento | Leituras bloqueadas de origem, destino e EOF, usando os handlers de produção |
| ASan/UBSan do helper | Aprovado; também incluído no CI |
| Núcleo independente de hashes e sanitizadores | Aprovados |
| Backend real em VM sem rede | 40 casos aprovados, nenhum reprovado |
| Idiomas | Preferência salva e markup das notas 0.3.5 conferidos em pt_BR, en, zh_CN e es |

Os testes de backend usam três arquivos de disco virtual e o helper real,
como root somente dentro do guest. Incluem RAW/comprimidos com invalidação do
cache, captura/restauração, cancelamento, teste de blocos e recusas por disco
protegido, swap, capacidade, identidade, sequência e seleção de partição.
Persistência e Windows To Go nesse conjunto usam imagens sintéticas.
O perfil de VM utilizado teve 1536 MiB de RAM; tentativas menores falharam ao
carregar o initramfs, antes da aplicação, e não foram contadas como aprovadas.

[Evidências](evidence/linux/tuxflash/stability-0.3.5/validation.json),
[suítes nativas](evidence/linux/tuxflash/stability-0.3.5/native-summary.log),
[ASan/UBSan](evidence/linux/tuxflash/stability-0.3.5/helper-sanitize.log) e
[40 cenários em VM](evidence/linux/tuxflash/stability-0.3.5/backend-summary.log).

## Critérios para ampliar a qualificação

- Repetir o job automático completo com a imagem SteamOS de tamanho real nesta versão. A nova tentativa foi interrompida antes de concluir; não conta como aprovação.
- Boot e recuperação completos em Steam Deck físico.
- Sessão Linux persistente real, reinicialização e conferência de dados preservados.
- Matriz de distribuições/arquiteturas e firmware Secure Boot.
- Windows To Go com OOBE e desktop completos.

Os resultados históricos de imagens reais da 0.3.4 continuam no
[relatório correspondente](IMAGENS-REAIS-2026-10-21.md). Eles não são novos
boots físicos da 0.3.5. Nenhum pendrive físico foi gravado nesta revisão.
Não é declarada compatibilidade universal nem paridade integral com Rufus.

## English

Version 0.3.5 fixes prefix-only source acceptance and a possible full-pipe wait
when a compressed source grows. Verification requires exact source EOF,
checks decoder success before its final progress event, preserves cleanup
errors, and allows unused target space. Progress events are bounded by byte
and time intervals. Production signal handlers no longer restart interrupted
reads, enabling cancellation of blocked source, target and EOF reads.

All 18 native suites, 15 named verification regression cases with ASan/UBSan,
independent hash checks, and 40 real-helper backend VM cases passed. Each of
four codecs has exact, grown, shortened and damaged-trailer variants. Release
notes and saved language settings were checked in all four languages.
The VM used 1536 MiB RAM; smaller experimental profiles failed to unpack the
initramfs before application startup and are not counted as passing.

No physical USB was written. Full-size SteamOS automatic compressed-job
qualification for this version, physical Steam Deck recovery, persistent live
sessions across reboot, Secure Boot, a distro/architecture matrix and complete
Windows To Go desktop qualification remain open. The interrupted new SteamOS
attempt is not a passing result; previous real-image evidence retains its own
version and scope. Passing these regressions does not establish universal
compatibility or full Rufus parity.
