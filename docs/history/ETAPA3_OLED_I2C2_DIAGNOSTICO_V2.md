# Etapa 3 — diagnóstico aprofundado de OLED/I2C2 (V2)

Esta é uma versão experimental de diagnóstico, não uma correção confirmada. Foi feita a partir do ZIP de diagnóstico anterior e mantém as alterações anteriores.

## O que foi acrescentado

Antes de cada chamada bloqueante de transferência do OLED, o firmware agora registra:
- `PRESTATE`: estado da HAL I2C2 imediatamente antes da chamada;
- `PRELOCK`: estado do lock interno da HAL antes da chamada;
- `HW_BUSY`: estado da flag BUSY do periférico I2C2 antes da chamada (`1` ativa, `0` inativa);
- `SDA` e `SCL`: níveis lógicos lidos nos pinos PB3 (SDA) e PB10 (SCL), respectivamente (`1` alto, `0` baixo).

Esses campos são incluídos na mensagem `ERRO,FALHA_LEITURA_EEPROM1,...` quando a exportação falha. A intenção é diferenciar um periférico marcado como ocupado pela HAL de um barramento que já estava ocupado antes de iniciar a transferência. A leitura dos pinos é apenas observacional; o código não altera a configuração GPIO nem tenta recuperar o barramento.

## Como testar

1. Preserve uma cópia do projeto e extraia o ZIP em uma pasta nova.
2. Abra no STM32CubeIDE e compile. **Não grave o firmware se houver erros de compilação**; envie o erro completo.
3. Grave o firmware e deixe-o funcionando até o OLED apresentar corrupção ou parar de atualizar.
4. Só depois disso, tente `EXPORT` pelo procedimento habitual.
5. Copie a linha completa que começa com `ERRO,FALHA_LEITURA_EEPROM1` e informe se o OLED já estava corrompido antes do comando.

Não execute limpeza, formatação ou apagamento das EEPROMs durante o diagnóstico.

## Interpretação dos campos principais

- `HAL=2` ou `OLED=2`: chamada da HAL retornou `HAL_BUSY`.
- `I2CERR` / `OLEDERR`: código de erro da HAL; `32` (`0x20`) é `HAL_I2C_ERROR_TIMEOUT` nesta versão do driver.
- `I2CSTATE` / `OLEDSTATE`: estado capturado depois da falha na operação correspondente.
- `PRESTATE`: estado da HAL imediatamente antes da chamada OLED que falhou.
- `PRELOCK`: lock interno da HAL antes da chamada (`0` normalmente significa desbloqueado).
- `HW_BUSY=1`: flag BUSY do periférico estava ativa antes da chamada.
- `SDA=0` ou `SCL=0`: linha correspondente estava baixa no instante da leitura; isso, isoladamente, não identifica qual dispositivo ou condição a manteve baixa.
- `OLEDPAGE`: página 0–7 que falhou; `255` se nenhuma página foi identificada.
- `OLEDSTAGE=1`: comando de página; `2`: envio dos pixels; `3`: falha ao adquirir mutex.
- `OLEDLOCKFAIL`: número de falhas ao adquirir o mutex do OLED.
- `PAGINAS`: contador de trocas de página, não número de atualizações completas.

Os valores são evidências para o próximo diagnóstico; não provam isoladamente que o problema seja físico, de concorrência ou da HAL. Este projeto não foi compilado nem testado em hardware nesta sessão.
