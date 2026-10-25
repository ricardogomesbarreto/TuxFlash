# TuxFlash 0.3.6 — critérios de validação / Release readiness

> **Estado em 24/10/2026:** candidata **0.3.6-rc.2** no código de desenvolvimento; ainda não publicada. A última versão pública é **0.3.5**. / **Status as of 2026-10-24:** development candidate **0.3.6-rc.2**, not released. Public version remains **0.3.5**.

## Português (Brasil)

### Alterações de código propostas no GitHub (PR #3)

- Imagens comprimidas: abertura única com `O_NOFOLLOW` e `fstat`; o descompressor recebe o descritor já aberto via entrada padrão. Teste reproduz substituição do caminho por outro arquivo após a abertura, sem modificar mídia física.
- Captura VHD/VHDX: reserva exclusiva do caminho de saída com `O_CREAT | O_EXCL | O_NOFOLLOW` e permissões 0600 antes da criação do contêiner. Teste protege arquivo anterior e rejeita saída simbólica.
- Cancelamento: `write_all()` verifica cancelamento após sinais `EINTR`, sem repetir indefinidamente uma escrita bloqueada. Novo teste usa somente um pipe temporário.
- O PR #2, de verificação de runtime antes da instalação de atalhos, foi incorporado ao `main` após CI verde.

### Cancelamento com mudança de privilégios (rc.2)

O GUI agora envia um pedido de cancelamento ao helper por `stdin` e o helper instala um observador dedicado quando recebe `--cancel-stdin`. A nova suíte testa o byte de cancelamento e EOF usando pipes locais. **Ainda é necessária uma validação real com pkexec/Polkit em VM antes de considerar esse fluxo aprovado para produção**.

### Testes relatados pelo Codex local, ainda não transferidos ao GitHub

O histórico do Codex fornecido pelo desenvolvedor relata **19 suítes nativas**, **43 cenários de backend em VM**, ASan/UBSan, janela 1024×768 sem rolagem e matriz de quatro idiomas em três resoluções, além de cancelamento não privilegiado do helper administrativo. Essas evidências se referem a um **checkout local ainda não acessível nesta integração** e **não demonstram a validação do código deste PR**.

### Requisitos obrigatórios antes de publicar 0.3.6

1. Integrar cuidadosamente as alterações do checkout local, inclusive `test_helper_io.c`, sem sobrescrever seu trabalho em andamento.
2. Executar o CI **sobre o commit integrado final** e confirmar todos os jobs aprovados.
3. Os testes de I/O do GitHub e o smoke test gráfico não destrutivo em Xvfb a 1024×768 passaram no CI; ainda é obrigatório reexecutar as suítes completas de I/O e os **43 cenários em VM** relatados no Codex local sobre o commit reconciliado.
4. O código mantém botões Iniciar/Fechar num rodapé fixo e desabilita rolagem horizontal; o smoke gráfico confirma que a janela cabe em 1024×768. Ainda falta validar visualmente ausência de rolagem vertical no formulário principal, opções avançadas e mensagens longas nos quatro idiomas e nas demais resoluções.
5. Investigar o fundo azul observado no teste em VM; registrar causa e conclusão sem inventar compatibilidade de boot físico.
6. Confirmar traduções e build da candidata 0.3.6-rc.2 e o cancelamento Polkit real; atualizar para a versão 0.3.6 final, README de downloads, manifesto SHA-256, artefato portátil e proveniência apenas quando todos os gates estiverem aprovados.
7. Publicar o release com SHA-256 e repetir um smoke test não destrutivo do pacote **efetivamente gerado**.

Nenhuma operação destrutiva deve ter como destino um disco físico durante a validação automática. Nenhum documento público deve afirmar testes adicionais de ISO Windows 10 em pendrive físico.

## English

The GitHub development candidate 0.3.6-rc.2 adds fd-anchored compressed decoding, exclusive VHD/VHDX capture filename reservation and blocked-write cancellation handling with nonphysical regression tests. Local Codex work reporting 19 native suites, 43 VM scenarios and responsive GUI tests has **not been transferred or reproduced on this candidate**. Do not tag, package, or publish 0.3.6 until the complete local code is reconciled and all gates above pass on the integrated commit. The current public release remains 0.3.5.

[PR #3](https://github.com/ricardogomesbarreto/TuxFlash/pull/3) · [Releases](https://github.com/ricardogomesbarreto/TuxFlash/releases)
