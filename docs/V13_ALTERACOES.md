# V13 - economia de energia, limpeza e testes automáticos

NÃO foi compilada para ARM nem testada na placa (só checagem de sintaxe e testes no PC). Faça Clean + Build.
Base: V12d.

## Firmware
- **OLED apaga sozinho** após `OLED_AUTO_OFF_MS` (30 s) sem atividade e acende pelo botão (PA0, ativo em nível baixo,
  filtrado por 2 leituras de 20 ms) ou quando começa uma operação USB (exportação/limpeza/ajuste do RTC). Com o display
  apagado a tarefa do OLED não renderiza nem usa o I2C2. Comandos: apagar = `0xAE` + bomba de carga desabilitada
  (`0x8D 0x10`); acender = `0x8D 0x14` + `0xAF`. A política está em `oled_power.c` (lógica pura, testada no PC).
  Se o botão não acender o display, confira a polaridade do botão na sua PCB (o firmware espera nível baixo ao pressionar).
  Ao reiniciar a placa o display fica aceso por 30 s.
- **MPU6500 dorme entre as amostras**: acorda, espera 100 ms, lê e volta a dormir (`MPU6500_Wake/Sleep`).
- **`WFI` na tarefa ociosa** (`ENABLE_IDLE_WFI`). Ponha 0 em `app_config.h` ao depurar com breakpoints.
- **Comando `CLEAR` exige confirmação**: `CLEAR,CONFIRMO`. O `CLEAR` sozinho responde `ERRO,CLEAR_REQUER_CONFIRMACAO`.
  O `exportar_eeprom.py` já envia a forma nova.
- **`WDTTEST` desativado por padrão** (`ENABLE_WDT_TEST` em `app_config.h`).
- **`DIAG`** novos campos: `oledon`, `oledonn`, `oledoffn`.
- O ENS160 continua na tabela de sensores (só aparece se um módulo estiver conectado); ver "Limitações" no README.

## Repositório
- Documentos de versões anteriores em `docs/history/`; scripts em `tools/`
  (comandos agora: `python tools/exportar_eeprom.py --port COM3`, `python tools/diag.py --port COM3`).
- `README.md`, `docs/MEDICAO_ENERGIA.md`, `.gitignore`, `tests/` (+ `.github/workflows/host-tests.yml`).

## Testes no PC (`make -C tests`)
`test_registry`: 840.378 verificações, 0 falhas. `test_oled_power`: 226 verificações, 0 falhas (inclui 200 sequências
aleatórias comparadas com um modelo de referência). Cinco erros introduzidos de propósito (OLED apaga um instante
tarde, botão mantido repete o acendimento, falha ao apagar não desfaz o estado, MPU não volta a dormir, MPU lê sem
acordar) foram todos detectados.

## V13.1: correção do agendamento
A `sensorTask` calculava o próximo despertar a partir do fim do ciclo, e o período real era 10 s + duração do ciclo (≈ 10,3 s). A correção avança o alvo de despertar
em múltiplos do período e só recalcula se o ciclo estourar o período inteiro:

```c
next_wake_tick += period_ticks;
if ((int32_t)(osKernelGetTickCount() - next_wake_tick) >= 0)   // ciclo estourou o período
    next_wake_tick = osKernelGetTickCount() + period_ticks;
```

Verificado em hardware: intervalo médio de 9,994 s e 9,997 s (antes, 10,307 s).

## Validação em hardware
Executada em 04/11/2026. Resultados completos em [RESULTADOS_DOS_TESTES.md](RESULTADOS_DOS_TESTES.md) e [MEDICAO_ENERGIA.md](MEDICAO_ENERGIA.md):
- OLED apaga após 30 s e acende pelo botão; o `DIAG` conta os eventos (`oledonn`, `oledoffn`).
- Consumo: 27 mA (V12d) → 17 mA com o OLED aceso e **10 mA com o OLED apagado**.
- Registros continuam sendo gravados com o display apagado; exportações íntegras (contagem exportada = informada pelo STM32).
- Ensaio de estabilidade prolongada executado (12 h 07 min até encher a memória); ver `RESULTADOS_DOS_TESTES.md`.
