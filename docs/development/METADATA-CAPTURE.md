# Captura de metadados / Metadata capture

## Português Brasil

Na 0.3.9, `virtual_disk_size` (qemu-img) e `windows_image_default_language`
(wimlib-imagex) usam `capture_tool_metadata`. A alteração é do helper; não altera
os programas de inspeção da GUI que vivem em outros arquivos.

O coletor drena stdout e stderr separadamente com poll, acompanha cancelamento e
a saída do líder e aplica o contrato de grupos da 0.3.8. A falha não depende de
EOF, que um descendente poderia impedir. O stdin da ferramenta é /dev/null;
canais de controle não são consumidos pelo processo externo. Descritores de
captura ficam acima de 2 e usam close-on-exec, inclusive se stdin/stdout do
chamador estavam fechados.

O limite combinado é de **4 MiB de bytes capturados**, não um limite de memória
ou CPU do processo externo. Excesso é EFBIG; cancelamento é ECANCELED. O texto
é validado com comprimento explícito para recusar UTF-8 inválido e NUL interno.
Uma saída inválida não chega ao parser como um prefixo aparentemente válido.
Erros do WIM preservam diagnósticos válidos com até **2048 caracteres UTF-8**.

Sucesso exige fim das saídas e saída zero da ferramenta. Falhas encerram o grupo;
a espera preserva o identificador do líder até o último sinal. Consultas ainda
em execução não recebem um timeout automático global: o usuário pode cancelar.
Os limites de kernel/hardware e processos que escapam do grupo continuam os do
[contrato de cancelamento](PROCESS-CANCELLATION.md). A descoberta inicial de
montagens não foi convertida. Isso não é um sandbox dos parsers QEMU/wimlib.

## English

Version 0.3.9 routes qemu-img virtual-capacity inspection and wimlib image-language
queries through the helper's managed metadata collector. Unprivileged GUI probes
in other source files are outside this change.

Separate stdout/stderr pipes are polled while cancellation and leader failure are
observed. Missing EOF from a descendant cannot hide failure. Tools receive
/dev/null on stdin; control channels remain private. Capture descriptors use
close-on-exec and stay above stdio, including callers that closed descriptors 0/1.

The combined **4 MiB captured-byte limit** does not bound the external tool's own
CPU/memory. Excess returns EFBIG and cancellation ECANCELED. Explicit-length UTF-8
validation rejects embedded NUL as well as invalid encoding. WIM error diagnostics
are limited to **2048 UTF-8 characters** without cutting a character.

Success requires output EOF and a zero exit. Failures stop the owned group while
the leader remains waitable through the last signal. Running queries have no
automatic global timeout; the user can cancel. Kernel waits and escaped groups
retain the linked cancellation contract's limits. Initial mount discovery is
unchanged; this is not a QEMU/wimlib parser sandbox.

## Referências / References

- [GIO communicate_utf8](https://docs.gtk.org/gio/method.Subprocess.communicate_utf8.html)
- [GLib UTF-8 validation with an explicit byte count](https://docs.gtk.org/glib/func.utf8_validate.html)
- [Linux poll](https://man7.org/linux/man-pages/man2/poll.2.html)
- [Resultados da 0.3.9 / 0.3.9 results](../validation/REVISAO-0.3.9.md)
