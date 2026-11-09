Etapa 3 — OLED em tarefa independente

Alterações desta etapa:
- Criada StartOledTask, de baixa prioridade, com alternância nominal a cada 3 s.
- A tarefa principal não atualiza mais o display diretamente após iniciar o escalonador; solicita atualização por flag.
- O driver recebe um DisplaySnapshot com cópias dos dados de estado. Os mutexes de sensores e EEPROM são liberados antes das transações I2C2 do OLED.
- I2C2 continua exclusivo para OLED/EEPROM; I2C1 continua dedicado aos sensores.
- Mantida a aquisição em StartSensorTask e o período nominal de 10 s.

Validação pendente: esta entrega foi modificada estaticamente, mas não compilada nem testada no hardware nesta sessão. Faça Clean/Build no STM32CubeIDE antes de gravar. Não execute CLEAR para validar esta etapa.

Observação técnica: os estados de sensores e EEPROM são copiados sob mutexes separados, portanto o snapshot evita leitura durante atualização individual, mas não representa uma transação atômica conjunta entre os dois grupos.
