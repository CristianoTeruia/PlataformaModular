# Etapa 3 — teste de diagnóstico do OLED/I2C2

Esta versão é uma alteração experimental para coletar evidências da falha intermitente. **Não é uma correção confirmada**: a compilação e o teste precisam ser feitos no STM32CubeIDE e na Black Pill.

## Alterações nesta versão

- Mantém a exclusão mútua compartilhada entre as transferências do OLED e as leituras/gravações das EEPROMs.
- Protege também `SSD1306_TestConnection()`, `I2C2_Scan_All()` e `SSD1306_Fill()` com o mutex do I2C2, para que essas rotinas não concorram com EEPROM se forem chamadas durante a execução do RTOS.
- Registra o código de erro e o estado da HAL imediatamente quando uma transferência do OLED falha.
- Registra a página e a fase que falharam: `OLEDSTAGE=1` indica comando de página; `OLEDSTAGE=2` indica transferência dos pixels; `OLEDSTAGE=3` indica falha ao adquirir o mutex.
- A resposta de falha de exportação passa a incluir `OLEDERR`, `OLEDSTATE`, `OLEDPAGE` e `OLEDSTAGE`, além dos campos anteriores.
- Não apaga nem formata as EEPROMs.

## Como testar

1. Faça uma cópia de segurança do projeto atual e extraia este ZIP em uma pasta nova.
2. Importe/abra o projeto no STM32CubeIDE e compile. Se houver erro, não grave a placa; envie o erro completo de compilação.
3. Grave o firmware e deixe-o funcionando sem iniciar exportação primeiro. Observe se o texto do OLED se corrompe ou para de atualizar.
4. Se o OLED falhar, anote o tempo aproximado desde a inicialização e o que aparece na tela.
5. Em seguida, tente exportar pela ferramenta Python. Se falhar, copie a linha completa `ERRO,FALHA_LEITURA_EEPROM1,...` do terminal.
6. Não execute comandos de limpeza/apagamento das EEPROMs durante este teste.

## Como interpretar os campos novos

- `HAL=2`: a leitura da EEPROM retornou `HAL_BUSY`.
- `OLED=2`: a última atualização do OLED retornou `HAL_BUSY`.
- `OLEDERR`: `hi2c2.ErrorCode` capturado no instante da falha do OLED (pode preservar erro anterior se HAL retornou `HAL_BUSY`).
- `OLEDSTATE`: estado `hi2c2.State` capturado no instante da falha.
- `OLEDPAGE`: página 0–7 que falhou; `255` se não houve falha de página identificada.
- `OLEDSTAGE`: `1` comando de página, `2` envio de pixels, `3` mutex.
- `OLEDLOCKFAIL`: contagem de falhas ao adquirir o mutex do OLED.
- `PAGINAS`: quantidade de trocas de página realizadas, não número de transferências I2C.

A captura é diagnóstica: se a exportação falhar, envie a linha completa e informe se a tela já estava corrompida antes de iniciar a exportação.
