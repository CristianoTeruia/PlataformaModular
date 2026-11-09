# V8 - diagnostico e recuperacao do OLED travado (mantem as correcoes da V7)

NAO foi compilada para ARM nem testada na placa (so checagem de sintaxe no PC).
Faca Clean + Build antes de gravar. Nao execute CLEAR nos testes.

## O que a V8 acrescenta
1. Comando USB `DIAG` (ou `python diag.py --port COM3`): devolve o estado interno,
   mesmo com o OLED congelado. Nao interfere na exportacao.
2. A tarefa do OLED registra em que ponto esta (`stage`) e conta atualizacoes OK/falhas.
3. Recuperacao automatica: se 2 atualizacoes seguidas do OLED falharem, a tarefa
   libera o barramento I2C2 (ate 9 pulsos de SCL + STOP), reinicia o periferico
   (SWRST) e reinicializa o SSD1306. No maximo 1 vez a cada 5 s.

## Como testar
1. Grave a V8, deixe 1 min, rode `exportar_eeprom.py` como antes.
2. Se o OLED travar, ANTES de reiniciar a placa: `python diag.py --port COM3`.
3. Copie a saida inteira. Espere 10 s e rode de novo (para ver o que mudou).

## Como ler o DIAG
- stage: 0 ocioso | 1 esperando mutex dos sensores | 2 esperando mutex da EEPROM |
  3 renderizando | 4 esperando mutex do I2C2 | 5 transferindo pelo I2C2 | 6 recuperando.
- loops: so cresce se a tarefa do OLED esta viva. Parado = tarefa presa.
- okc / failc / consec: atualizacoes OK, falhas, e falhas consecutivas.
- recov / recst: quantas recuperacoes foram feitas e o resultado da ultima (0 = HAL_OK).
- sw: trocas de pagina feitas; deve crescer 1 a cada 3 s.
- ssd: status HAL da ultima atualizacao (0 OK, 1 erro, 2 BUSY, 3 timeout).
- i2cst / i2cerr: estado e codigo de erro da HAL do I2C2 (i2cerr 32 = timeout).
- busy / sda / scl: flag BUSY do periferico e nivel das linhas (sda=0 ou scl=0 = linha presa).
- stkdef/stksen/stkoled: menor folga de stack ja vista, em bytes. Abaixo de ~300 e perigoso.
- heap / heapmin: heap livre agora e o minimo ja visto.
