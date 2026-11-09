# Diagnóstico V4 — exportação USB / mutex EEPROM

## Objetivo
Esta versão mantém a V3 como referência e aplica uma mudança pequena para evitar que a tarefa dos sensores espere indefinidamente pelo mutex geral da EEPROM enquanto mantém `sensorDataMutexHandle`.

## Alterações
1. Em `Core/Src/main.c`, a aquisição de `eepromMutexHandle` na `StartSensorTask()` agora tem timeout de aproximadamente 250 ms. Se não conseguir o mutex, a amostra atual não é gravada, `eeprom_status_display` é sinalizado e a tarefa segue para alternar o LED e liberar `sensorDataMutexHandle`.
2. Em `USB_DEVICE/App/usbd_cdc_if.c`, `CDC_Transmit_FS()` agora retorna `USBD_FAIL` se `hUsbDeviceFS.pClassData` for nulo, evitando desreferência de ponteiro nulo nessa condição.

## Limites conhecidos
- Esta alteração não confirma a causa original do travamento.
- A exportação ainda mantém `eepromMutexHandle` durante toda a leitura e transmissão CSV.
- `EEPROM_USB_SendText()` continua usando timeout de até 5 s por tentativa de envio; múltiplas linhas podem fazer a exportação durar mais.
- A mudança pode descartar uma amostra quando a exportação mantiver a EEPROM ocupada por tempo superior ao timeout. Não apaga nem reescreve dados já armazenados.
- Não foi possível compilar no STM32CubeIDE nem testar em hardware nesta execução. É necessário compilar e validar antes de gravar a placa.

## Teste recomendado
1. Importar o projeto em uma cópia separada no STM32CubeIDE e compilar.
2. Gravar a V4 somente se a compilação terminar sem erros.
3. Confirmar primeiro que LED e OLED continuam operando por pelo menos 1 minuto.
4. Antes de tentar `EXPORT`, abrir a captura serial e verificar se recebe o comando de aceitação.
5. Fazer no máximo uma tentativa de exportação. Registrar a última linha recebida e se LED/OLED continuam ativos.
6. Se travar novamente, reiniciar a placa e guardar o log; não executar `CLEAR` nem testes de escrita/apagamento.

## Integridade
O formato dos registros da EEPROM e a lógica de exportação CSV não foram alterados.
