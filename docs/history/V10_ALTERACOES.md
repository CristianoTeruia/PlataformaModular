# V10 - regeneracao pelo CubeMX + watchdog supervisionado

NAO foi compilada para ARM nem testada na placa (so checagem de sintaxe no PC).
Faca Clean + Build antes de gravar. O resultado da V9 (108 min, failc=0, recov=0) valida a
correcao da causa raiz do OLED; a V10 mantem essa logica sem alteracao.

## O que a regeneracao do CubeMX fez com a V9
- Ativou o IWDG corretamente: driver copiado, hal_conf e .ioc atualizados (prescaler 128, reload 4095).
- APAGOU quase todo o main.c da V9: tasks, mutexes, DIAG, recuperacao do I2C2, RTC_SET, etc.
  estavam FORA dos blocos USER CODE. (O original esta na pasta Backup do seu projeto gerado e no ZIP da V9; as pastas Backup nao vieram nesta V10.)
- Apagou o ajuste do FreeRTOSConfig.h (heap 32 KB, deteccao de overflow, hook de malloc).
- Apagou CDC_IsTxBusy() em usbd_cdc_if.c e as declaracoes extern em stm32f4xx_it.c.
- Reduziu a stack da defaultTask para 512 bytes e moveu MX_USB_DEVICE_Init() para dentro dela.
- Passou a gerar MX_RTC_Init() que ZERA A HORA (2000-01-01) A CADA BOOT: isso anularia a CR2032.

## O que a V10 faz
1. Todo o codigo de aplicacao saiu do main.c para Core/Src/app_tasks.c (+ app_tasks.h) e
   Core/Src/app_watchdog.c (+ .h). Esses arquivos o CubeMX nao toca. No main.c so ha
   chamadas curtas dentro de blocos USER CODE. Futuras regeneracoes nao apagam mais nada.
2. Ajustes do FreeRTOS agora ficam no bloco USER CODE Defines do FreeRTOSConfig.h; a stack
   da defaultTask (4096 bytes) esta no .ioc (1024 palavras) e no main.c, coerentes.
3. MX_RTC_Init: bloco Check_RTC_BKUP preserva a hora quando DR0 == 0x32F2 (CR2032).
4. Removido o envio "USB CDC funcionando!" antes da USB estar inicializada.
5. WATCHDOG (IWDG, ~16 s nominal; 11 a 31 s reais por causa da LSI):
   - Cada task registra atividade. O IWDG so e alimentado se a task de sensores (prazo 30 s)
     e a do OLED (prazo 10 s) estiverem dentro do prazo; a defaultTask executa a verificacao.
   - Exportacao, CLEAR e esperas USB tambem chamam a verificacao, entao operacoes longas
     NAO causam reset.
   - Durante a inicializacao anterior ao kernel o IWDG e alimentado manualmente.
   - O debugger pausado congela o IWDG (nao reinicia a placa durante depuracao).
   - Falhas fatais (estouro de stack, falha de heap, HardFault) gravam o motivo no
     registrador de backup e o IWDG reinicia a placa sozinho (antes ficava piscando para sempre).
6. DIAG novos campos: rstcsr, wdgrst, lastfault, wdgstale. O diag.py decodifica.
7. Comando WDTTEST (teste do watchdog): faz a task de sensores parar de responder.

## Testes
A) 10 min normal: `python diag.py --port COM3` -> wdgrst=0, wdgstale=0, falhas do OLED = 0.
B) Exportar CSV e rodar diag.py logo depois (stkdef mostra o pior caso da pilha).
C) Teste do watchdog: `python diag.py --port COM3 --cmd WDTTEST`. A placa deve reiniciar
   sozinha em 40 a 65 s (OLED apaga e volta). Depois: diag.py -> ultimo reset: IWDG,
   wdgrst=1 e a hora do RTC deve continuar certa.
D) Conferir que, apos reiniciar, o log continua e a hora nao voltou a 2000-01-01.
