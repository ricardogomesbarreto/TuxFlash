Atualização 0.3.2: notas da versão e textos legais traduzidos nos quatro idiomas.
Veja [notas da versão](../releases/NOTAS-DA-VERSAO.md) e [avisos de terceiros](../legal/THIRD_PARTY_NOTICES.md).
Os registros abaixo incluem o histórico das versões anteriores.

# Atualização TuxFlash 0.3.1

Créditos de Ricardo Barreto com [GitHub](https://github.com/ricardogomesbarreto)
e escolha de Português (Brasil), English, 中文（简体） e Español.
Clique no ícone de idioma do cabeçalho, salve e reabra a aplicação.
Veja [idiomas e créditos](../guides/IDIOMAS.md). Os registros abaixo descrevem a versão anterior.

# TuxFlash 0.3.0 — entrega e validação

**Bootable USB Creator for Linux**

Transforme imagens ISO em pendrives inicializáveis com simplicidade e segurança.

## Execução

Na raiz do projeto, execute `./tuxflash.sh`. O lançador usa a cópia portátil
em `linux/portable/`. `./tuxflash.sh doctor` verifica 25 ferramentas.
`python3 instalar-menu.py` registra o ícone original no menu do usuário;
`python3 instalar-menu.py --remove` remove o atalho. Não é necessário root
para abrir a janela. A gravação pede autorização por Polkit/pkexec.

O arquivo `dist/TuxFlash-0.3.0-linux-x86_64.tar.xz` pode ser extraído em outra
pasta; execute `./tuxflash` dentro dela. O pacote inclui o código da aplicação,
bibliotecas privadas e avisos de licença. Não depende de Wine ou FUSE.
Requer Linux x86-64, shell POSIX em `/bin/sh`, `/proc`, `/sys`, X11 ou Wayland
para a janela, suporte do kernel aos sistemas de arquivos usados e acesso
a USB. A GUI requer Polkit/pkexec; em ambientes sem Polkit use a CLI
como administrador. A ferramenta de menu requer Python 3.

## Alterações

* Nome, identificação GTK, metadados e ajuda agora usam TuxFlash.
* O pinguim enviado está incorporado ao executável e no atalho, sem mudar os pixels.
* A organização da janela GTK recebida foi mantida, com cabeçalho e descrição solicitados.
* Particionamento/sistema de arquivos exibidos acompanham o modo selecionado.
* DD/RAW permite a conferência após gravar. Opções sem implementação ficam desativadas.
* NTFS tenta o driver ntfs3 e oferece fallback para ntfs-3g.
* Pré-verificação de ferramentas ocorre antes de operações de gravação.
* O ambiente das bibliotecas privadas não é transferido ao pkexec do sistema.
* Fontes, catálogos, TLS com certificados públicos, fontes tipográficas e ferramentas acompanham o pacote.

## Testes desta versão

| Verificação | Resultado |
| --- | --- |
| Compilação e testes Meson | 12 suítes aprovadas |
| Backend portátil em Linux mínimo, sem GTK/glibc do hóspede | 34 casos aprovados |
| Marcas mínimas de boot BIOS, UEFI e layout To Go | 3 aprovadas; Secure Boot desligado |
| Janela, ícone incorporado, FAT32/NTFS/RAW e conta genérica | Aprovado |
| Bibliotecas da janela | GTK, libadwaita e glibc privadas conferidas |
| Mudança de pasta com espaços e & | Aprovado |
| Atalho de menu e integridade do PNG original | Aprovado em diretório temporário |
| REA | Cabeçalhos, seções e símbolos dos ELF finais inspecionados |

As evidências e os hashes estão em `../analysis/linux/tuxflash/`.
Os 772 fontes/recursos Win32 originais permaneceram inalterados nesta etapa.
Os registros entregues não incluem identificadores do computador.
Nenhuma mídia física foi gravada nesta etapa. O instalador Windows existente
continua com o rótulo derivado da ISO e teve boot físico confirmado pelo usuário
na etapa anterior.

## Limites conhecidos

Não foi comprovado funcionamento em qualquer Linux ou com qualquer ISO.
O pacote pronto é x86-64; outras arquiteturas exigem recompilação e validação.
A cópia DD de uma ISO não híbrida não a torna automaticamente inicializável por USB.
A interface é a GTK recebida do Lufus, sem paridade integral com Rufus Win32.
Rótulo manual, escolha livre de particionamento/sistema de arquivos/cluster
e outras opções ainda estão desativados; o backend define esses parâmetros
conforme o modo. O rótulo Windows acompanha o nome da ISO respeitando FAT32/NTFS.

FFU permanece sem backend. Windows To Go com Windows real ainda não chegou
a uma sessão de desktop validada, nem passou por reinício/persistência.
A tentativa adicional desta etapa mostrou início do Windows, mas o monitor
da VM excedeu o tempo de resposta; o resultado é inconclusivo.
Os testes sintéticos de preparação To Go passaram. Secure Boot, sessão real
com persistência Linux e outras distribuições não têm qualificação completa.

## Compilar

`make -f Makefile.linux native` compila a GUI/CLI e o helper com Meson.
`linux/build-local.sh --sysroot DIRETORIO` permite desenvolvimento com pacotes
extraídos, sem instalação global. Para produzir o pacote privado, use
`python3 linux/tools/build-portable.py SAIDA --busybox BUSYBOX_ESTATICO`
com ferramentas e dependências de desenvolvimento disponíveis. Consulte
`--help` para o diretório opcional de ferramentas. A saída deve ser nova
e estar fora do projeto. O empacotador não redistribui pkexec/sudo/su.

Este é um derivado independente; as licenças e os créditos Rufus/Lufus e
dos demais componentes foram preservados.
