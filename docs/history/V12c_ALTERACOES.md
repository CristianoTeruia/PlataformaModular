# V12c - ENS160: nao reiniciar sensor que ja mede + valores e contadores no DIAG

NAO foi compilada para ARM nem testada com o sensor real. Faca Clean + Build antes de gravar.
Mudancas: Core/Src/ens160.c/.h, bh1750.c/.h (ultima leitura), DIAG (app_tasks.c) e diag.py.

## O que o diag.py da V12b mostrou
sens=25 (AHT + BH1750 + ENS160 respondendo), ensop=2 (STANDARD) - o ENS160 passou a funcionar.
Dois pontos: (1) ensinit=4 em ~6 ciclos (o esperado em regime normal e 1);
(2) ensst=0x03: o bit 0x80 ("modo em execucao") NUNCA liga neste modulo, mesmo medindo.

## Causa do ensinit alto
A V12b tratava "bit 0x80 desligado" como falha. Como ele nunca liga aqui, toda espera sem dado novo
virava reinicializacao (com RESET) no ciclo seguinte, reiniciando um sensor que estava medindo e
atrapalhando seu aquecimento. O bit 0x80 deixou de ser usado como criterio.

## Correcao
- So inicializa o sensor quando ele NAO esta em STANDARD (OPMODE lido a cada ciclo) ou apos erro.
- Logo apos iniciar o modo, espera ate 3,2 s pelo primeiro dado (antes 1,6 s); em regime, 1,6 s.
- Sensor medindo que fica sem entregar dado: so reinicializa (com RESET) depois de 3 ciclos seguidos.
- STATER (erro interno): devolve erro e a proxima tentativa usa RESET.

## DIAG (e diag.py) novos campos
ensaqi, enstvoc, enseco2, ensval: ultima amostra do ENS160 (da para conferir SEM exportar CSV);
bhraw: ultima contagem do BH1750 (lux = bruto / 1,2);
enstmo (ciclos sem dado), enserr (erro interno), enscomm (falhas I2C), ensrst (usos de RESET).

## Teste no PC (nao substitui o teste com o sensor)
840.357 verificacoes, 0 falhas, com um simulador que agora imita o que o seu modulo faz: bit 0x80
nunca liga, NEWGPR volta a ligar, primeira medicao lenta. O driver da V12b falha em tres cenarios
(primeira medicao lenta, reinicializacoes repetidas, reinicio com sensor medindo). Mutacao proposital
(voltar a reiniciar quando o bit 0x80 esta desligado) tambem foi detectada.

## O que esperar no diag.py
- ensinit = 1 e ensrst = 0 (ou ensrst = 1 se o sensor precisou de RESET), e estes numeros NAO crescem
  a cada ciclo. Se ensinit crescer sem parar, envie o diag.py inteiro.
- ENS160 ultima amostra com AQI 1 a 5, TVOC e eCO2 plausiveis (eCO2 tipico de ar interno: 400 a 1000 ppm).
- validade: 1 (aquecimento) ou 2 (partida inicial) nas primeiras horas e 0 depois. Use so validade 0.
- BH1750: lux compativel com o ambiente (cubra o sensor para ver cair).

## IMPORTANTE: memoria cheia (eepst=2)
Com a memoria cheia o firmware NAO grava registros novos; por isso o CSV nao tem dados do ENS160.
Para gravar de novo: python exportar_eeprom.py (guarda o CSV) e responda SIM quando perguntar sobre
limpar a memoria. Depois deixe rodar e exporte novamente.
