# Contribuir / Contributing

## Português (Brasil)

Use issues e pull requests em https://github.com/ricardogomesbarreto/TuxFlash.
Descreva o problema ou a melhoria, a versão e os passos para reproduzir.
Não publique dados pessoais, seriais de dispositivos, credenciais ou caminhos privados.
Para relatos de segurança, consulte [SECURITY.md](../../SECURITY.md).

### Ambiente e verificação

Referência de desenvolvimento: Ubuntu 24.04 ou ambiente compatível com GTK 4.14
e libadwaita 1.5. O pacote pronto tem runtime próprio; o clone usa dependências
de desenvolvimento e ferramentas de mídia do sistema.

```sh
sudo apt update
sudo apt install build-essential pkg-config meson ninja-build gettext \
  libgtk-4-dev libadwaita-1-dev libsoup-3.0-dev libjson-glib-dev libhivex-dev
make
make check
make check-sanitize
```

`make` compila e executa as suítes Meson, preparando o runtime local.
Testes unitários e de hashes não devem exigir root ou um pendrive.
Os testes em VM são separados: [VM-TESTING.md](VM-TESTING.md).
Nunca aponte um teste automatizado para discos físicos.

Conforme o modo de mídia, o backend também exige util-linux, dosfstools,
ntfs-3g, e2fsprogs, 7-Zip, wimlib, hivex, QEMU, F3 e ferramentas de compressão.
Use `./tuxflash.sh doctor` para verificar a instalação local.
Não execute operações destrutivas apenas para verificar a compilação.

Mantenha mudanças focadas, atualize os testes de comportamento afetados e
documente limites reais. Atualize as quatro traduções quando mudar textos da
interface. Preserve créditos, proveniência e licenças de Rufus, Lufus e componentes.
Contribuições ao código da aplicação seguem GPL-3.0-or-later; componentes
separados mantêm suas licenças.

## English

Use issues and pull requests at https://github.com/ricardogomesbarreto/TuxFlash.
Describe the problem or improvement, version and reproduction steps. Remove
personal information, device serials, credentials and private paths from reports.
See [SECURITY.md](../../SECURITY.md) for security reports.

The development baseline is Ubuntu 24.04 or an environment providing GTK 4.14
and libadwaita 1.5. Install the development dependencies using the commands above.
`make` builds the application, runs the Meson suites and prepares the local
runtime. Run `make check` and `make check-sanitize` for the independent core tests.
These checks require neither root nor a USB drive. VM tests are a separate
workflow described in [VM-TESTING.md](VM-TESTING.md). Never target physical disks
from automated tests.

Media operations additionally need the tools appropriate to their mode:
util-linux, dosfstools, ntfs-3g, e2fsprogs, 7-Zip, wimlib, hivex, QEMU, F3 and
compression tools. Run `./tuxflash.sh doctor` to inspect the local installation.
Destructive media creation is not required to validate a build.

Keep changes focused, test affected behavior and document actual limitations.
Update all four UI languages when changing interface text. Preserve upstream
credits, provenance and licenses. Application-code contributions use
GPL-3.0-or-later; separately licensed components retain their terms.
