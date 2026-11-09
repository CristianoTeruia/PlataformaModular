# Correção de concorrência no barramento I2C2 — Etapa 3

## Motivo
O OLED SSD1306 e as duas EEPROMs 24LC512 usam `hi2c2`. Antes desta alteração, uma atualização do OLED podia ocorrer simultaneamente a uma transação de EEPROM. O mutex anterior protegia o estado/operações de EEPROM entre tarefas, mas não serializava o OLED no mesmo barramento físico.

## Alterações
- Adicionado `i2c2MutexHandle`, criado antes das tarefas do FreeRTOS.
- Adicionada a API `I2C2_BusAcquire()` / `I2C2_BusRelease()` em `Core/Inc/i2c2_bus_lock.h` e `Core/Src/main.c`.
- Cada transação física de leitura/gravação da EEPROM adquire e libera o mutex compartilhado.
- `SSD1306_UpdateScreen()` mantém o mutex durante a atualização completa das oito páginas, evitando que transações de EEPROM se intercalem entre comandos e dados do OLED.
- `SSD1306_UpdateScreen()` agora verifica o retorno das transferências I2C e registra falha em `ssd1306_status` quando uma transferência falha.
- A inicialização do OLED antes do escalonador continua sem mutex, pois ocorre sequencialmente antes da criação/execução das tarefas.

## Limites da validação
Esta é uma alteração de código, ainda não validada por compilação no STM32CubeIDE nem por teste na placa física nesta etapa. A causa da corrupção era plausível e havia uma ausência concreta de serialização; isso não prova que seja a única causa.

## Teste recomendado
1. Importar o projeto no STM32CubeIDE e compilar.
2. Executar normalmente e observar várias trocas de página do OLED.
3. Durante a execução, deixar ocorrer aquisição/gravação de sensores e atividade normal de EEPROM; observar se a tela permanece íntegra.
4. Repetir por alguns minutos, anotando qualquer falha de OLED/I2C e se a falha aparece durante gravação/leitura.
5. Não executar `EEPROM24LC512_EraseAll()` nem testes destrutivos para validar esta correção.


## Diagnostico adicional (sem apagar EEPROM)
- `ssd1306_status` agora reflete o resultado da ultima atualizacao completa da tela, inclusive voltando a `HAL_OK` apos uma atualizacao bem-sucedida.
- Se a exportacao falhar ao ler a EEPROM1, a resposta USB inclui `HAL`, `I2CERR`, `I2CSTATE` e `OLED` para registrar o estado observado no momento da falha.
- Isso e instrumentacao para coleta de evidencias, nao uma correcao confirmada da causa raiz.
