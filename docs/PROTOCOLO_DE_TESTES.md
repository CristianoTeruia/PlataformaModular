# Protocolo de testes e medições (Etapas 2 a 5)

> **Situação:** os Testes 1 a 7 e a medição de energia já foram executados; os resultados estão em [RESULTADOS_DOS_TESTES.md](RESULTADOS_DOS_TESTES.md). Este documento mantém o roteiro original.

Documento de trabalho para os ensaios e para a redação do TCC. Cada teste traz: o que já se sabe, o que o código prevê,
o procedimento, o que registrar e um critério **sugerido** de aprovação (ajuste com o seu orientador).

**Legenda das origens:** **[código]** = lido no firmware · **[medido]** = resultado de um ensaio já feito ·
**[derivado]** = calculado a partir de [código]/[medido] · **[a testar]** = ainda sem evidência.
Nada aqui foi inventado: onde não há dado, está escrito "a testar" ou "desconhecido".

Firmware sob teste: V13 (anote o commit/data do build): ______ · `app_config.h`: `OLED_AUTO_OFF_MS`=____ `ENABLE_IDLE_WFI`=__ `ENABLE_WDT_TEST`=__

---
## 0. Preparação comum (vale para todos os testes)

**Ferramentas** (na pasta `tools/`): `exportar_eeprom.py`, `diag.py`, `comparar_csv.py`, `verificar_csv.py`.
Salve toda saída de terminal em arquivo, por exemplo: `python tools/diag.py --port COM3 > T1_diag_inicio.txt`.

**Cinco fatos que afetam todos os testes**
1. **O `diag.py` e o `exportar_eeprom.py` exigem a USB.** Se a placa estiver na bateria, ligar a USB exige desconectar a bateria,
   o que **reinicia** o sistema (a hora é mantida pela CR2032) e **zera os contadores do `DIAG`** [código]. Em ensaios em que
   você precisa consultar o `DIAG` sem interromper, alimente a placa pela USB do PC.
2. **Durante a exportação e a limpeza a aquisição perde amostras**: a tarefa de sensores espera no máximo 250 ms pelo mutex
   da EEPROM e descarta a amostra [código]. Lacunas nesses momentos são esperadas.
3. **O OLED apaga após 30 s sem atividade** (V13) [código]. Para ver a tela, aperte o botão. Se precisar dela sempre acesa,
   compile com `OLED_AUTO_OFF_MS 0U`.
4. **Estado da memória antes de cada teste:** a memória cheia para a gravação [código + medido]. Antes dos testes, exporte o que
   existir e **limpe** (o script pede para digitar `APAGAR`; a limpeza leva cerca de 20 s [código]). Anote a tela de
   armazenamento do OLED ("EEPROM ATIVA" e "PROX. END") antes e depois.
5. **O LED da placa alterna uma vez por ciclo de aquisição (~10 s)** [código]: serve de "batimento cardíaco" da tarefa de sensores.

**Como ler o OLED [código]:** tela 1 = `ESTADO DO SISTEMA` (GERAL, `SENSORES: n/m`, `RTC:VALIDO`/`RTC:INVALIDO`, linha de erro);
tela 2 = `ARMAZENAMENTO` (`EEPROM ATIVA: EEPROM1/EEPROM2/CHEIA/ERRO`, `PROX. END`, estado USB e códigos S/E/T/R).

**Nomes de arquivo sugeridos:** `T1_AAAAMMDD_export.csv`, `T1_..._verificacao.txt`, `T1_diag_inicio.txt`, `T1_diag_fim.txt`, fotos `T1_oled_*.jpg`.
**Integridade das evidências:** guarde o CSV original sem editar e o hash, no Windows: `certutil -hashfile arquivo.csv SHA256`.

### Evidências que já existem (de versões anteriores; cite a versão no TCC)

| Tema | Evidência | Versão | Limite |
|---|---|---|---|
| Exportação reproduzível | `comparar_csv`: IDENTICO, 4360 registros | V10→V11 | memória cheia: sem registros novos entre as exportações |
| OLED/I2C2 | 27 episódios de falha em 93 min → 0 em 108 min | V8 → V9 | não repetido na V13 |
| Watchdog | `WDTTEST`: reinício por IWDG, `wdgrst=1`, hora mantida | V10 | tempo até o reset e dados da EEPROM após o reset não verificados |
| Memória cheia | 4360 registros, para de gravar, OLED AVISO | V6–V12 | |
| Pilha e heap | `defaultTask`: mínimo livre ≈ 2288 B após exportação; heap 24 KB livres, sem variação | V10–V12d | |
| Consumo | 27 mA total (OLED ≈ 6,5, MPU ≈ 4,5, resto ≈ 16 mA) a ≈ 4,07 V | V12d | V13 não medido |
| Período de amostragem | 10,307 s (V13) → 9,994 e 9,997 s (V13.1, após corrigir o agendamento) | Testes 1 e 3 | ver [RESULTADOS_DOS_TESTES.md](RESULTADOS_DOS_TESTES.md) |
| V13.1 | compila; OLED apaga e acende pelo botão; testes 1 a 6 e consumo medidos | V13.1 | Teste 7 executado |

---
## ETAPA 2 — Testes funcionais

### Teste 1 — Aquisição contínua
**Objetivo:** mostrar que a aquisição, o armazenamento e a exportação funcionam em sequência por pelo menos 30 min.

**Esperado:** LED alternando a cada ~10 s [código]. Registros ≈ `T/10 + 1` na **V13.1** (com a correção do agendamento); na V13 original o intervalo real era
10 s + a duração do ciclo (≈ 10,3 s). Para T = 30 min: 181. Com 3 sensores cada registro ocupa 30 B (32 B com o BH1750)
[código], então "PROX. END" deve crescer nesses múltiplos [derivado].

**Procedimento**
1. Exporte e limpe a memória (Seção 0, item 4). Anote o "PROX. END" inicial.
2. Alimente a placa (anote: bateria ou USB) e `diag.py` > `T1_diag_inicio.txt`. Anote a hora real do início.
3. A cada ~10 min, aperte o botão e confira se a tela 1/2 mostra dados coerentes. Fotografe ao menos uma vez.
4. Ao completar ≥ 30 min: `diag.py` > `T1_diag_fim.txt`; aperte o botão e anote "PROX. END" e o estado.
5. `python tools/exportar_eeprom.py --port COM3` (**não** digite `APAGAR`). Anote o `EXPORT_END` (válidos/suspeitos), o tempo da exportação (cronômetro) e o nome do CSV.
6. `python tools/verificar_csv.py T1.csv --esperado <válidos informados pelo STM32>`.

**Registrar**

| Item | Valor |
|---|---|
| Duração (primeiro a último registro) | |
| Registros esperados (`T/10 + 1`) | |
| Registros exportados / informados pelo STM32 | |
| Reinicializações (`wdgrst`, `tick` do `diag`) | |
| `failc` / `recov` / `wdgstale` no fim | |
| OLED atualizando ao apertar o botão? | |
| Valores coerentes? (ver Teste 2) | |

**Critério sugerido:** sem reinício, `failc=0`, `recov=0`, contagem exportada = informada, lacunas só explicáveis (exportação), diferença
para o esperado explicada pelo período efetivo.

### Teste 2 — Integridade dos registros
Use o CSV do Teste 1 e `verificar_csv.py` (gera `*_verificacao.txt`, guarde como evidência).

| Pergunta do roteiro | Onde está a resposta |
|---|---|
| Horários em sequência? | "Horário retrocedeu", "Intervalos dentro do esperado", "Lacunas", "Intervalo médio" |
| Registros duplicados? | "Horários repetidos" e "Linhas totalmente idênticas" |
| Valores zerados ou inválidos? | "Registros com TODAS as colunas do ... em zero" e `fora_da_faixa` por coluna (faixas editáveis em `FAIXAS`) |
| O firmware identifica CRC incorreto? | **[código]** sim: registro com CRC ou máscara inválidos é contado em "suspeitos" (`EXPORT_END,<válidos>,<suspeitos>`) e ignorado. **[a testar]** não foi demonstrado em experimento (ver Seção 7) |
| Quantidade exportada = informada pelo STM32? | O script imprime as duas e **bloqueia a limpeza** se divergirem [código]; use `--esperado` |

**Esperado:** "suspeitos" = 0 numa memória sã [derivado]. Referência de coerência física em um CSV antigo [medido]: com a placa parada, `accel_z` entre 0,975 e 1,016 g
e giroscópio próximo de 0; temperatura 19,8–22,4 °C.
**Limite:** o relatório mostra coerência, **não** exatidão física. Se tiver um termômetro/higrômetro de referência, compare 1 ou 2 pontos e registre a diferença.

### Teste 3 — Exportação e preservação dos dados
**Esperado [código]:** a exportação **não apaga** nada; ao reiniciar, o firmware lê os metadados e **continua acrescentando** a partir do último endereço.

**Procedimento**
1. Com a aquisição rodando (pode ser a continuação do Teste 1), exporte → `A.csv` (**não** apague). Anote N1 (`EXPORT_END`), a duração da exportação e o "PROX. END".
2. Desconecte a USB, ligue a bateria, deixe rodando **≥ 10 min**.
3. Exporte de novo → `B.csv` (**não** apague). Anote N2, a duração e o "PROX. END".
4. `python tools/comparar_csv.py A.csv B.csv` e `python tools/verificar_csv.py B.csv --esperado N2`.

**Esperado:** `IDENTICO: N1 registros ... reproduzidos exatamente (N2−N1 novos)` [derivado]. No relatório de B: lacunas na janela da exportação 1 e na troca de alimentação. Pode haver
**retrocesso de horário** de alguns segundos logo após o primeiro ajuste do RTC (o `RTC_SET` reajusta o relógio ao do PC) [derivado].

| Item | Valor |
|---|---|
| N1 / N2 / novos (N2−N1) | |
| Resultado do `comparar_csv` | |
| Duração da exportação 1 e 2 (s) | |
| Lacunas em B (início, duração) | |
| Novos registros com horário posterior ao último de A? | |

**Critério sugerido:** `IDENTICO`, `novos = N2−N1`, sem duplicatas, lacunas explicadas.

---
## ETAPA 3 — Confiabilidade

### Teste 4 — RTC e bateria de backup
**Esperado [código]:** o firmware guarda um marcador nos registradores de backup. Se ele sumir, o boot ajusta o RTC para 01/01/2000 00:00:00, `rtc_time_valid=0`, o OLED mostra
`RTC:INVALIDO` e o GERAL vira AVISO. **Não há marca no registro**: registros gravados nesse estado saem com ano 2000 no CSV, e os antigos **não** são corrigidos depois do ajuste.
**Medido [medido]:** com a CR2032, a hora sobrevive à retirada da alimentação principal; a retirada total de energia zerou os contadores do backup (`wdgrst` voltou a 0).

**Parte A (CR2032 instalada)**
1. Rode `exportar_eeprom.py` e anote a linha `RTC_SET_END,OK,<data hora>` e a hora real do PC.
2. Desconecte USB e alimentação principal. Cronometre **≥ 10 min**.
3. Religue. Anote a hora real do religamento. Aperte o botão e fotografe a tela 1 (`RTC:`).
4. Espere ≥ 1 registro novo e exporte (o CSV é salvo **antes** do novo ajuste do RTC). Compare o horário do 1º registro novo com a hora real do religamento.

**Parte B (sem a CR2032; só com a alimentação principal desligada)**
1. Anote a tensão da CR2032 em circuito aberto (multímetro). Desligue tudo, retire a CR2032, espere alguns segundos.
2. Religue só a alimentação principal. Aperte o botão, fotografe a tela 1. Deixe ≥ 2 registros.
3. Exporte: observe o ano dos novos registros e o `RTC_SET_END` ao final. Rode `verificar_csv.py` (deve acusar "ano 2000").
4. Exporte de novo após o ajuste: os registros novos devem ter a data correta e os de ano 2000 devem permanecer.
5. Recoloque a CR2032 com tudo desligado.

| Item | Parte A | Parte B |
|---|---|---|
| Texto do OLED (`RTC:...`, GERAL) | | |
| Horário do 1º registro novo × hora real (diferença, s) | | |
| Registros com ano 2000 (quantidade) | | |
| Tensão da CR2032 (V) | | |
| Exportação funcionou normalmente? | | |

**Desconhecido:** a deriva do RTC por hora/dia (hoje não há comando para ler o RTC; ver Seção 7). A diferença da Parte A é uma medida **conjunta** de deriva e do tempo de religamento.
**Critério sugerido:** Parte A: diferença de poucos segundos e `RTC:VALIDO`; Parte B: perda de hora detectada (`RTC:INVALIDO`) e comportamento igual ao previsto acima.

### Teste 5 — Watchdog
**Esperado [derivado do código]:** o `WDTTEST` trava a tarefa de sensores no **próximo** ciclo (até 10 s depois); 30 s depois ela é considerada atrasada, a alimentação do IWDG
para, e o reset ocorre após o tempo do IWDG (`4096 × 128 / f_LSI`: 16,4 s com LSI nominal de 32 kHz; 11 a 31 s se a LSI for 17 a 47 kHz [confirmar no datasheet]).
Tempo do comando até o reset: **≈ 46 a 56 s** com LSI nominal; faixa total ≈ 41 a 71 s.
Antes do reset, o `DIAG` deve mostrar `wdgstale` com o bit dos sensores (sensor "atrasado") a partir de ~30 s depois do travamento [derivado].

**Procedimento (em uma CÓPIA do projeto, com backup)**
1. Exporte os dados atuais (**sem** apagar) → `REF.csv`.
2. Na cópia, ponha `ENABLE_WDT_TEST 1` em `app_config.h`, compile e grave. `diag.py` > `T5_diag_antes.txt` (anote `wdgrst`).
3. `python tools/diag.py --port COM3 --cmd WDTTEST` (resposta esperada: `WDTTEST,ACEITO`). **Inicie o cronômetro.** O LED deixa de alternar quando a tarefa trava.
4. Aos ~35–40 s, rode `diag.py` e salve (procure `wdgstale`). Pare o cronômetro quando a placa reiniciar (OLED reinicia, LED volta).
5. `diag.py` > `T5_diag_depois.txt`: esperado "último reset: IWDG" e `wdgrst` +1.
6. Exporte → `DEPOIS.csv` e rode `comparar_csv.py REF.csv DEPOIS.csv` (prova que as EEPROMs sobreviveram).
7. Repita 3 vezes. **Volte `ENABLE_WDT_TEST` para 0** e regrave a versão normal.

| Repetição | Tempo comando→reset (s) | `wdgstale` visto? | `wdgrst` antes→depois | `comparar_csv` |
|---|---|---|---|---|
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |

**Limites:** só a tarefa de sensores é travada pelo teste. O travamento da `defaultTask` ou da `oledTask` **não** tem gatilho no firmware (exigiria código de teste). O valor real da LSI do seu chip não é conhecido.
**Critério sugerido:** reset em todas as repetições, tempo dentro (ou perto) da faixa prevista, dados preservados, sistema normal depois.

---
## ETAPA 4 — Modularidade e desempenho

### Teste 6 — Combinações de sensores
**Esperado [código]:** máscaras: AHT=1, BMP280=2, MPU6500=4, BH1750=8. Módulo ausente não é erro. Campos de sensor ausente ficam **vazios** no CSV. A tela mostra `SENSORES: n/m` com m = sensores que responderam desde o boot.
**Regra de segurança:** troque módulos **somente com bateria e USB desconectadas**. Trocar com o sistema ligado não é suportado.

| Config | Sensores | `sensor_mask` esperada | `sens`/`sdet` no `diag` | OLED |
|---|---|---|---|---|
| A | AHT20+BMP280 e MPU6500 | 7 | 7 / 7 | `SENSORES: 3/3` |
| B | A + BH1750 | 15 | 15 / 15 | `SENSORES: 4/4` |
| C1 | só AHT20+BMP280 | 3 | 3 / 3 | `SENSORES: 2/2` |
| C2 | só MPU6500 | 4 | 4 / 4 | `SENSORES: 1/1` |

(Os valores esperados são [derivado]; confirme no ensaio.)

**Procedimento (para cada configuração)**
1. Tudo desligado; monte a configuração. Ligue. `diag.py` > `T6_<cfg>_diag.txt` (confira `sens`/`sdet`). Fotografe a tela 1.
2. Deixe ≥ 10 min (≥ 60 registros). Exporte **sem apagar** e salve `T6_<cfg>.csv`; `verificar_csv.py` (confira "Mascara de sensores" e "máscara incoerente" = 0).
3. **Prova de tamanho variável:** rode A por ~5 min, desligue, acrescente o BH1750, rode B por ~5 min e exporte **sem apagar**: o mesmo CSV deve conter linhas com máscara 7 e 15, e `bh_lux` vazio nas linhas antigas.
4. Para o BH1750: cubra o sensor com a mão por alguns ciclos e confira a queda do `bh_lux` no CSV.
5. *(Opcional, caso extremo)* nenhum módulo conectado: pelo código nada é gravado e o OLED mostra `ERRO:SEM SENS`; depois de 3 ciclos o firmware tenta recuperar o I2C1.

| Config | Inicializou? | `sens`/`sdet` | Máscara no CSV | Registros gravados? | Observações |
|---|---|---|---|---|---|
| A | | | | | |
| B | | | | | |
| C1 | | | | | |
| C2 | | | | | |

**Observações:** o ENS160 está fora dos ensaios (ver README). Não conecte dois módulos com AHT (mesmo endereço 0x38).

### Teste 7 — Estabilidade prolongada (4 a 8 h)
**Esperado [derivado]:** `T/10 + 1` registros (firmware V13.1): 4 h → 1441; 8 h → 2881. Com a configuração A (30 B) a memória enche em ≈ 12,1 h (4.360 registros). A configuração de quatro sensores (32 B, máscara 15, ≈ 4.088 registros) **não é possível**: a PCB comporta dois módulos de sensores.

**Procedimento**
1. Exporte e limpe. **Alimente pela USB do PC** (assim o `diag.py` pode ser consultado sem reiniciar). Anote o início e `diag.py` > `T7_diag_0h.txt`.
2. A cada hora, rode `diag.py` (salve) e anote qualquer evento (travamento, OLED, LED parado).
3. No fim: `diag.py`, exporte **duas vezes seguidas sem apagar** (`T7_a.csv`, `T7_b.csv`), `comparar_csv.py T7_a.csv T7_b.csv` (o export é reproduzível?) e `verificar_csv.py` no último.

| Hora | `failc` | `recov` | `wdgrst` | `wdgstale` | `stkdef`/`stksen`/`stkoled` | `heapmin` | Observações |
|---|---|---|---|---|---|---|---|
| 0 h | | | | | | | |
| 1 h | | | | | | | |
| … | | | | | | | |
| fim | | | | | | | |

Registrar também: registros esperados × exportados × informados pelo STM32, lacunas, diferença entre as duas exportações.
**Critério sugerido:** sem reinício nem watchdog, `failc=0`, margens de pilha/heap estáveis (sem tendência de queda), duas exportações idênticas.
**Atenção:** este ensaio na USB **não** mede autonomia. Para autonomia, use a medição de energia (Etapa 5).

---
## ETAPA 5 — Consumo energético

**Regras de segurança:** multímetro de corrente **sempre em série**, nunca em paralelo com a bateria; comece pela escala maior; não mexa na ligação da 18650 com o circuito energizado;
não use depurador/ST-Link alimentando a placa durante a medição. Anote o modelo do multímetro, a escala e a tensão da bateria em **cada** leitura (o boost puxa mais corrente com tensão menor).
Repita 3 leituras de 30–60 s e registre típico e máximo.

### 5.1 Matriz de builds (para isolar cada otimização)
| Build | `app_config.h` | Serve para isolar |
|---|---|---|
| V12d | (versão anterior) | linha de base: **27 mA** (OLED aceso, MPU ligado) [medido] |
| V13 padrão | padrão | efeito total |
| V13-a | `ENABLE_IDLE_WFI 0` | efeito do `WFI` = (V13-a) − (V13 padrão) |
| V13-b | `OLED_AUTO_OFF_MS 0U` | efeito de apagar o OLED = (V13-b) − (V13 padrão) |

### 5.2 O que medir (lado da bateria, mesma configuração de sensores da linha de base)
| Situação | Como | Medido (mA) | Tensão da bateria (V) |
|---|---|---|---|
| Funcionamento normal, OLED aceso (primeiros 30 s) | | | |
| Funcionamento normal, OLED apagado (depois de ≥ 40 s) | | | |
| V13-a (sem `WFI`), OLED apagado | | | |
| V13-b (OLED sempre aceso) | | | |

**Eficiência da cadeia de alimentação:** meça `V5` (tensão no pino 5V da Black Pill) e `I5` (corrente em série entre a saída do MT3608 e o pino 5V). Com `Vbat`/`Ibat`:
boost = (V5·I5)/(Vbat·Ibat); perda no regulador de 3,3 V = (V5−3,3)·I5; cadeia = (3,3·I5)/(Vbat·Ibat). Estimativa anterior ≈ 53 % **depende de 80 % de boost supostos** [derivado].

### 5.3 Pontos do roteiro original e o que é possível
| Pedido | Situação |
|---|---|
| Funcionamento normal | Medir com o multímetro (5.2). |
| Durante a aquisição | **Limite:** o multímetro dá média lenta; os picos de cada ciclo (≈ décimos de segundo a cada 10 s) exigiriam um resistor shunt e um osciloscópio. Sem isso, registre como limitação. |
| Durante a exportação | A placa está na USB (bateria desconectada); só interessa se você tiver um medidor USB. Pode ficar fora, declarando o motivo. |
| Modo de baixo consumo | Não existe STOP/Standby no projeto. O que existe na V13: OLED que apaga, MPU6500 que dorme, `WFI`. Os efeitos vêm da matriz 5.1. |

### 5.4 Autonomia
Estimativa (modelo de potência constante): `autonomia ≈ energia útil (Wh) / (Ibat × Vbat)`, com energia útil = capacidade × 3,7 V × ~80 % [suposição].
**Confirme a capacidade real da sua célula.** Um teste de descarga completa **não é recomendado** sem confirmar que o seu módulo tem proteção contra descarga profunda
(nem todo módulo TP4056 tem); não descarregue a 18650 abaixo de ~3 V.

---
## 7. Limites do que as ferramentas atuais conseguem demonstrar (e possíveis acréscimos)
| Lacuna | Hoje | Acréscimo possível |
|---|---|---|
| Detecção de CRC incorreto | só por leitura do código | teste no PC que corrompe bytes de um registro e mostra o CRC detectando |
| Deriva do RTC | só medida conjunta (Teste 4) | comando para ler o RTC e o script imprimir (hora do PC − hora do RTC) antes de ajustar |
| Registro gravado com hora inválida | não é marcado | bit de "hora inválida" no registro (muda o formato; exige decisão) |
| Duração da exportação | cronômetro manual | o firmware ou o script informar a duração |
| LSI real (tempo do watchdog) | desconhecida | medir a LSI com um timer |
| Picos de corrente | indisponível | shunt + osciloscópio |
| Travamento da `defaultTask`/`oledTask` | sem gatilho | comandos de teste (só em build de teste) |

---
## 8. Checklist final de evidências
- [ ] CSVs originais + hashes SHA-256 · [ ] relatórios `verificar_csv` · [ ] saídas do `diag.py` (início/meio/fim) · [ ] saídas do `comparar_csv`
- [ ] fotos do OLED (tela 1 e 2; `RTC:INVALIDO`; `SENSORES n/m` por configuração) · [ ] tabela do watchdog (3 repetições)
- [ ] tabelas de consumo preenchidas, com a tensão da bateria · [ ] versão do firmware e do `app_config.h` em cada teste · [ ] lista de limitações declaradas
