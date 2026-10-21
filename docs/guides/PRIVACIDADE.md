# Privacidade dos arquivos do projeto

Os registros locais foram anonimizados a pedido do usuário. Foram removidos
nomes pessoais, caminhos da pasta pessoal, nome do computador, números de série,
modelos dos dispositivos, inventários de discos e variáveis de ambiente herdadas.
A limpeza inclui as cópias históricas em `.linux-port-backups`.

Os arquivos de inventário agora contêm apenas aviso de remoção. Os resultados
técnicos gerais dos testes foram mantidos. Os scripts históricos de operações
físicas foram desativados porque seus destinos e identificadores locais foram
removidos; para novas operações, use a aplicação e escolha explicitamente a
imagem e o dispositivo. Caminhos ilustrativos não são destinos reais.

Os relatórios e manifests de etapas antigas refletem seus respectivos momentos;
alguns conteúdos foram anonimizados e não são mais cópias integrais dos logs
originais. O manifesto atual de entrega foi recalculado para os arquivos atuais.
A limpeza não conservou novas cópias dos dados pessoais removidos no projeto.

A GUI passou a usar o nome de conta inicial genérico `User`, editável pelo usuário,
e registra o nome do arquivo selecionado sem anexar o caminho da pasta pessoal.
A identificação ao vivo de dispositivos continua disponível para selecionar e
proteger corretamente o destino das operações.

Novas compilações e diagnósticos podem gerar informações locais novamente.
Antes de compartilhar esses arquivos, revise logs e inventários; os diretórios
de build locais e os backups não são necessários para executar o runtime.

A auditoria desta continuação está em
`../analysis/linux/lufus/privacy-labels-2026-10-20/summary.json`.
