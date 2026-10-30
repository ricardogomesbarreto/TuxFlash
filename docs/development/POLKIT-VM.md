# Ensaio real do Polkit em VM / Real Polkit VM qualification

## Português Brasil

Este ensaio executa os callbacks da GUI, `LufusJob`, `pkexec`, Polkit/PAM e o
helper real como root em um SO descartável. O usuário da GUI é UID 1000. Um
agente automatizado implementa a API oficial do Polkit e responde com uma senha
aleatória criada apenas na VM; não altera a política para permitir acesso sem
autenticação. O helper e suas bibliotecas vêm do pacote portátil fornecido.

São **13 casos GUI**: autorização, recusa do agente, senha incorreta,
cancelamento durante autenticação e durante escrita em cada um dos layouts
instalado/portátil, mais recusa em sessão local inativa no layout instalado e encerramento abrupto
da GUI durante RAW nos dois layouts.
Há ainda recusa de um chamador sem sessão local. A GUI usa os métodos reais de
início, leitura de eventos, espera e cancelamento; a seleção/confirmação inicial
do trabalho é preparada pelo fixture, sem automatizar um clique humano no diálogo.

O layout instalado usa um wrapper root em `/usr/libexec/tuxflash-helper` e a
política original `auth_admin` para sessão ativa, `no` para demais sessões.
O portátil resolve seu helper dentro do runtime e usa a ação genérica do Polkit.
Os testes revogam autorizações temporárias entre casos para que cada pedido
precise passar por uma nova decisão. Isso não é instalação/qualificação de um
pacote Debian/RPM nem teste de um agente visual GNOME/KDE.

Apenas dois arquivos novos são conectados ao QEMU: raiz ext4 de 3 GiB e USB
virtual de 64 MiB. Não são aceitos destinos físicos. O USB tem limite de escrita
de 8 MiB/s para tornar o cancelamento reproduzível. O controlador root compara
SHA-256 do dispositivo completo nas recusas e no cancelamento antes da escrita;
a autorização positiva exige comparação dos bytes efetivamente gravados. A rede
existe para instalar pacotes no primeiro boot e os links são desativados antes
dos testes. Não há encaminhamento de portas nem compartilhamento do disco do host.

### Reproduzir (host x86-64)

Use um host de desenvolvimento Debian/Ubuntu com QEMU/TCG, SeaBIOS, fakeroot,
e2fsprogs, tar/xz, gpgv e as dependências normais de compilação. Instale também
`libpolkit-agent-1-dev` para compilar o target opcional. Ele não é instalado com
a aplicação e não roda durante `make test`.

```sh
make
# Compilado automaticamente quando os headers opcionais estão disponíveis:
# build/native/test-gui-polkit-vm

qa_source=$(mktemp -d /tmp/tuxflash-polkit-source.XXXXXX)
qa_base=https://cloud-images.ubuntu.com/minimal/releases/noble/release-20261027
for qa_name in SHA256SUMS SHA256SUMS.gpg ubuntu-24.04-minimal-cloudimg-amd64-root.tar.xz; do
  curl --fail --location --output "$qa_source/$qa_name" "$qa_base/$qa_name"
done
qa_work=$(mktemp -d /tmp/tuxflash-polkit.XXXXXX)
python3 linux/tests/vm/polkit/run.py \
  --work "$qa_work" --project "$PWD" \
  --runtime "$PWD/dist/portable/runtime" \
  --binary "$PWD/build/native/test-gui-polkit-vm" \
  --kernel /caminho/para/kernel-legivel \
  --modules /lib/modules/versao-exata-do-kernel \
  --rootfs-tar "$qa_source/ubuntu-24.04-minimal-cloudimg-amd64-root.tar.xz" \
  --sums "$qa_source/SHA256SUMS" --signature "$qa_source/SHA256SUMS.gpg"
```

A chave padrão é `/usr/share/keyrings/ubuntu-cloudimage-keyring.gpg`; use
`--keyring` se ela estiver em outro local confiável. O SHA-256 da origem está
fixado no harness. Mudá-lo exige revisão da nova imagem e sua proveniência.
`--qemu-root` permite fornecer uma árvore de ferramentas extraída em vez da
instalação do host. O kernel precisa ser legível sem sudo e conter suporte
virtio/ext4; os módulos precisam corresponder exatamente a ele.

O runner recusa execução como root no host, pasta ocupada, entradas de dispositivo
e preparação forjada. Ele cria a árvore e a imagem em uma única sessão fakeroot,
confere o ext4 antes de iniciar e remove a cópia de preparação após gerar o disco.
Não modifica offline um sistema de arquivos já iniciado. Nomes com separadores de opções do QEMU também são recusados. Os discos são privados
(modo 0600); dimensione memória/armazenamento do host para a VM e esses arquivos.
O fixture ELF usa um interpretador que só existe no guest e exige a marca
`tuxflash_qa=1` do kernel e o arquivo marcador do guest antes de operar.

O resultado é `results.json`, acompanhado por `provenance.json` e `serial.log`.
A pasta e a imagem contêm uma credencial descartável; mantenha-as privadas.
Publique somente o resumo sanitizado, nunca a imagem, senha, logs sem revisão
ou uma alegação de compatibilidade além do ambiente efetivamente executado.
Um supervisor sem privilégios usa `PR_SET_CHILD_SUBREAPER` e espera o helper
real após a GUI chamar `_exit(0)`: exige saída normal 1, sem SIGPIPE. Isso cobre
a perda abrupta da interface durante RAW; não comprova rollback nem limpeza de
árvores arbitrárias de subprocessos. O teste unitário separado usa uma ferramenta
cooperante e um `umount` falso para verificar a passagem pela limpeza.
Este ensaio não cobre ISO Windows completa, persistência, Secure Boot, retirada
física de USB ou todos os agentes/desktops Linux.

## English

This fixture runs the actual GTK job callbacks, shared `LufusJob`, `pkexec`,
Polkit/PAM and the real privileged helper in a disposable OS. The GUI user is
UID 1000. An automated official-API agent answers with a random guest-only
credential; no rule bypasses authentication. Initial job selection/confirmation
is prepared by the fixture rather than a human clicking the confirmation dialog.

Thirteen GUI cases cover approval, agent dismissal, wrong password, cancellation
during authentication and during writing for installed and portable layouts,
plus inactive-local-session rejection for the installed layout and abrupt GUI
exit during RAW in both layouts. A separate
sessionless caller must also be rejected. The installed wrapper uses the original
application policy; the portable helper uses Polkit's generic exec action.
Temporary grants are revoked between cases. GNOME/KDE visual agents and native
Debian/RPM package installation are not qualified here.

Only a new regular 3 GiB root image and 64 MiB virtual USB image are attached.
A full-device SHA-256 comparison checks refusals/pre-write cancellation, and
successful writes are independently compared. Write throttling makes mid-write
cancellation observable. Networking is used to install guest packages, then guest
links are disabled before testing; no host ports or filesystem shares are added.

Use the commands above on an x86-64 development host with optional
`libpolkit-agent-1-dev`, QEMU/SeaBIOS, fakeroot, e2fsprogs, tar/xz and gpgv.
Provide a readable virtio/ext4-capable kernel and exactly matching modules.
The pinned official cloud archive must pass SHA-256 and signature verification.
The host runner refuses root, nonempty work directories, device inputs and forged
preparation. Images are created in one fakeroot session and checked before boot;
previously booted filesystems are never patched. QEMU option separators in work/firmware paths are rejected. The VM-only ELF and kernel marker
prevent ordinary host execution.

Review `results.json`, `provenance.json` and private `serial.log`. Images contain
an ephemeral credential and must stay private; publish only sanitized evidence.
An ordinary-user subreaper waits for the real root helper after the GUI calls
`_exit(0)`: normal exit 1 is required, without SIGPIPE. This qualifies abrupt
frontend loss during RAW, not rollback or arbitrary subprocess-tree cleanup.
A separate unit test uses a cooperating tool and fake umount to check cleanup.
This test does not qualify full Windows installation, persistent reboots, Secure
Boot, physical removal or every desktop authentication agent.

## Fontes / Sources

- [Imagem Ubuntu Minimal e versão datada / dated image](https://cloud-images.ubuntu.com/minimal/releases/noble/release-20261027/)
- [PolkitAgentListener — API oficial](https://www.freedesktop.org/software/polkit/docs/latest/PolkitAgentListener.html)
- [PolkitAgentSession — autenticação nativa/PAM](https://www.freedesktop.org/software/polkit/docs/latest/PolkitAgentSession.html)

A qualificação da [0.3.8](../validation/REVISAO-0.3.8.md) repete os 13 casos com o helper atualizado. Os testes adicionais de grupos são documentados separadamente no [contrato de cancelamento](PROCESS-CANCELLATION.md). / The 0.3.8 review repeats all 13 cases with the updated helper; additional group tests are documented in the cancellation contract.

A [0.3.9](../validation/REVISAO-0.3.9.md) repete os 13 casos RAW e recusa sem sessão com o helper atualizado. Os oito fixtures de metadados são exercitados separadamente, inclusive na VM root; não são testes Polkit de falhas WIM integradas. / Version 0.3.9 repeats the 13 RAW cases and sessionless denial with the updated helper. Eight metadata fixtures run separately, including in the root VM; these are not integrated WIM-failure Polkit tests.
