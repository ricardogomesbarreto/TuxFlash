# TuxFlash 0.4.0-rc1 — candidata / release candidate

Revisão / Review: **2026-11-04**. Base: `7acb398500ded4fd25b7faf8a8793db6d7f17006` (0.3.9).

## Português Brasil

Esta entrega é uma **pré-versão de qualificação**. A 0.3.9 permanece a versão
pública recomendada. O marco 0.4.0 estável exige concluir a instalação Windows
e os ensaios de boot restantes. Não há afirmação de estabilidade universal.

### Alterações

- Novo roteiro reproduzível de mídias reais em discos virtuais descartáveis,
  com origem somente para leitura, conferência independente por SHA-256 e
  registro explícito do alcance de cada resultado.
- Correção do parser do roteiro: conclusão Windows contém texto; RAW contém
  contagem de bytes. Saída zero sem conclusão, duplicação e polling de progresso
  não comprovam sucesso. Nove regressões protegem essas regras e os arquivos.
- Carregamento explícito de UDF, ISO9660 e NLS na VM mínima de teste.
- Instalação não assistida que apaga Disco 0 exige opção explícita no roteiro.
- Identificação da candidata e notas nos quatro idiomas. Interface, ícone,
  créditos e licença foram mantidos; sem novas dependências de runtime.

O helper compilado é idêntico, byte a byte, ao da 0.3.9:
`6f3ea5337051c1d9bddb1a5da83cc120787555203633ea35ad209dc4e833b16a`.
Os testes grandes usam esse binário sem alterar suas proteções. A VM tem kernel
7.0.0-38, 2 vCPUs, 1536 MiB, KVM e destino USB virtual de 16 GiB, sem rede.

### Verificações da candidata

Passaram **25 verificações nativas** (24 suítes sem GUI e a conferência gráfica
separada), **51 casos ASan/UBSan do helper**, checks independentes de hashes e
plataforma e **24 configurações GUI** (12 nativas + 12 portáteis, quatro idiomas
em 800×600, 1024×768 e 1920×1080). Um caso entre UIDs foi omitido no host; a
cobertura root/Polkit de 0.3.9 é histórica e não foi repetida neste ciclo.
O diagnóstico do pacote encontrou todas as ferramentas. A interface continua
com rodapé fixo e sem barras de rolagem visíveis.

### Evidências

| Imagem / ensaio | Resultado |
| --- | --- |
| SteamOS 3.8.14 `.img.bz2` | 8.120.172.544 bytes gravados e verificados; SHA-256 do destino igual à referência expandida independente |
| Kali 2026.2 installer amd64 | 4.802.531.328 bytes gravados e verificados; SHA-256 igual à ISO |
| Zorin 18.1 Core | Execução interrompida pelo serviço da sessão; sem aprovação nova |
| Windows 10, Pro índice 4 | A VM iniciou particionamento/FAT32 e cópia; interrupção durante cópia, sem conclusão nem instalação completa |

[Evidências sanitizadas](evidence/linux/tuxflash/qualification-0.4.0-rc1/summary.json),
[SteamOS](evidence/linux/tuxflash/qualification-0.4.0-rc1/steam.json),
[Kali](evidence/linux/tuxflash/qualification-0.4.0-rc1/kali.json) e
[interrupções](evidence/linux/tuxflash/qualification-0.4.0-rc1/interrupted.json).
Os hashes locais comprovam igualdade, não a autenticidade do fornecedor.
Nenhum pendrive físico foi acessado nesta revisão.

As primeiras tentativas Windows falharam por módulos UDF/NLS não carregados
no guest mínimo; esses problemas do ambiente de QA foram corrigidos. Nenhuma
dessas tentativas é contada como aprovação. O encerramento posterior por
SIGTERM veio do serviço da sessão de testes; não comprova falha do aplicativo.

Boot além do menu, instalação Windows completa, persistência entre boots,
Windows To Go até o desktop, Secure Boot e matriz ampliada de distribuições
continuam no [roteiro da 1.0](../development/ROADMAP-1.0.md).
[Roteiro reproduzível e limites](../development/REAL-MEDIA-QUALIFICATION.md).

## English

This is a **qualification prerelease**. Version 0.3.9 remains the recommended
public release. Stable 0.4.0 still requires complete Windows installation and
remaining image boot qualification. No universal stability claim is made.

A new disposable-disk VM runner checks the copied read-only input, helper
completion protocol and independent full-payload SHA-256. It correctly accepts
Windows text completion while requiring exact RAW byte counts, and rejects
missing/duplicate completion, progress-only records and unsafe files. Nine
guard regressions cover these contracts. UDF/ISO9660/NLS are explicitly loaded
in minimal QA guests. Silent Disk 0 Setup requires explicit opt-in. Four-language
candidate notes preserve UI, icon, credits and license without new runtime dependencies.

Passed: **25 native checks** (24 headless suites plus separate GUI), **51 helper
ASan/UBSan cases**, independent hash/platform checks and **24 GUI configurations**
(12 native + 12 portable, four languages at three screen sizes). One cross-UID
host case is skipped; 0.3.9 root/Polkit results remain historical and were not
rerun. Portable diagnostics found all tools. The footer remains fixed and
scrollbars stay hidden.

The helper is byte-identical to 0.3.9 (digest above). Full SteamOS compressed
write/read-back and independent comparison passed for 8,120,172,544 bytes; Kali
passed for 4,802,531,328 bytes. QEMU/KVM uses kernel 7.0.0-38, 2 vCPUs, 1536 MiB,
a new 16 GiB USB file and no networking or physical host disks.

Zorin and Windows tests were interrupted by the test-session service. Windows
had reached copying after GPT/FAT32 creation; neither completion nor complete
installation is established. Earlier missing UDF/NLS guest modules were fixed
and those failed attempts are not counted. Local hashes establish equality,
not vendor authenticity. No physical USB was accessed. Remaining boot,
installation, persistence, Windows To Go, Secure Boot and portability gates
stay open in the roadmap. Sanitized evidence and reproduction instructions
are linked above.
