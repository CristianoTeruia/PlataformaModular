# Notas da primeira refatoração (revisão inicial)

## Alterações

- Separados os drivers de AHT20, BMP280, MPU6500, SSD1306 e EEPROM 24LC512 em arquivos `.c` próprios.
- Criados headers correspondentes e `app_state.h` / `app_state.c` para tipos, constantes e estado compartilhado.
- O `main.c` ficou responsável pela inicialização dos periféricos e pelo fluxo principal.
- Removida a varredura I²C2 da inicialização normal. A função de diagnóstico `I2C2_Scan_All()` foi preservada.
- Removida a leitura de todos os 64 KiB da EEPROM 1 durante a inicialização. A função de diagnóstico foi preservada, mas não é chamada no fluxo normal.
- Removida a execução automática do teste CRC-16 durante a inicialização. A função de teste foi preservada para uso manual.
- Mantida a leitura dos metadados no início; agora o retorno é verificado e um erro é indicado no estado do display. Não se tenta reinicializar automaticamente metadados inválidos.
- Os testes de EEPROM continuam disponíveis no código, mas não são executados automaticamente. O teste `EEPROM_Test_SerializedRecordRoundTrip()` não deve ser executado com dados reais, pois escreve na EEPROM 2.

## Cuidados

- Esta é uma refatoração inicial baseada no projeto recebido; ainda precisa ser compilada no STM32CubeIDE e testada na placa.
- Não foi possível confirmar a compilação neste ambiente porque o toolchain `arm-none-eabi-gcc` não está disponível.
- Antes de usar com dados importantes, faça backup dos dados das EEPROMs e confirme que a gravação, a transição EEPROM 1 -> EEPROM 2 e a recuperação após reinicialização continuam corretas.
- Timers, FreeRTOS e Watchdog ainda não foram implementados nesta etapa.


## Etapa inicial — aquisição e painel de diagnóstico (2026-10-26)

Alterações preparadas nesta cópia do projeto:

- Intervalo nominal de aquisição centralizado em `SENSOR_ACQUISITION_INTERVAL_MS` (10.000 ms). A primeira aquisição ocorre logo após a inicialização; as seguintes usam deadline por `HAL_GetTick()`. Se uma operação demorar além do período, o firmware evita executar várias aquisições acumuladas em sequência.
- O resultado de cada leitura de sensor passa a atualizar o estado de disponibilidade usado pelo `sensor_mask` já existente no registro. O formato binário dos registros não foi alterado.
- Sensores com falha continuam sendo tentados nas próximas aquisições; o MPU6500 tenta reinicialização/calibração quando está marcado como indisponível.
- OLED alterado para duas páginas de diagnóstico com alternância automática: estado geral/sensores/RTC e armazenamento/operação USB.
- A limpeza e exportação atualizam o estado de operação USB mostrado no OLED. A exportação continua síncrona no fluxo principal, portanto a aquisição fica naturalmente pausada enquanto ela ocorre; a coordenação por tarefas/filas pertence à etapa FreeRTOS posterior.
- Se nenhum sensor produzir uma leitura válida, não é criado registro vazio nem o evento é classificado como erro de EEPROM.

Limitações e validação pendente:

- Esta etapa ainda não integra FreeRTOS nem watchdog.
- O painel mostra a EEPROM ativa e o próximo endereço usado pelos metadados; não calcula ainda a contagem total de registros nem uma porcentagem global exata de ocupação das duas EEPROMs. Isso exige varredura/contagem consistente com o formato e CRC atuais.
- A detecção de falha do OLED cobre a inicialização; o driver atual não expõe claramente o resultado de cada atualização de tela.
- Compilação no STM32CubeIDE e testes físicos de desconexão/reconexão dos sensores, exportação, limpeza, RTC e gravação ainda precisam ser executados. Não considerar esta cópia validada em hardware até esses testes.
