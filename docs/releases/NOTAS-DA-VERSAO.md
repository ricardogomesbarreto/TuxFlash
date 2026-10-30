# TuxFlash 0.3.8 — 2026-10-30

Cancelamento e falhas de ferramentas gerenciadas abrangem os descendentes de seu grupo, com SIGTERM e escalonamento após dois segundos. Comandos bem-sucedidos preservam serviços em segundo plano. A saída com falha é detectada mesmo com stdout mantido aberto por um descendente; EIO evita mensagens de sucesso incorretas. SIGCHLD é restaurado para permitir recolhimento seguro dos filhos. Notas traduzidas nos quatro idiomas, créditos e GPL-3.0-or-later preservados.

Managed-tool cancellation/failure covers descendants in their group, using SIGTERM and escalation after two seconds. Successful commands preserve background services. Failed leaders are detected despite descendant-held stdout; EIO prevents stale success errors. Default SIGCHLD enables safe child reaping. Four-language notes, credits and GPL-3.0-or-later are preserved.

[Revisão, resultados e limites / review, results and limits](../validation/REVISAO-0.3.8.md).

---

# TuxFlash 0.3.7 — 2026-10-29

- O helper administrativo ignora SIGPIPE para poder tratar o cancelamento e passar pela limpeza quando a interface desaparece.
- Comandos auxiliares recusam sucesso quando falha a entrega da entrada ou há cancelamento, inclusive se a ferramenta termina com código zero.
- Três regressões reproduzem os problemas da base anterior; testes ampliados de GUI, memória e VM acompanham a correção.
- Notas na aplicação traduzidas para Português (Brasil), English, 中文（简体） e Español. Créditos e GPL-3.0-or-later preservados.

Veja [resultados e limites da 0.3.7](../validation/REVISAO-0.3.7.md).

The administrative helper ignores SIGPIPE so frontend loss can be handled through cancellation and cleanup. Auxiliary commands cannot report success after failed input delivery or cancellation, even when a tool exits zero. Three regressions reproduce the earlier defects; GUI, sanitizer and VM coverage accompanies the fix. In-app notes are translated into all four supported languages. Credits and GPL-3.0-or-later are preserved. See the linked review for the exact results and limits.

---

# TuxFlash 0.3.6 — 2026-10-28

Janela adaptável sem barras visíveis, ações fixas, canal privado de cancelamento e captura VHD/VHDX com descritores fixados. Integra o lançador único e a instalação segura do atalho. Veja [alterações, validação e limites](../validation/REVISAO-0.3.6.md).

Responsive window without visible scrollbars, fixed actions, private cancellation and pinned VHD/VHDX capture. Includes the unified launcher and safe menu integration. See [changes, validation and limits](../validation/REVISAO-0.3.6.md).

---

# TuxFlash 0.3.6-rc.2 — cancelamento e preparação do lançamento

> **Histórico de desenvolvimento:** candidata anterior, substituída pela versão final 0.3.6 abaixo.

- O botão Cancelar envia um pedido por pipe ao helper, inclusive quando iniciado com `pkexec`, sem tentar enviar sinais a um processo root com privilégios de usuário comum.
- O helper monitora `stdin` apenas quando recebe `--cancel-stdin`, preservando o comportamento do CLI sem essa opção; um byte de cancelamento ou EOF sinaliza a thread principal para interromper I/O bloqueada.
- Os testes automatizados cobrem canal de cancelamento, perda da interface e parâmetros do processo sem tocar em dispositivos físicos.
- **Pendente:** validação de ponta a ponta com `pkexec` real e autorização Polkit em VM; conciliação do checkout local do Codex e reprodução das 43 operações em VM. O cancelamento de operações root ainda não deve ser considerado certificado no ambiente real.

## English

**0.3.6-rc.2 is a development candidate, not a stable release.** The GUI sends cancellation through a stdin control pipe and the helper listens only with `--cancel-stdin`; cancellation or EOF interrupts blocked I/O. Non-destructive tests cover this protocol. **Real Polkit authorization and privileged cancellation in a VM are still pending**, as are the local Codex changes and the full 43-scenario backend regression. Current published version: 0.3.5.

---

# TuxFlash 0.3.6-rc.1 — candidata de segurança e integridade

> **Histórico de desenvolvimento:** candidata anterior à versão final 0.3.6.

- Imagens comprimidas são abertas com `O_NOFOLLOW` e `fstat`, e o descompressor consome o mesmo descritor, sem reabrir o caminho.
- Capturas VHD/VHDX reservam arquivos de saída com `O_CREAT | O_EXCL | O_NOFOLLOW` e permissões 0600, preservando arquivos existentes.
- Escritas bloqueadas respeitam o cancelamento após `EINTR`; testes de regressão utilizam arquivos temporários e pipes, sem pendrives físicos.
- Rodapé GTK fixo para status/progresso e botões Iniciar/Fechar; o formulário se organiza em duas colunas quando há espaço e não usa rolagem horizontal. Uma rolagem vertical continua possível em janelas pequenas ou opções avançadas expandidas.
- Smoke test gráfico em Xvfb a 1024×768 aprovado no CI, comprovando abertura e dimensões da janela sem acesso à mídia física (não comprova todos os controles em todas as resoluções).
- Regressões incluem substituição de caminho, links simbólicos, colisão de saída e contêineres reais com `qemu-img`.
- A janela sem rolagem, o cancelamento do helper privilegiado e os 43 cenários VM relatados pelo Codex local **ainda exigem integração e validação no mesmo commit** antes da publicação final.

Veja [critérios de aprovação](../validation/REVISAO-0.3.6.md).

## English

**0.3.6-rc.1 is a development release candidate, not a published stable package.** This candidate anchors compressed input to a previously opened descriptor, reserves VHD/VHDX output paths exclusively, and honors cancellation during blocked writes. Non-destructive tests cover pathname replacement, symlink rejection, output collisions and `qemu-img` container creation. Local Codex UI and privileged-helper work must be reconciled and retested before publishing 0.3.6. The latest public version remains 0.3.5.

---

# TuxFlash 0.3.5 — integridade e cancelamento

- A verificação exige que a origem termine exatamente no tamanho gravado; recusa imagens maiores ou menores e preserva o espaço extra legítimo no destino.
- A conferência consome o fim do fluxo antes de aguardar o descompressor, evitando espera com o pipe cheio quando a origem cresce.
- O progresso é emitido por intervalo de bytes/tempo; o evento final vem após o sucesso do descompressor, incluindo sua conferência de integridade.
- Handlers sem `SA_RESTART` e tratamento de `EINTR` permitem cancelar leituras bloqueadas de origem, destino e fim do fluxo.
- Novo teste com 15 casos nomeados, incluindo quatro codecs e quatro variações por codec; ASan/UBSan agora integra o CI do helper.
- Notas traduzidas nos quatro idiomas. Leia [a revisão e os limites](../validation/REVISAO-0.3.5.md).

## English

Verification now requires the source to end exactly at the committed length,
consumes EOF before waiting for a decoder, and checks its successful exit before
the final progress event. This rejects changed lengths and damaged compressed
streams while allowing unused target space. Progress traffic is bounded by byte
and time intervals. Non-restarting signal handlers let cancellation interrupt
source, target and EOF reads. Fifteen named regression tests cover four codecs,
length changes, corruption, padding and cancellation; helper ASan/UBSan checks
also run in CI. Release notes are translated into all four interface languages.
See the linked review for evidence and remaining qualification work.

---

# TuxFlash 0.3.4 — imagens reais e persistência

- Corrigida a cópia dos links internos da ISO do Zorin OS 18.1 para mídia persistente.
- Aliases internos são materializados; caminhos externos, ciclos e arquivos especiais são recusados.
- A árvore e o tamanho expandido são conferidos antes de particionar o destino de persistência.
- Sete novos cenários de regressão, 17 suítes nativas e conferência com ASan/UBSan.
- Notas traduzidas em Português (Brasil), English, 中文（简体） e Español.
- Consulte o [relatório de imagens reais](../validation/IMAGENS-REAIS-2026-10-21.md) para os resultados exatos de SteamOS, Kali e Zorin. Configurar persistência não comprova uma sessão persistente depois de reiniciar.

## English

Fixed Zorin OS 18.1 persistent-media creation by materializing bounded internal
ISO aliases. Escapes, cycles and special files remain rejected; preflight tree
and expanded-capacity checks now precede persistent-media partitioning.
Seven regression scenarios, 17 native suites, ASan/UBSan checks and four-language
release notes accompany this fix. See the linked image report for the exact
write, firmware and persistence validation scope.

---

# TuxFlash 0.3.3 — segurança e integração

* Proteção de submontagens do sistema, volumes com múltiplos vínculos e swap ativo.
* Capacidade exata, estado somente leitura e sequência do disco quando disponível.
* Revalidação antes de ferramentas externas e conferência do descritor RAW/leitura posterior.
* Espera de filhos resistente a sinais; sucesso da GUI condicionado ao término e à leitura da saída.
* Correção da execução dos geradores BCD-SYS com Bash e propagação de falhas.
* Polkit legado com autenticação administrativa; mensagens de lançadores mais claras.
* Empacotamento sem ícones de aplicações instaladas no computador.
* Frase de aplicação nativa Linux e repositório Lufus explícito nos créditos, nos quatro idiomas.
* README atualizados com diferenças comprovadas, requisitos e limitações.

Aplicação continua GPL-3.0-or-later; autores e licenças originais preservados.
Veja [revisão e testes](../validation/REVISAO-0.3.3.md). As notas abaixo são históricas.

---

# TuxFlash 0.3.2 — notas da versão

Abra Sobre e escolha a seção de novidades para ler as notas dentro da aplicação.
A área Legal continua separada, dedicada às licenças. As duas áreas acompanham
Português do Brasil, English, 中文（简体） ou Español escolhido na aplicação.

## Atualizações e melhorias

* Nova página de notas da versão em Sobre.
* Textos principais, direitos autorais e resumos das licenças dos componentes traduzidos.
* Explicação explícita de que o TuxFlash continua livre e de código aberto, sob GPL-3.0-or-later.
* Licenças de terceiros e créditos preservados; nenhuma substituição das licenças originais.
* Correção dos avisos sobre bibliotecas/ferramentas incluídas no pacote portátil.
* Inclusão da licença OFL-1.1 da fonte Noto Sans CJK no índice visual.
* Origem, avanços recentes e limitações documentados sem declarar paridade completa com Rufus Windows.

## Rufus → Lufus → TuxFlash

| Projeto | Papel e diferenças |
| --- | --- |
| Rufus | Projeto original Windows de Pete Batard e colaboradores. Fontes e recursos Windows originais preservados. |
| Lufus | Base nativa C/GTK Linux recebida, com helper separado para operações privilegiadas. |
| TuxFlash | Evolução independente dessa base, com identidade própria, contribuições de Ricardo Barreto, quatro idiomas, pacote portátil e correções de integração. Sem Wine. |

A janela GTK segue a organização do Lufus, não a interface Win32 original.
As notas citam o histórico 0.3.0 (identidade, empacotamento e melhorias de mídia)
e 0.3.1 (idiomas e créditos). Esta atualização não reimplementa o backend de gravação.
FFU, sessão real de Windows To Go, Secure Boot e persistência Linux entre boots
continuam com os limites descritos em COMPATIBILIDADE.md e TUXFLASH.md.

## Licença

A aplicação mantém GPL-3.0-or-later. A licença original em LICENSE.txt não foi
alterada. Os textos traduzidos na interface são explicações; as condições,
avisos e exclusões de garantia são os dos documentos originais.
Dependências e ativos de terceiros preservam suas próprias licenças.
Veja THIRD_PARTY_NOTICES.md e o índice de avisos no pacote portátil.
