# Resultados dos testes (V13.1)

**Firmware:** V13 + correção do agendamento da tarefa de sensores (ver "Achados"), chamado aqui de **V13.1**.
**Data dos ensaios:** 04/11/2026. **Origem dos dados:** **[relato]** = informado pelo autor; **[arquivo]** = calculado dos CSVs e
relatórios em `docs/evidencias/`; **[derivado]** = calculado a partir do código ou de outros dados; **[código]** = lido no firmware.
Roteiro dos ensaios: [PROTOCOLO_DE_TESTES.md](PROTOCOLO_DE_TESTES.md).

## Resumo

| Teste | Situação | Resultado principal |
|---|---|---|
| 1. Aquisição contínua | Aprovado (após correção) | 181 registros em 29 min 59 s, intervalo médio 9,994 s, sem reinício, lacuna ou duplicata |
| 2. Integridade | Aprovado; detecção de CRC não demonstrada | 0 registros suspeitos; contagem exportada = informada pelo STM32 em todas as exportações |
| 3. Exportação e preservação | Aprovado | `IDENTICO`: 215 registros reproduzidos + 125 novos; 340 = 340 |
| 4. RTC e CR2032 | Aprovado | CR2032 mantém a hora; sem alimentação total, registros em ano 2000; o ajuste pelo script restabelece |
| 5. Watchdog | Aprovado | 4 reinícios pelo IWDG, ≈ 57 s cada; exportação e registros preservados |
| 6. Combinações de sensores | Aprovado | máscaras 7, 11 e 3 conforme o esperado |
| 7. Estabilidade prolongada | Executado (3 sensores) | 8 h 55 min (3.212 registros) e 12 h 07 min até encher a memória (4.360 registros), sem reinício por watchdog |
| Energia | Medido | 27 mA → 10 mA (OLED apagado), ver [MEDICAO_ENERGIA.md](MEDICAO_ENERGIA.md) |

## Teste 1: aquisição contínua
Duas execuções, a segunda após a correção do agendamento. **[arquivo]** e **[relato]** (saídas do `diag.py` e do `verificar_csv.py`):

| | 1a execução (antes da correção) | 2a execução (V13.1) |
|---|---|---|
| Duração (1o a último registro) | 32 min 59 s (1979 s) | 29 min 59 s (1799 s) |
| Registros exportados = informados pelo STM32 | 193 | 181 |
| Registros suspeitos / linhas malformadas | 0 / 0 | 0 / 0 |
| Máscara | 7 em todos | 7 em todos |
| Duplicatas, retrocessos de horário, lacunas, ano 2000 | 0 | 0 |
| Intervalos dentro de 10 ± 2 s | 192 de 192 | 180 de 180 |
| **Intervalo médio** | **10,307 s (+3,07 %)** | **9,994 s (−0,06 %)** |
| `tick` no `diag.py` (sem reinício) | 1969 s | 1858 s |
| `failc` / `recov` / `wdgrst` / `wdgstale` | 0 / 0 / 0 / vazio | 0 / 0 / 0 / vazio |
| OLED: acendeu / apagou (`oledonn` / `oledoffn`) | 2 / 3 | 4 / 5 |
| Menor folga de pilha (`stkdef`) | 2232 B de 4096 B | 2232 B de 4096 B |

Contagens do OLED: o display começa aceso no boot e termina apagado, então `oledoffn = oledonn + 1` é o esperado **[código]**, e foi o que se observou nas duas
execuções (3 = 2 + 1 e 5 = 4 + 1). Cada acendimento posterior corresponde a um toque no botão (ou ao início de uma operação USB, como a exportação); o número de toques
realizados **não foi registrado**, e deve ser informado pelo autor se for citado.
O `diag.py` foi tirado **antes** da exportação em ambas as execuções; o pior caso de pilha depois de uma exportação não foi registrado na V13.1
(nas versões V10 a V12d foi de 2288 B livres após exportação).
Valores dos sensores (2a execução): temperatura 31,70–31,93 °C, umidade 74,3–75,5 %, pressão 94.078–94.144 Pa, `accel_z` média 0,994 g,
giroscópio com média ≈ 0. Esses valores são coerentes e estáveis, mas **não foram comparados com um instrumento de referência**.

## Teste 2: integridade dos registros

| Pergunta | Resposta |
|---|---|
| Horários em sequência? | Sim. Retrocessos: 0; intervalos dentro do esperado em 180 de 180 (Teste 1) e 339 de 339 (Teste 3) |
| Registros duplicados? | Não. Horários repetidos e linhas idênticas: 0 |
| Valores zerados ou inválidos? | Nenhum registro com todas as colunas de um sensor em zero; nenhum valor fora das faixas configuradas em `tools/verificar_csv.py`. Zeros isolados em `accel` e `gyro` são esperados em repouso (resolução de 2 a 3 casas) |
| O firmware identifica CRC incorreto? | **[código]** sim: registro com CRC ou máscara inválidos é contado como suspeito e ignorado. **[arquivo]** 0 suspeitos em todas as exportações. **A capacidade de detecção não foi demonstrada experimentalmente**: nunca houve registro corrompido para detectar |
| Quantidade exportada = informada pelo STM32? | Sim: 193 = 193, 181 = 181 e 340 = 340 |

O relatório verifica coerência, **não exatidão física**. Duração medida da exportação de ≈ 200 registros: ≈ 2 s **[relato]**.

## Teste 3: exportação e preservação dos dados
Exportação A (`dados_sensores_20261104_191008.csv`, 215 registros) e B (`..._193054.csv`, 340 registros), sem limpar a memória, com a placa **somente na USB** **[relato]**:
- `comparar_csv.py`: `IDENTICO: 215 registros do arquivo antigo reproduzidos exatamente (125 registros novos no fim)` **[relato]**.
- B: 340 = 340 informados; 339 de 339 intervalos dentro do esperado; intervalo médio 9,997 s; sem lacunas, duplicatas ou retrocessos **[relato]**.
- **Limite:** como a placa não foi reiniciada nem teve a alimentação trocada, este teste **não exercita** o retorno após reinício. Esse ponto está coberto pelos
  Testes 4 e 5, que incluíram interrupções de energia e reinícios pelo watchdog **[relato]**: nos CSVs seguintes a série continua a mesma (mesmo primeiro registro,
  quantidade crescente, com lacunas nos momentos das interrupções) **[arquivo]**.
- Nenhuma lacuna apareceu em torno das exportações, que duraram ≈ 2 s. A perda de amostra durante a exportação é possível pelo código (a tarefa de sensores
  espera no máximo 250 ms pelo mutex da EEPROM e descarta a amostra), mas não foi observada nestas exportações curtas.

## Teste 4: RTC e bateria de backup
**[relato]:** com a alimentação principal removida e só a CR2032 ligada, o RTC manteve a hora, e nos registros aparece um salto de alguns minutos (o tempo desligado).
Sem nenhuma alimentação, os registros passaram a sair com o ano 2000, como previsto; após uma exportação (que ajusta o RTC), os registros voltam a ter a data correta
e a gravação continua. A ocorrência foi identificada pelo `diag.py` e pelo `verificar_csv.py`. Tensão da CR2032: **3,25 V**.
- **[arquivo]** Os 4 registros de ano 2000 permanecem nas exportações seguintes: o firmware **não corrige** os horários antigos, e os registros gravados com hora
  inválida **não são marcados** no formato (confirma o que se lê no código).
- **[código]** A indicação de hora inválida no equipamento é a tela do OLED (`RTC:INVALIDO`); o `DIAG` não tem campo de validade do RTC, e a causa do último reset (POR/PDR) aparece no `diag.py`.
- **A preencher pelo autor:** quanto tempo ficou só com a CR2032 e a lacuna medida no CSV; fotos do OLED com `RTC:INVALIDO`.
- A deriva do RTC **não foi medida** (o teste mede a continuidade da hora, não a precisão do cristal).

## Teste 5: watchdog
**[relato]:** teste do watchdog ligado (`ENABLE_WDT_TEST`) executado 4 vezes. O tempo do comando até o reinício foi de **≈ 57 s** em todas. Mesmo após os 4 resets,
a exportação funcionou e os registros estavam intactos. `ENABLE_WDT_TEST` está em 0 na versão final **[código: `Core/Inc/app_config.h`]**.
- **[derivado]** O código prevê: tempo = (instante do ciclo, 0 a 10 s) + 30 s de prazo da tarefa + tempo do IWDG. Com ≈ 57 s, o tempo do IWDG fica entre ≈ 17 e 27 s,
  contra 16,4 s com a LSI nominal de 32 kHz. Isso sugere uma LSI abaixo da nominal (≈ 19 a 31 kHz), dentro da faixa do datasheet (17 a 47 kHz, a confirmar). É uma
  inferência a partir de um tempo aproximado; os quatro tempos individuais não foram registrados.
- O resultado confirma o objetivo: a ausência de atividade é detectada, a placa reinicia, volta a operar normalmente e os dados são preservados.
- **Limite:** só a tarefa de sensores é travada pelo teste. O travamento da `defaultTask` ou da `oledTask` não tem gatilho no firmware.

## Teste 6: combinações de sensores

| Configuração | Máscara esperada | Observada |
|---|---|---|
| AHT20+BMP280 e MPU6500 | 7 (1+2+4) | 7 |
| AHT20+BMP280 e BH1750 | 11 (1+2+8) | 11 |
| AHT20+BMP280 sozinho | 3 (1+2) | 3 |

*(Um valor "2" para a terceira configuração, informado inicialmente, foi um erro de anotação do autor, corrigido para 3.)*
O arquivo `dados_sensores_20261104_210648.csv` contém as três configurações na mesma memória **[arquivo]**: 61 registros de máscara 7 (30 B), 91 de máscara 11 (20 B) e
78 de máscara 3 (18 B), com os campos dos sensores ausentes vazios e 2 lacunas (as trocas de módulos). Isso demonstra **registros de tamanho variável** convivendo na mesma
memória. Tamanhos de registro **[derivado do código]**. `bh_lux` nos registros de máscara 11: 25.0 a 75.0 lx em 91 registros **[arquivo]**.
**Não ensaiável:** a combinação de quatro sensores (máscara 15, registro de 32 B) exigiria três módulos (AHT20+BMP280, MPU6500 e BH1750), e a PCB comporta dois. A capacidade para ela (4.088 registros, ≈ 11,4 h) é teórica.

## Teste 7: estabilidade prolongada
Números **[relato]**, informados no rascunho do TCC; os CSVs correspondentes ainda **não estão** em `docs/evidencias`. Configuração AHT20+BMP280+MPU6500 (máscara 7).

| Conjunto | Registros | Duração | Intervalo médio | Lacunas > 12 s | Suspeitos |
|---|---|---|---|---|---|
| D (exportação intermediária) | 3.212 | 8 h 55 min 28 s | 10,006 s (9,999 s sem a lacuna) | 1 (30 s) | 0 |
| E (exportação final, memória cheia) | 4.360 | 12 h 07 min 46 s | 10,017 s (10,000 s sem as lacunas) | 3 (30 s, 30 s, 48 s) | 0 |

- D e E parecem ser **a mesma execução** vista em dois momentos: as lacunas de 30 s de E são as das exportações feitas por volta de 15:58 e 15:59, que também delimitam D **[derivado do relato; a confirmar]**.
- Duas exportações seguidas de D: os 3.211 registros da primeira foram reproduzidos exatamente na segunda, que tinha 1 registro novo **[relato]**.
- E: 4.360 registros = capacidade prevista para registros de 30 B (2.176 na EEPROM 1 + 2.184 na EEPROM 2) **[derivado do código]**; a gravação parou sem sobrescrever e o OLED avisou **[relato]**.
- Sem reinício por watchdog, retrocesso de horário, duplicata ou máscara incoerente **[relato]**.
- **A preencher pelo autor:** a fonte de alimentação durante o ensaio (USB ou bateria) e as saídas do `diag.py` ao longo do tempo.

## Achados e correções durante os ensaios
1. **Defeito de agendamento da `sensorTask`.** O intervalo médio entre registros foi de 10,307 s (+3,07 %), não 10 s. A tarefa calculava o próximo despertar a partir do **fim** do ciclo
   (a condição `current_tick - next_wake_tick >= 0` era sempre verdadeira), então o período real era 10 s + a duração do ciclo (≈ 0,3 s: esperas do AHT20 de 80 ms, do BMP280 de 100 ms e
   o despertar do MPU6500 de 100 ms). A correção passou a avançar o alvo de despertar em múltiplos do período. Depois dela: 9,994 s e 9,997 s. **Verificado em hardware**, com o mesmo método.
   O despertar do MPU6500 (economia de energia da V13) é o que alongou o ciclo em ≈ 0,1 s.
2. **Hipótese descartada.** A explicação anterior para o intervalo do CSV antigo (10,055 s), o oscilador interno de 16 MHz, não foi necessária nem comprovada.
3. **Erro de anotação.** A máscara do Teste 6 (configuração só AHT20+BMP280) era 3, não 2.

## Limitações e itens pendentes
- A combinação de quatro sensores (máscara 15) não é ensaiável com a PCB atual (dois módulos). Os CSVs do Teste 7 ainda não estão em `docs/evidencias`.
- Detecção de CRC incorreto não demonstrada experimentalmente; deriva do RTC não medida; LSI real do chip desconhecida (só inferida no Teste 5).
- Valores físicos dos sensores não comparados com instrumentos de referência.
- Registros gravados com hora inválida não são marcados nem corrigidos depois.
- ENS160 não validado em hardware (ver README).
- Autonomia estimada, não medida em descarga; picos de corrente durante a aquisição não medidos.
- Dados a preencher pelo autor: tempos individuais do watchdog, tempo só com a CR2032 e a lacuna correspondente, fotos do OLED, tempo de exportação completa.
