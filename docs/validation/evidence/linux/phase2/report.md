# Etapa 2: núcleo de hash original compilado em Linux

## Resultado

O build Linux agora compila diretamente `src/hash.c` com
`RUFUS_HASH_CORE_ONLY`. A ferramenta usa os algoritmos MD5, SHA-1, SHA-256 e
SHA-512 originais, em vez do backend OpenSSL da etapa anterior. O build produz
`librufus-hash.a`, reutilizável por futuras partes da interface nativa.

As definições antes contidas em `rufus.h` foram movidas para `hash_core.h`, que
não inclui tipos de interface nem APIs de sistema. O Windows continua incluindo
as mesmas definições por esse cabeçalho. O build completo Windows não foi
executado; a preservação desse caminho foi avaliada por revisão do diff.

`hash_compat.h` fornece os tipos mínimos do núcleo e acessos de bytes sem
desalinhamento. Os diálogos, I/O Windows, verificação de certificados, PE256 e
bancos de revogação permanecem no caminho Windows. **O núcleo Linux compilado
nesta etapa não implementa validação de Secure Boot.**

## Evidência obtida com REA

Foi reutilizado o REA 6.5.0, extraído do ZIP fornecido pelo usuário e compilado
na etapa anterior. As chamadas ao serviço real do REA usaram o provedor GNU
objdump, com resultados completos e sem truncamento:

| Arquivo de evidência | Observação |
| --- | --- |
| `rea-before-disassembly.json` | Binário da etapa 1: chamadas `EVP_Digest*` do backend OpenSSL |
| `rea-after-disassembly.json` | Binário da etapa 2: rotinas originais, sem chamadas `EVP_Digest*` |
| `rea-core-symbols.json` | Símbolos do objeto compilado diretamente de `src/hash.c` |
| `rea-core-disassembly.json` | Instruções do objeto do núcleo compartilhado |

Esses resultados incluem SHA-256 e IDs de evidência do REA. O script não
executou os alvos para analisá-los. A evidência do binário anterior identifica
o caminho usado antes da atualização; a versão anterior é preservada no backup
de instalação. Os arquivos `native-linkage.json` e `toolchain.json` foram obtidos
por ferramentas locais, separadamente do REA. `readelf` não encontrou dependência
de `libcrypto` ou `libssl` no novo executável.

Isto é análise estática e portabilidade orientada pelos fontes disponíveis.
Não houve desempacotamento ou descompilação completa do executável Windows 4.15,
nem prova de equivalência binária com a revisão de fontes 4.16.2412.

## Erro MD5 reproduzido e corrigido

No código recebido, `md5_write` preenchia o bloco pendente com `memcpy(p, buf,
num)` quando `len < num`. Com duas chamadas de um byte, o segundo acesso
tentava ler 63 bytes de uma alocação de um byte.

O arquivo `md5-short-write-before.json` registra a execução real do teste C
com AddressSanitizer, que terminou com erro de leitura fora do buffer. A troca
de `num` por `len` corrige o tamanho lido. `sanitizer-after.json` registra o mesmo
teste após a correção, terminando com código 0. A reprodução foi feita no núcleo
compilado em Linux; não foi demonstrado um caminho de exploração no aplicativo
Windows.

`HashBuffer` também passou a recusar ponteiro de entrada nulo com comprimento
positivo e a permitir explicitamente mensagem vazia sem chamar `memcpy` com
entrada nula. As assinaturas públicas e os valores de hash válidos permanecem
compatíveis.

## Validação

`make -f Makefile.linux check` passou:

* Sete testes de integração da camada Linux existente e seus testes C de
  cancelamento e alteração da origem durante leitura/cópia.
* Cinco grupos de testes do núcleo: vetores conhecidos, limites de padding,
  entradas desalinhadas, incrementos de tamanho variável e argumentos inválidos,
  além de chamadas concorrentes com contextos independentes.
* Comparações dos quatro algoritmos com Python `hashlib` nos modos escalar e
  acelerado. A CPU desta máquina confirmou SHA-1 e SHA-256 acelerados; nenhum
  teste forçou instruções não suportadas pelo processador.

`make -f Makefile.linux check-sanitize` passou com ASan e UBSan no teste de
escritas curtas. A detecção de vazamentos foi desabilitada nessa execução; não
há alegação de validação de vazamentos. Os resultados completos estão nos JSONs.

O código foi compilado com avisos tratados como erros. A arquitetura validada
é Linux x86-64 little-endian; outras arquiteturas e o build completo Windows
não foram testados. A interface gráfica e as operações em USB não foram
alteradas nem validadas nesta etapa. O Rufus completo ainda não roda nativamente
em Linux.

## Revisão e restauração

`change.patch` registra as mudanças de código. `before-sha256.json` contém o
estado anterior desta etapa e `delivery-sha256.json` identifica os arquivos da
entrega. A instalação preserva as versões anteriores dos arquivos modificados
em `.linux-port-backups/phase2/`, na raiz do projeto.

Os próximos componentes permanecem sendo a seleção segura de dispositivos,
particionamento/formatação e integração dos diálogos originais com um toolkit
Linux, seguidos da validação das operações de boot e dos recursos Windows.
