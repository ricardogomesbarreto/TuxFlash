# Revisão TuxFlash 0.3.3 — 20/10/2026

Comparação local com os ZIP fornecidos: Rufus Windows e Lufus 0.2.2.
Os recursos nativos de gravação, captura e mídias Windows já existiam no Lufus.
Esta atualização reforça a integração, segurança, tratamento de erros, créditos
e documentação; não declara superioridade geral nem paridade integral com Rufus.

Veja a matriz técnica em [README](../README.pt-BR.md). Os resultados finais desta
rodada serão registrados em `analysis/linux/tuxflash/releases-0.3.3/validation.json`.
Os relatórios anteriores continuam históricos, associados a seus próprios binários.

## Correções verificáveis

* Sistema: submontagens protegidas, todos os vínculos na listagem e travessia
  de slaves sysfs no helper; inspeção de swap com recusa quando indeterminável.
* USB: confirmação guarda identidade, tamanho e sequência; tamanho exato,
  BLKROGET e identidade do descritor verificados no helper. Sequência depende
  do kernel. Ferramentas externas ainda abrem caminhos; não há prova de eliminação
  de todas as corridas entre verificações e acessos externos.
* Integridade: verificação RAW compara o conteúdo após invalidar o cache.
  O sucesso visual exige COMPLETE, saída lida e término correto do helper.
* Cancelamento: espera de filhos repete após EINTR e mantém o filho sinalizável
  durante a espera do descompressor. CLI trata término por sinal.
* Polkit: políticas atual/legada exigem autenticação administrativa; nenhum
  arquivo do sistema ou permissão permanente é instalado nesta revisão.
* Portátil: apenas tema genérico/ícone da aplicação; execução explícita com Bash
  nos geradores BCD, com falhas propagadas antes de atualizar o BCD.
* Descrição Linux, link Lufus, créditos e notas traduzidos em quatro idiomas.

## Escopo dos testes

Testes unitários incluem topologia de vários discos, limites de caminhos,
swap, espera interrompida, recusa de arquivo regular e falha de gerador BCD.
A VM usa apenas discos virtuais, WIMs/ISOs sintéticos e marcas mínimas de firmware.
A GUI usa enumeração vazia e processo simulado para verificar os estados finais,
sem gravar dispositivos. Evidências públicas não incluem dados do computador.

FFU, Windows To Go até o desktop, Secure Boot e persistência real entre boots
continuam sem qualificação completa. Boot físico depende de reiniciar a máquina
no pendrive e não é comprovado por uma leitura posterior bem-sucedida.

## Resultados desta rodada

* 16 suítes Meson aprovadas, sem falhas.
* 40 cenários no helper portátil real em discos virtuais e 3 boots mínimos de firmware aprovados.
* 12 cenários gráficos (Sobre, Créditos e Novidades em quatro idiomas), incluindo
  simulação de sucesso, erro e ausência de confirmação de conclusão.
* ASan/UBSan aprovados em topologia, montagem/espera e durabilidade; detecção de
  vazamentos desativada, portanto não há alegação de validação por LeakSanitizer.
* Políticas Polkit atual e legada conferidas; análise ELF executada com REA.
* USB físico: duas leituras do primeiro GiB, com cache invalidado, coincidiram.
  O helper recusou sequência de disco divergente em verificação sem gravação.
  Isso não comprova toda a capacidade, gravação ou boot físico.
* Gravação física nova não executada: a revisão automática exigiu aprovação
  explícita da ISO Linux Mint 22.3 MATE para sobrescrever o pendrive.

As primeiras rodadas expuseram dependências de interpretador em scripts BCD
e uma comparação no harness realizada depois de desmontar; foram corrigidas
antes da rodada final aprovada. Não se apresentam essas falhas iniciais como
qualificação de uma sessão real Windows To Go.
