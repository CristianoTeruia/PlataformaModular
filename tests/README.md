# Testes no PC

Rodam o código real do firmware (tabela de sensores, drivers, política do OLED) no computador, sem a placa.

```text
make -C tests
```

Requisitos: `gcc` e `make`. Os cabeçalhos do HAL/CMSIS já estão no repositório (`Drivers/`).

- `test_registry.c`: formato do registro e CSV, drivers do BH1750 e do ENS160 contra dispositivos I2C simulados
  (um ENS160 estrito que imita o comportamento do módulo real), ordem de sono/despertar do MPU6500.
- `test_oled_power.c`: política de energia do OLED (tempo, botão, eventos, virada do contador, falhas, sequências
  aleatórias contra um modelo).

Os testes validam a **lógica**. Eles não substituem os ensaios em hardware (consumo, tempos reais, sensores físicos).

**O que os testes no PC não cobrem:** o agendamento da `sensorTask` e outras partes de `app_tasks.c` que dependem do FreeRTOS e do hardware (por exemplo, o defeito de
agendamento corrigido na V13.1 foi encontrado e verificado em hardware, não nestes testes), a exportação, o watchdog e o consumo.
