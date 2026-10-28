# Caminho para a versão 1.0 / Road to version 1.0

Revisão / Review: **2026-10-28**. Versão pública / Public release: **0.3.6**.
Este documento define critérios de aprovação; não declara recursos pendentes como concluídos.
This document defines acceptance criteria; it does not mark pending features complete.

## Português Brasil

A base atual tem GUI e CLI nativas, backend compartilhado, proteção de discos do
sistema, verificação de gravação e pacote portátil x86-64. A 0.3.6 passou em
21 suítes nativas, 45 casos de backend em VM, 31 casos do helper com ASan/UBSan
e 24 configurações de interface. Veja a [revisão da 0.3.6](../validation/REVISAO-0.3.6.md).
Esses resultados não comprovam instalação completa, todos os firmwares ou todas as distribuições.

### Critérios que ainda faltam

| Prioridade | Frente | Critério de aprovação para a 1.0 | Estado comprovado |
| --- | --- | --- | --- |
| Crítica | Segurança de destino | Repetir as proteções com o pacote candidato: disco do sistema, swap, volumes dependentes, dispositivo trocado/desconectado e montagem alterada após confirmação. Nenhuma escrita no alvo errado; nenhuma falha reportada como sucesso. | Proteções e regressões existentes; falta ampliar cenários e qualificar o pacote final. |
| Crítica | Helper e Polkit | Executar GUI → pkexec → helper real em VM com usuário comum; aprovar, negar e cancelar autenticação, cancelar I/O bloqueada e encerrar a interface. Confirmar que a política não autoriza sessões indevidas. | Fluxo automatizado real aprovado em 11 casos GUI e recusa sem sessão; agentes visuais e encerramento abrupto ainda pendentes. Veja [Polkit](../validation/POLKIT-2026-10-28.md). |
| Alta | Falhas e recuperação | Simular falta de espaço, erros de leitura/escrita, imagem alterada/truncada, retirada do USB e falha no commit final. Saída sem falso sucesso, limpeza de recursos e próximo trabalho possível. | Vários casos unitários já cobertos; falta matriz integrada GUI/CLI e pacote candidato. |
| Alta | Imagens Linux reais | Gravar e comparar as imagens completas suportadas com o binário final; iniciar o sistema/instalador além do menu em BIOS/UEFI conforme a imagem. Repetir SteamOS comprimido completo. | Evidências reais históricas; falta qualificação completa da candidata final. |
| Alta | Instalador Windows | Recriar a mídia com o binário final e concluir uma instalação em disco virtual descartável. Qualificar os modos FAT32/WIM dividido, UEFI:NTFS e BIOS/UEFI anunciados, incluindo seleção de edição e instalação não assistida em VM com discos extras preservados. Investigar a captura azul anterior com logs e reprodução controlada. | Boot até escolha de idioma; instalação completa pendente. Controle virtual não comprovou correção da captura física. |
| Alta | Persistência Linux | Iniciar uma sessão real, criar um arquivo, reiniciar pela mesma mídia e confirmar que o arquivo permanece; testar as famílias anunciadas. | Layout sintético aprovado; persistência entre boots reais pendente. |
| Alta | Windows To Go | Aplicar a imagem, completar a configuração inicial, chegar ao desktop e reiniciar mantendo os dados. | Provisionamento e boot parciais; desktop completo pendente. |
| Alta | Portabilidade | Executar o pacote em sistemas limpos de pelo menos duas famílias de distribuição; registrar versão, kernel, sessão gráfica e dependências. Testar GUI/CLI e autorização em cada ambiente declarado suportado. | CI de compilação Ubuntu 24.04 e runtime privado; matriz de execução ainda insuficiente. |
| Média | Interface e idiomas | Revisar fluxos completos em pt_BR, en, zh_CN e es; mensagens de erro, teclado, foco, escala de tela, telas pequenas e opções avançadas acessíveis. | Traduções e encaixe testados; revisão completa dos fluxos ainda pendente. |
| Alta | Distribuição e manutenção | Automatizar a criação e conferência do pacote, garantir fontes correspondentes/licenças, documentar instalação/remoção e gerar hashes verificáveis para a candidata exata. Nenhum defeito crítico/alto conhecido aberto no escopo estável. | Pacote e hashes conferidos; falta consolidar uma rotina de release reproduzível e a qualificação final. |

### Escopo da primeira 1.0

A 1.0 precisa publicar uma matriz explícita de distribuição, arquitetura, imagem,
modo de escrita e firmware. O pacote pronto atual é **Linux x86-64**; outras
arquiteturas exigem seus próprios builds e testes antes de serem anunciadas.

Cada recurso apresentado como estável precisa cumprir o respectivo critério
acima. Recursos ainda em qualificação devem aparecer como experimentais na
interface, na CLI e na documentação. Não basta a opção estar visível ou o helper
terminar a cópia. A proposta de manter os modos existentes permanece; esta lista
não remove funcionalidades.

Secure Boot só entra na matriz comprovada após ensaios próprios com imagens e
firmwares compatíveis, incluindo os caminhos Windows anunciados. FFU continua
fora do backend Linux atual. Compatibilidade universal e equivalência integral
com o Rufus Windows não são critérios verificáveis para este lançamento.

### Sequência de trabalho

Os marcos abaixo são planejamento; não são versões já publicadas nem um calendário.

1. **0.3.7 — falhas e cancelamento:** ampliar regressões da GUI/CLI, testar Polkit real em VM e cenários de troca/desconexão do destino.
2. **0.4 — mídias completas:** qualificar imagens reais com o pacote exato e completar uma instalação Windows; investigar divergências com evidências.
3. **0.5 — recursos avançados:** concluir persistência entre boots e Windows To Go até o desktop; recuperação física Steam Deck quando houver hardware disponível para esse ensaio.
4. **0.6–0.8 — portabilidade e entrega:** executar a matriz de distribuições, consolidar empacotamento e revisar os quatro idiomas e acessibilidade.
5. **0.9 / 1.0-rc — congelamento:** fechar o escopo, evitar novos recursos e repetir os critérios sobre os mesmos artefatos que serão entregues.
6. **1.0:** publicar somente após os critérios obrigatórios do escopo estável passarem, com evidências e limitações atualizadas.

### Progresso após a 0.3.6

Foram acrescentadas regressões da interface para seis cenários: cancelamento,
erro do helper, confirmação seguida de falha, saída zero sem confirmação,
conclusão válida e conclusão confirmada que coincide com pedido de cancelamento.
A mesma janela é reutilizada entre os casos, verificando limpeza e recuperação
dos controles. Passaram nas 12 combinações de idioma/resolução com bibliotecas
nativas e nas mesmas 12 com o runtime portátil. São processos simulados usando
pipes; não executam o helper real, Polkit ou gravação em dispositivo.
[Evidência sanitizada](../validation/evidence/linux/tuxflash/road-to-1.0/gui-writer-lifecycle.json).
A versão pública continua 0.3.6; isso é avanço dos testes no desenvolvimento.

### Como registrar um resultado

Cada qualificação precisa identificar a versão/commit, SHA-256 do pacote/helper
e da imagem, ambiente, modo, firmware, resultado esperado/observado e evidência
sanitizada. Diferenciar cópia, conferência, menu de boot, sessão/instalação completa
e reinicialização. Um resultado antigo conserva sua própria versão e alcance.
O inventário de evidências não deve incluir seriais, caminhos pessoais ou credenciais.

Regressões automáticas devem usar arquivos, pipes ou discos virtuais descartáveis.
Ensaios físicos exigem alvo dedicado identificado; não se usa o disco do sistema.
A cobertura de hardware que falta depende de execução nesse hardware e não pode
ser substituída por uma porcentagem estimada de progresso.

## English

The current foundation provides native GUI/CLI, a shared backend, system-disk
protection, write verification and an x86-64 portable package. Version 0.3.6 passed
21 native suites, 45 VM backend cases, 31 nonprivileged helper sanitizer cases and
24 GUI configurations. See the [0.3.6 review](../validation/REVISAO-0.3.6.md).
These results do not establish complete installations or universal firmware/distribution support.

### Remaining acceptance criteria

| Priority | Area | Required evidence |
| --- | --- | --- |
| Critical | Target safety | Candidate-package tests for system/swap/dependent volumes, device replacement/removal and mount changes after confirmation. No wrong-target writes or false success. |
| Critical | Helper/Polkit | Real GUI → pkexec → helper authorization in a VM as an ordinary user: approval, denial, auth cancellation, blocked-I/O cancellation and frontend exit. Cross-UID and real automated Polkit/PAM tests pass; human desktop agents and abrupt-exit qualification remain open. See the [Polkit report](../validation/POLKIT-2026-10-28.md). |
| High | Failure recovery | Integrated GUI/CLI faults: ENOSPC, read/write errors, modified/truncated input, USB removal and final commit failure. Cleanup and a subsequent operation must work. |
| High | Real Linux images | Full write/read-back with the final binary, including compressed SteamOS; boot beyond menus to the advertised installer/session in the applicable BIOS/UEFI modes. |
| High | Windows installer | Recreate media with the final binary and finish installation on a disposable virtual disk. Qualify FAT32/split WIM, UEFI:NTFS and advertised BIOS/UEFI paths, including edition selection and unattended installation with extra VM disks left intact. Investigate the earlier physical-USB blue capture with reproducible evidence/logs. |
| High | Persistence/Windows To Go | A real persistent Linux session must retain a created file across reboot. Windows To Go must complete initial setup, reach the desktop and survive reboot. Current evidence is partial. |
| High | Portability | Test the package on clean systems from at least two distribution families, recording version, kernel, desktop and dependencies. Qualify GUI, CLI and authorization for each declared environment. |
| Medium | UI/languages | Complete flow/error review in pt_BR, en, zh_CN and es, including keyboard/focus, display scaling, small screens and advanced controls. Current layout tests cover only part of this contract. |
| High | Delivery | Reproducible packaging and artifact checks, matching source/licenses, installation/removal instructions and candidate-specific hashes; no known critical/high defect left in the stable scope. |

### Scope and milestones

The first 1.0 must publish an explicit distribution/architecture/image/mode/firmware
matrix. The ready-made package currently targets **Linux x86-64**. Other
architectures require their own builds and execution evidence. Existing modes are
retained; each feature advertised as stable must pass its gate. Unqualified
features must be marked experimental consistently in the GUI, CLI and docs.

Secure Boot requires its own image/firmware qualification. FFU remains outside
the current Linux backend. Neither universal compatibility nor complete Windows
Rufus parity is an established release claim.

Planned milestones, without promised dates: **0.3.7** failure/cancellation and real
Polkit tests; **0.4** full media/Windows installation; **0.5** persistent reboots
and Windows To Go desktop; **0.6–0.8** portability, delivery and UI/language review;
**0.9 / 1.0-rc** scope freeze and exact-artifact regression; **1.0** only after all
mandatory stable-scope gates pass. Physical Steam Deck recovery additionally
requires suitable hardware for that qualification.

Record version/commit, package/helper/image SHA-256, environment, mode, firmware,
expected/observed outcome and sanitized evidence. Distinguish copying, verifying,
boot menu, full session/installation and reboot. Historical results retain their
original scope. Automated regressions use pipes/files/disposable virtual disks;
physical tests use an identified dedicated target. Missing hardware evidence
cannot be replaced by an estimated completion percentage.

### Development progress after 0.3.6

Six GUI lifecycle scenarios were added: cancellation, helper error, completion
followed by failure, zero exit without completion, valid completion and confirmed
completion racing with a cancellation request. Reusing the window checks cleanup
and control recovery between cases. All 12 language/resolution configurations
passed with native libraries and the same 12 with the portable runtime. Children
are pipe-only mocks; the real helper, Polkit and physical targets are not exercised.
See the [sanitized evidence](../validation/evidence/linux/tuxflash/road-to-1.0/gui-writer-lifecycle.json).
The public release remains 0.3.6; this is development test progress.

### Atualização Polkit / Polkit update — 2026-10-28

A bateria real em VM passou nos 11 casos GUI e na recusa sem sessão. Ela cobre autorização/recusa, senha incorreta, cancelamento antes/depois de autorizar e sessão instalada inativa. Agentes visuais GNOME/KDE, encerramento abrupto e demais ambientes continuam abertos. [Relatório e limites](../validation/POLKIT-2026-10-28.md).

The real VM battery passed 11 GUI cases and sessionless denial. Human GNOME/KDE agents, abrupt exit and other environments remain open. See the linked report and limits.
