# V7 - correcao da exportacao USB + deteccao de falhas

NAO foi compilada para ARM nem testada na placa (so checagem de sintaxe no PC).
Faca Clean + Build no STM32CubeIDE antes de gravar. Nao execute CLEAR nos testes.

## Teste (nesta ordem)
1. Clean + Build + gravar em modo Debug (ou Run).
2. Deixe rodar 1 min: LED deve piscar a cada 10 s, OLED alternando as telas.
3. Rode `python exportar_eeprom.py --port COM3`.
4. Observe:
   - O script imprime as primeiras linhas, uma a cada 200, e as linhas PROGRESS.
   - Se der timeout, ele informa quantas linhas chegaram e qual foi a ultima.
5. Espere ~1 min depois da exportacao e veja se o LED volta a piscar.

## Codigos do LED (PC13) quando o sistema para por falha
- 2 piscadas + pausa: estouro de stack. O nome da task fica em `diag_overflow_task_name`.
- 3 piscadas + pausa: falha de alocacao no heap do FreeRTOS.
- 4 piscadas + pausa: HardFault. Registradores em `diag_fault_cfsr`, `diag_fault_hfsr`,
  `diag_fault_bfar`, `diag_fault_mmfar` (ler no debugger, janela Expressions).
- LED parado sem piscar: nenhuma dessas falhas foi detectada (deadlock/travamento por outro motivo).

## O que mudou
1. defaultTask: stack 2048 -> 4096 bytes; heap do FreeRTOS 15 KB -> 32 KB.
2. FreeRTOSConfig: configCHECK_FOR_STACK_OVERFLOW=2 e configUSE_MALLOC_FAILED_HOOK=1,
   com os hooks em freertos.c.
3. HardFault_Handler grava CFSR/HFSR/BFAR/MMFAR e pisca o LED (4x).
4. EEPROM: HAL_Delay() trocado por espera que cede a CPU (osDelay) quando o RTOS roda,
   na gravacao e no envio USB. Antes, a exportacao deixava sensor/OLED/LED sem rodar.
5. Exportacao: leitura da EEPROM em blocos de 128 B (antes: 1 leitura I2C por byte) e
   linha `PROGRESS,<endereco>` a cada 500 ms de silencio, para o PC nao dar timeout.
6. Buffers grandes (`header`, `diagnostic`) fora da stack.
7. Respostas USB (PONG, EXPORT,ACEITO, CLEAR_*, RTC_SET_*) agora em buffers `static`:
   o driver USB le o buffer depois que a funcao retorna.
8. Timeout de mutex da EEPROM durante EXPORT/CLEAR nao marca mais ERRO:EEPROM no OLED.
9. exportar_eeprom.py: ignora PROGRESS, mostra contagem de linhas e a ultima linha no timeout.
   (Use o script novo com o firmware novo.)

## Nao alterado
Formato dos registros, metadados, CRC, logica CSV, enderecos das EEPROMs.
