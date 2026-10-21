Atualização 0.3.3: descrição Linux, crédito direto ao repositório Lufus,
novos estados e notas de segurança traduzidos nos quatro idiomas.
Consulte [revisão](REVISAO-0.3.3.md). Registros abaixo preservam o histórico.

Atualização 0.3.2: notas da versão e textos legais traduzidos nos quatro idiomas.
Veja [notas da versão](NOTAS-DA-VERSAO.md) e [avisos de terceiros](THIRD_PARTY_NOTICES.md).
Os registros abaixo incluem o histórico das versões anteriores.

# Idiomas e créditos — TuxFlash 0.3.1

Ricardo Barreto integra os créditos de desenvolvimento desta versão.
Seu nome aparece em Sobre → Créditos, com o perfil clicável
https://github.com/ricardogomesbarreto. O aviso de direitos autorais também
inclui Ricardo Barreto. As atribuições anteriores de Rufus/Lufus foram mantidas.
O projeto-base Lufus também tem uma entrada própria nos créditos, com link direto
para https://github.com/openresearchtools/lufus, traduzida nos quatro idiomas.

## Escolher idioma

Clique no ícone de idioma ao lado do botão Sobre, no cabeçalho. Escolha
Português (Brasil), English, 中文（简体）ou Español e clique em Salvar.
Feche e abra novamente o TuxFlash para aplicar. Não é preciso gravar nem
formatar um dispositivo para alterar o idioma. A troca é bloqueada durante
gravação, download ou cálculo de somas de verificação.

A preferência fica em `$XDG_CONFIG_HOME/tuxflash/settings.ini`, ou na pasta
`.config/tuxflash` do usuário quando XDG_CONFIG_HOME não estiver definido.
É salvo apenas o código do idioma; nenhuma conta ou identificação de hardware.
Sem preferência, a aplicação escolhe um dos quatro idiomas conforme o sistema
e usa inglês quando não encontra correspondência. Chinês usa escrita simplificada.

## Traduções

192 mensagens próprias incluem os campos, botões, confirmações de apagamento,
progresso, seletor de idioma e texto principal de licença. Os catálogos GTK,
GLib e libadwaita vieram dos repositórios oficiais GNOME, com origem e SHA-256
em `po/toolkit/SOURCES.json`. A fonte Noto Sans CJK acompanha o pacote para
renderizar chinês sem depender das fontes do computador.

Protocolos, saída técnica das ferramentas externas e avisos/licenças de
terceiros continuam em sua linguagem original. O idioma da aplicação não
muda o idioma de instalação do Windows; este depende da ISO selecionada.

Os testes verificam as quatro traduções ao reiniciar o processo, preservação
de outras configurações, preferência inválida e arquivo de configuração
corrompido. A validação gráfica verifica os créditos e o link sem abrir
um navegador, as quatro janelas e a cobertura de caracteres chineses.
A inclusão pública do nome e do GitHub foi solicitada pelo desenvolvedor;
a anonimização dos dados do computador continua preservada.
