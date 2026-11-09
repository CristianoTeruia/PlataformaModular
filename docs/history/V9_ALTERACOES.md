# V9 - correcao da causa raiz da corrupcao do OLED (+ mutex do RTC)

NAO foi compilada para ARM nem testada na placa (so checagem de sintaxe no PC).
Faca Clean + Build antes de gravar. Watchdog (IWDG) fica para a V10.

## Causa raiz (hipotese forte, a ser confirmada pelo teste)
Dados do DIAG da V8 (93 min): okc=1823, failc=54, recov=27 -> falhas SEMPRE em pares
(failc = 2 x recov), ~1 episodio a cada 3,5 min (~4,8% dos ciclos de 10 s dos sensores).
- A task de sensores (prioridade acima da do OLED) fazia HAL_Delay(80) no AHT20 e
  HAL_Delay(100) no BMP280 a cada ciclo: ~180 ms de ESPERA OCUPADA sem ceder a CPU.
- Se isso comecava no meio de uma transferencia do OLED (~120 ms a cada 3 s, ~4% do
  tempo), a transferencia ficava suspensa mais que o timeout de 100 ms da HAL. A HAL
  desiste sem gerar STOP, o I2C2 fica BUSY e as transferencias seguintes falham ate o
  periferico ser reiniciado. Quadro meio atualizado = texto corrompido.
- Isso tambem explica o travamento original do OLED (sem recuperacao, ficava preso).

## Mudancas
1. aht20.c, bmp280.c, mpu6500.c, ssd1306.c: HAL_Delay -> App_Delay (osDelay com o RTOS
   rodando). A task de sensores passa a ceder a CPU enquanto espera a conversao.
2. Recuperacao do I2C2/OLED agora dispara apos a PRIMEIRA falha (era a segunda).
3. Mutex dedicado do RTC (leitura em 2 pontos, ajuste RTC_SET). Mutex "folha": nao
   cria deadlock.
4. DIAG novos campos, para confirmar a causa se ainda houver falhas:
   ffst / fferr / ffstate = status HAL, codigo de erro e estado do I2C2 na PRIMEIRA
   falha do ultimo episodio (fferr 32 = timeout; ffstate 32 = READY);
   ffsens = 1 se a task de sensores estava no meio de um ciclo nesse instante.

## Como validar
Deixe rodar 90+ min (antes: ~27 episodios nesse tempo) e rode diag.py.
- Esperado: failc=0 e recov=0.
- Se failc>0: copie o DIAG inteiro; ffsens=1 e fferr=32 confirmam preempcao/timeout.
Antes do diag.py, faca uma EXPORTACAO sem desligar a placa (para medir o pior caso de
stack da defaultTask em stkdef). Quando possivel, use USB sem tirar a bateria: o DIAG
zera a cada reinicio.
