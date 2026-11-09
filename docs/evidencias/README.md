# Evidências dos ensaios de 04/11/2026

CSVs exportados do datalogger (firmware V13 e V13.1) e relatórios gerados por `tools/verificar_csv.py`.
Os números abaixo foram **calculados a partir dos próprios arquivos**. A coluna "Descrição" diz a que ensaio o
arquivo se refere; onde está "a confirmar", a associação é uma inferência do conteúdo, não um registro do ensaio.

| Arquivo | Registros | Primeiro → último | Máscaras (quantidade) | Reg. ano 2000 | Lacunas > 12 s | Relatório | Descrição |
|---|---|---|---|---|---|---|---|
| `dados_sensores_20261104_181715.csv` | 193 | 04/11 17:44:08 → 04/11 18:17:07 | 7: 193 | 0 | 0 | sim | Teste 1, 1a execução (antes da correção do agendamento) (confirmada pelo autor) |
| `dados_sensores_20261104_183348.csv` | 283 | 04/11 17:44:08 → 04/11 18:33:44 | 7: 283 | 0 | 3 |  | mesma série do 181715 (mesmo 1o registro), exportada depois; tem 3 lacunas > 12 s (a confirmar) |
| `dados_sensores_20261104_190431.csv` | 181 | 04/11 18:34:24 → 04/11 19:04:23 | 7: 181 | 0 | 0 | sim | Teste 1 refeito (V13.1, com a correção) (confirmada pelo autor) |
| `dados_sensores_20261104_191008.csv` | 215 | 04/11 18:34:24 → 04/11 19:10:04 | 7: 215 | 0 | 0 |  | Teste 3, exportação A (confirmada pelo autor) |
| `dados_sensores_20261104_193054.csv` | 340 | 04/11 18:34:24 → 04/11 19:30:53 | 7: 340 | 0 | 0 | sim | Teste 3, exportação B (confirmada pelo autor) |
| `dados_sensores_20261104_193932.csv` | 391 | 04/11 18:34:24 → 04/11 19:39:23 | 7: 391 | 0 | 0 |  | mesma série do Teste 3, exportação posterior (a confirmar) |
| `dados_sensores_20261104_195617.csv` | 393 | 04/11 18:34:24 → 04/11 19:56:08 | 7: 393 | 0 | 2 |  | mesma série; 2 lacunas (compatível com interrupção da alimentação, Teste 4) (a confirmar) |
| `dados_sensores_20261104_195825.csv` | 403 | 04/11 18:34:24 → 01/01 00:00:35 | 7: 403 | 4 | 3 | sim | mesma série; termina em registro com ano 2000 (compatível com perda total de energia, Teste 4) (a confirmar) |
| `dados_sensores_20261104_195932.csv` | 409 | 04/11 18:34:24 → 04/11 19:59:26 | 7: 409 | 4 | 3 | sim | mesma série; 4 registros de ano 2000 no meio, depois data correta (compatível com Teste 4) (a confirmar) |
| `dados_sensores_20261104_201259.csv` | 463 | 04/11 18:34:24 → 04/11 20:12:50 | 7: 463 | 4 | 4 |  | mesma série; mais lacunas (compatível com os reinícios do Teste 5) (a confirmar) |
| `dados_sensores_20261104_202105.csv` | 490 | 04/11 18:34:24 → 04/11 20:21:04 | 7: 490 | 4 | 9 |  | mesma série; mais lacunas (compatível com os reinícios do Teste 5) (a confirmar) |
| `dados_sensores_20261104_210648.csv` | 230 | 04/11 20:27:04 → 04/11 21:06:40 | 11: 91, 3: 78, 7: 61 | 0 | 2 |  | Teste 6 (combinações de sensores em um só CSV): o conteúdo (máscaras 7, 11 e 3) é compatível com o relato do autor (a confirmar o nome do arquivo) |

Notas:
- Vários arquivos são exportações sucessivas da **mesma série** (mesmo primeiro registro), sem limpar a memória.
- Os 4 registros de ano 2000 permanecem nas exportações seguintes: o ajuste do RTC **não corrige** registros antigos.
- O arquivo do Teste 6 mistura registros de tamanhos diferentes na mesma memória (máscara 7 = 30 B, 11 = 20 B, 3 = 18 B).
  O `bh_lux` nos registros de máscara 11 vai de 25,0 a 75,0 lx em 91 registros.
- "Lacunas" conta saltos de horário maiores que 12 s e até 24 h (desligamentos, reinícios, trocas de módulo).
- Para gerar novas evidências: `python tools/exportar_eeprom.py` e `python tools/verificar_csv.py arquivo.csv`; mova os
  arquivos para esta pasta (o `.gitignore` só versiona CSVs e relatórios daqui).
