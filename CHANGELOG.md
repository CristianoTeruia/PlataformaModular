# Histórico de versões

O firmware evoluiu por etapas; os documentos de cada etapa estão em `docs/history/`. Resumo (sem datas):

## V13.1
- Corrige o agendamento da `sensorTask`: o período passa a ser de 10 s (antes, 10 s + duração do ciclo ≈ 10,3 s). Verificado em hardware: intervalo médio de 9,994 s e 9,997 s.

## V13: economia de energia e organização
- OLED apaga após 30 s sem atividade e acende pelo botão (PA0) ou ao iniciar uma operação USB.
- MPU6500 dorme entre as amostras. Tarefa ociosa com `WFI`.
- `CLEAR` exige `CLEAR,CONFIRMO`; `WDTTEST` fica atrás da opção `ENABLE_WDT_TEST`.
- Consumo medido: 27 mA (V12d) → 17 mA com o OLED aceso e 10 mA com o OLED apagado.
- Repositório: `tests/` com CI, `tools/`, `docs/`, README.

## V12 a V12d: BH1750 e ENS160
- BH1750 e ENS160 entram na tabela de sensores (bits 3 e 4); registro de até 38 bytes.
- Inicialização do ENS160 segundo os drivers de referência; descarte de amostras zeradas. O módulo ENS160+AHT21 testado não mediu (não validado).

## V11: tabela de sensores
- Cada sensor vira uma entrada de `sensor_registry.c`. Saída idêntica à da V10 (4360 registros reais reproduzidos).

## V10: regeneração do CubeMX e watchdog
- Código de aplicação movido para arquivos próprios (`app_tasks.c`, `app_watchdog.c`). IWDG com supervisão por tarefa. Hora preservada com a CR2032.

## V9: causa raiz da corrupção do OLED
- Esperas ocupadas (`HAL_Delay`) dos sensores substituídas por esperas que cedem a CPU. Mutex dedicado do RTC.

## V8: diagnóstico
- Comando `DIAG` e recuperação automática do I2C2/OLED.

## V7: estabilidade da exportação
- Pilha e heap maiores, detecção de estouro de pilha, leitura da EEPROM em blocos, esperas que cedem a CPU.

## Etapas 1 a 3 (antes da V7)
- Integração do FreeRTOS, aquisição e diagnóstico do OLED/I2C2. Ver `docs/history/ETAPA*.md`.
