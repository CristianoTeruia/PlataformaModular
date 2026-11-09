# Etapa 1 — Integração inicial do FreeRTOS (CMSIS-RTOS2)

## Estado desta entrega

- A configuração gerada pelo STM32CubeMX para CMSIS_V2 e os fontes do FreeRTOS foram mantidos.
- O `main.c` foi recuperado a partir da versão funcional anterior e integrado ao escalonador.
- A aplicação continua sequencial em uma única tarefa (`StartDefaultTask`); não foram criadas tarefas independentes para sensores, OLED, USB ou EEPROM.
- A rotina `RecoverSensorI2C1()`, os includes/estado da aplicação e o fluxo de comandos RTC/CLEAR/EXPORT foram preservados.
- O comportamento anterior de `MX_RTC_Init()` foi preservado; a geração automática de data/hora zero não foi incorporada.
- A função auxiliar `CDC_IsTxBusy()` foi restaurada porque fazia parte da interface USB anterior.
- A inicialização USB continua antes da criação do escalonador, na mesma ordem funcional anterior. O intervalo de espera do laço principal foi convertido de `HAL_Delay(20)` para `osDelay(20)` dentro da tarefa.

## Limites e validação necessária

Esta entrega ainda não foi compilada neste ambiente (não há toolchain ARM instalada) nem testada na placa. Abra o projeto no STM32CubeIDE e faça um clean/build antes de gravar. Se houver erros, envie o log completo.

Depois de compilar, valide nesta ordem:
1. O firmware inicia e o LED continua alternando durante a aquisição.
2. O OLED exibe estado dos sensores e continua alternando páginas.
3. USB CDC enumera e responde a `RTC_SET`, `EXPORT` e `CLEAR` conforme o protocolo anterior.
4. A aquisição continua ocorrendo no período configurado e os registros existentes nas EEPROMs permanecem intactos.
5. Desconectar sensores não afeta o OLED/EEPROM, que permanecem em I2C2.

## Atenção

A lógica da aplicação ainda é bloqueante e roda em uma única tarefa. Esta etapa valida somente a entrada do escalonador; a separação em tarefas, filas e mutexes será feita depois dos testes básicos. Não executar `CLEAR` em EEPROMs com dados importantes sem backup.
