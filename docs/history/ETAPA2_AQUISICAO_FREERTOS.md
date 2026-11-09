# Etapa 2 — Separação da aquisição de sensores com CMSIS-RTOS2

## Objetivo

Separar a aquisição periódica dos sensores da tarefa que processa comandos USB e atualiza o OLED, mantendo o período de aquisição configurado em `SENSOR_ACQUISITION_INTERVAL_MS` (10.000 ms).

## Alterações nesta cópia

- Criada a tarefa `StartSensorTask`, com prioridade abaixo da tarefa principal.
- A tarefa de aquisição usa `osDelayUntil()` e o tick reportado por `osKernelGetTickFreq()` para programar o próximo ciclo.
- O primeiro ciclo da tarefa é imediato, como ocorria no fluxo anterior.
- A leitura de AHT20, BMP280 e MPU6500, atualização dos estados e gravação do registro foram movidas para a tarefa de aquisição.
- A recuperação do barramento continua limitada ao I²C1 e mantém a tentativa limitada a uma vez a cada 30 s após ciclos sem sensores válidos.
- A tarefa principal continua processando os comandos RTC_SET, exportação e CLEAR, além de alternar as páginas do OLED a cada 3 s.
- Criado um mutex para serializar gravação, exportação e limpeza das EEPROMs.
- Criado um mutex para evitar que `UpdateDisplay()` leia os valores dos sensores enquanto a tarefa de aquisição os atualiza.
- Nenhuma operação de limpeza das EEPROMs foi executada por esta alteração.

## Limites conhecidos

- Esta etapa ainda não separa USB, OLED e armazenamento em tarefas próprias.
- A exportação USB continua sendo uma operação síncrona na tarefa principal; o mutex evita que uma gravação concorra com exportação/limpeza, mas não transforma a exportação em uma operação assíncrona.
- Os valores de RTC são lidos pela tarefa de aquisição e podem ser ajustados pela tarefa USB; esse caso deve ser validado durante os testes de sincronização de horário.
- O código não foi compilado neste ambiente. A tentativa de usar o `Debug/makefile` incluído no ZIP não funcionou porque o makefile gerado contém caminhos/formatação incompatíveis com o ambiente de execução. A compilação no STM32CubeIDE continua obrigatória antes de gravar na placa.

## Validação recomendada

1. Fazer uma cópia de segurança do ZIP e dos dados exportados das EEPROMs.
2. Abrir o projeto no STM32CubeIDE e executar **Project > Clean** e **Build Project**.
3. Se houver erro, guardar o log completo e não gravar o firmware.
4. Se compilar, gravar e verificar boot, LED, OLED, sensores e USB CDC.
5. Medir pelo menos três intervalos entre registros no CSV/exportação para confirmar o período nominal de 10 s.
6. Verificar que a página do OLED continua alternando a cada 3 s e que os valores atualizados aparecem.
7. Testar exportação antes de qualquer teste de CLEAR. Não testar CLEAR enquanto os dados existentes precisarem ser preservados.
8. Depois, testar a sincronização RTC_SET e a exportação sob aquisição ativa, conferindo que não há registros duplicados ou perda de comunicação.

## Estado

Alteração preparada para revisão e teste. Não considerar compilada nem validada em hardware até concluir a lista acima.
