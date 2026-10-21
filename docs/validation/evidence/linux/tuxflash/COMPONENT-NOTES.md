# Validação TuxFlash 0.3.0

Os registros pertencem aos binários identificados em summary.json. REA 6.5.0
foi usado para cabeçalhos, seções e símbolos dos dois ELF; a análise por si
só não comprova equivalência funcional. Os testes do backend foram repetidos
em VM com BusyBox estático, módulos do kernel e apenas o runtime portátil,
sem glibc/GTK no sistema hóspede. Foram anexados somente três discos virtuais.

Os três boots usam programas mínimos de confirmação, com Secure Boot desligado.
O caso To Go da bateria usa WIM/hives sintéticos. O teste da ISO Windows real
continua parcial para To Go; o instalador físico foi validado anteriormente.
A GUI usa inventário de dispositivos vazio no teste, para não registrar
identificadores do computador. A enumeração normal do aplicativo foi preservada.
Caminhos locais e identificadores foram removidos dos registros publicados.
