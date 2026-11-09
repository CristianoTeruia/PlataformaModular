# V12d - ENS160: descarta amostra sem medicao + diagnostico do sensor

NAO foi compilada para ARM nem testada com o sensor real. Faca Clean + Build antes de gravar.

## O que o diag.py da V12c mostrou (265 s de funcionamento)
sens=9 e sdet=25: ENS160 detectado, porem nao disponivel. ensop=2, enscomm=0 (comunicacao ok),
mas: ultima amostra AQI 0 / TVOC 0 / eCO2 0, ensst=0x01 (sem dado novo), enstmo=18, ensinit=6.
Padrao: apos cada inicializacao o sensor da UM "dado novo" com zeros e depois fica mudo (~4 ciclos
por inicializacao). Um ENS160 que mede nunca informa eCO2 < 400 ppm nem AQI 0.
Conclusao: o sensor se comunica mas NAO esta medindo. Isso nao e algo que o firmware resolve sozinho:
aponta para o modulo ou para a alimentacao dele (veja a lista de verificacoes abaixo).

## Mudancas no firmware
- Amostra com AQI = TVOC = eCO2 = 0 e descartada (nao vira registro nem "sensor disponivel").
- Sensor que responde no I2C mas nao entrega dado passa a contar como DETECTADO: o OLED mostra
  "ERRO:ENS160" e "SENSORES: 2/3" em vez de ignorar o modulo conectado.
- Inicializacao agora le tambem a versao do firmware do sensor (como o ESPHome) e o DIAG mostra
  a temperatura/umidade que o sensor informa estar usando (DATA_T/DATA_RH).
- DIAG/diag.py novos campos: enszero (leituras zeradas descartadas), ensfw (versao do firmware do
  sensor), enset/ensrh (T/UR usadas pelo sensor).

## Como interpretar o novo diag.py
- ENS160 firmware: um sensor vivo informa uma versao (ex.: 5.x.x). 0.0.0 = o microcontrolador interno
  do chip nao respondeu ao comando: suspeite do modulo/chip.
- T/UR que o sensor usa: se o sensor esta calculando, o valor segue a compensacao enviada (a mesma
  temperatura/umidade do AHT). Se ficar 25,0 C / 50,0 % (padrao), o algoritmo nao esta rodando.
- ultima amostra com AQI 1..5 e eCO2 >= 400: sensor medindo de verdade.

## Verificacoes de hardware sugeridas (nesta ordem)
1. Tensao na entrada do modulo (VIN/GND) COM o sistema rodando, com multimetro. Deve ficar estavel; verifique
   quedas. O ENS160 liga um aquecedor e consome dezenas de mA. Confira na serigrafia/vendedor se o modulo
   aceita 3,3 V e/ou 5 V e use a faixa indicada.
2. Alimente o sistema so pela USB (sem bateria/boost) para excluir ruido do conversor.
3. Confira soldas do modulo e jumpers/pads de configuracao (I2C/SPI, endereco).
4. Teste o MESMO modulo em outro controlador com biblioteca de referencia (Arduino/ESPHome). Se tambem
   der zeros, o modulo esta com defeito.

## Teste no PC (nao substitui o teste com o sensor)
840.367 verificacoes, 0 falhas, com um simulador que reproduz o seu caso (um "dado novo" com zeros e
depois silencio). Foram checados: zeros nunca viram amostra valida, o OLED aponta ERRO:ENS160, a
reinicializacao de um sensor mudo ocorre a cada ~3 ciclos e nao a cada ciclo, leitura de versao e eco de
T/UR. Mutacoes propositais (aceitar zeros; nao contar BUSY como detectado) foram detectadas.
O simulador reflete meu entendimento dos drivers de referencia, nao o chip real.

## Lembrete
Com a memoria cheia (eepst=2) o firmware nao grava registros novos. Exporte, responda SIM para limpar.
