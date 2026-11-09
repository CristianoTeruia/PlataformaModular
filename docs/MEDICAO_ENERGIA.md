# Medição de consumo

Resultados medidos em 04/11/2026 com o firmware V12d (linha de base) e V13/V13.1 (otimizações). Todas as correntes são
do **lado da bateria**, com a tensão da bateria em **4,07 V** (valor confirmado pelo autor para todas as medições).

## Método
Os itens marcados com **(confirmar)** seguem o roteiro de medição e **não foram confirmados pelo autor**; ajuste o texto conforme o que foi feito de fato.

Multímetro em série no fio positivo da bateria (antes do conversor MT3608), em modo corrente contínua **(confirmar)**. O valor "típico" é o
que o multímetro mostrou de forma estável; o "máx." é o maior valor visto no mostrador (média lenta do multímetro, **não** o pico
instantâneo). Placa sem depurador **(confirmar)**. A medição do lado da bateria já inclui as perdas do conversor boost. Sensores: AHT20+BMP280 e
MPU6500, sem BH1750 e sem ENS160 **(confirmar; é a configuração do roteiro e da linha de base)**. Incerteza típica de um multímetro nessa faixa: ±1 a 2 mA por leitura (verifique a do seu modelo);
a mesma condição medida duas vezes (V13 padrão com OLED aceso e V13 com `WFI` e OLED sempre aceso) deu 17 mA nas duas, com máximos de 20 e 19 mA.

## Resultados

| Configuração | Típico | Máx. |
|---|---|---|
| V12d: AHT+BMP+MPU+OLED (linha de base) | 27 mA | 28 mA |
| V12d: AHT+BMP+MPU (sem OLED) | 20 mA | 21 mA |
| V12d: AHT+BMP+OLED (sem MPU) | 22 mA | 23 mA |
| V13 padrão, OLED aceso (primeiros 30 s) | 17 mA | 20 mA |
| **V13 padrão, OLED apagado** | **10 mA** | 11 mA |
| V13 sem `WFI`, OLED aceso | 22 mA | |
| V13 sem `WFI`, OLED apagado | 15 mA | 16 mA |
| V13 com `WFI`, OLED sempre aceso | 17 mA | 19 mA |

Redução da V12d (27 mA) para a V13: **−10 mA com o OLED aceso (−37 %) e −17 mA com o OLED apagado (−63 %)**.

## Contribuição de cada medida (por diferenças entre medições)

| Medida | Comparação | Efeito |
|---|---|---|
| `WFI` na tarefa ociosa | 22 → 17 mA (OLED aceso) e 15 → 10 mA (OLED apagado) | ≈ −5 mA nas duas |
| OLED apagado | 17 → 10 mA (com `WFI`) e 22 → 15 mA (sem `WFI`) | ≈ −7 mA nas duas |
| MPU6500 dormindo | 27 mA (V12d) → 22 mA (V13 sem `WFI`, OLED aceso) | ≈ −5 mA (estimativa anterior: ≈ −4,5 mA) |

Os efeitos se confirmam por comparações independentes. O do MPU compara versões diferentes do firmware (V12d e V13), portanto inclui também pequenas mudanças de código.

## Eficiência da cadeia de alimentação
Caminho: bateria → TP4056 → **MT3608 (5 V)** → pino 5V da Black Pill → **regulador linear da placa (3,3 V)** → circuito.
Medido na saída do MT3608: **5,1 V**, com 11 mA (máx. 13) e OLED aceso, e 6 mA (máx. 7) com o OLED apagado.
Suposição: a corrente do regulador de 3,3 V é igual à de 5 V (desconsidera a corrente de repouso do regulador).

| Grandeza | OLED aceso | OLED apagado |
|---|---|---|
| Potência na bateria (4,07 V × corrente) | 69 mW | 41 mW |
| Potência na saída do MT3608 (5,1 V × corrente) | 56 mW | 31 mW |
| **Eficiência do boost** | **81 %** | **75 %** |
| Perda no boost | 13 mW (19 % da bateria) | 10 mW (25 %) |
| Perda no regulador de 3,3 V ((5,1 − 3,3) V × corrente) | 20 mW (29 %) | 11 mW (27 %) |
| Energia útil a 3,3 V | 36 mW | 20 mW |
| **Eficiência da cadeia** | **52 %** | **49 %** |

Conclusão medida: **cerca de metade da energia da bateria se perde nas duas conversões** (boost de 75 a 81 % e regulador linear de
3,3/5,1 ≈ 65 %). O boost fica menos eficiente com a carga menor, o que é comum em conversores sem modo de baixa carga (hipótese, não verificada).

Hipótese ilustrativa (não medida): gerar os 3,3 V direto da bateria com um conversor de 90 % exigiria ≈ 5,4 mA em vez de 10 mA
com o OLED apagado, um ganho de autonomia de ≈ ×1,85. A eficiência de 90 % é otimista para uma carga tão leve.

## Autonomia estimada (modelo de potência constante)
O boost mantém 5 V, então a corrente na bateria sobe quando a tensão cai; por isso a conta usa potência. Suposições: 2500 mAh nominais ×
3,7 V × 80 % utilizáveis ≈ 7,4 Wh (**confirme a capacidade real da sua célula**) e eficiência constante.

| Cenário | Corrente / potência | Autonomia estimada |
|---|---|---|
| V12d (OLED aceso, MPU ligado) | 27 mA ≈ 110 mW | ≈ 67 h (≈ 2,8 dias) |
| V13, OLED sempre aceso | 17 mA ≈ 69 mW | ≈ 107 h (≈ 4,5 dias) |
| V13, OLED apagado (uso normal) | 10 mA ≈ 41 mW | ≈ 182 h (≈ 7,6 dias) |

Estas autonomias são **estimativas**, não medidas em descarga completa. Um ensaio de descarga completa não é recomendado sem confirmar que
o módulo de carga tem proteção contra descarga profunda.

## Limitações das medições
- Valores médios de um multímetro: os picos de cada ciclo de aquisição (≈ décimos de segundo a cada 10 s) não foram capturados; exigiriam um
  resistor shunt e um osciloscópio.
- O consumo durante a exportação não foi medido (a placa está na USB, com a bateria desconectada).
- A tensão da bateria foi a mesma (4,07 V) em todas as medições; com a bateria mais descarregada a corrente do lado da bateria sobe.
- A corrente de repouso do regulador de 3,3 V e das EEPROMs/sensores não foi separada do "resto".

## Trabalho futuro
- Gerar os 3,3 V direto da bateria numa revisão da PCB (maior ganho de autonomia entre as opções analisadas).
- Desligar o PLL e o cristal HSE quando não há USB (hoje ficam sempre ligados porque o USB depende deles).
- Modo STOP do MCU entre as amostras (exige o RTC como despertador e tratar o USB).
- Diodo ideal na entrada de alimentação para permitir USB e bateria ao mesmo tempo.
