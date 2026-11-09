# V11 - registro de sensores (refatoracao SEM mudar o resultado)

NAO foi compilada para ARM nem testada na placa (so checagem de sintaxe + teste no PC).
Faca Clean + Build antes de gravar. Base: V10 (watchdog validado).

## Objetivo
Preparar a modularidade: cada sensor I2C passa a ser UMA entrada em uma tabela
(Core/Src/sensor_registry.c). Registro da EEPROM, exportacao CSV, task de sensores, tela do OLED
e inicializacao apenas percorrem a tabela. A V11 NAO adiciona sensores: os tres atuais (AHT,
BMP280, MPU6500) foram movidos para a tabela e o resultado gravado/exportado deve ser identico.

## O que mudou
- Novos: Core/Src/sensor_registry.c, Core/Inc/sensor_registry.h.
- eeprom24lc512.c: serializacao, tamanho por mascara, decodificacao e cabecalho do CSV via tabela.
  O formato dos registros na EEPROM e o CSV NAO mudaram (mesmos bits, ordem e colunas).
- app_tasks.c / main.c: laco de leitura dos sensores percorre a tabela (estagios 1,2,3 como antes).
- ssd1306.c/.h: a tela passa a comparar "sensores respondendo" com "sensores detectados desde o boot".
- DIAG: novos campos sens e sdet (mascaras). comparar_csv.py: ferramenta de verificacao.

## Unica diferenca visivel (intencional): OLED
Antes: o OLED esperava SEMPRE 3 sensores ("SENSORES: n/3", ERRO se faltasse um modulo).
Agora: o esperado e o que ja respondeu desde o boot ("SENSORES: n/m"). Modulo nao conectado
nao e erro; um sensor que respondeu e depois para de responder gera "ERRO:<nome>".
Sem nenhum sensor detectado: "ERRO:SEM SENS". Com os 3 sensores conectados a tela e igual a V10.
Atencao: um sensor com defeito JA NO BOOT e tratado como "nao conectado" (nao aparece como erro).

## Prova feita no PC (nao substitui o teste na placa)
Codigo de referencia da V10 x codigo novo, mascaras 1..7, 60.000 amostras aleatorias (incluindo
valores extremos): 840.258 verificacoes (bytes do registro, tamanho por mascara das 256 mascaras,
cabecalho e linhas CSV), 0 diferencas.

## Teste na placa (regressao: a saida tem que ser a mesma da V10)
1. Com a V10 gravada: exporte o CSV (ex.: dados_V10.csv).
2. Grave a V11 (a EEPROM nao e apagada ao gravar) e exporte de novo (dados_V11.csv).
3. python comparar_csv.py dados_V10.csv dados_V11.csv
   Esperado: "IDENTICO: N registros ... reproduzidos exatamente".
4. Deixe rodar 10 min: OLED "SENSORES: 3/3" e GERAL OK; diag.py com failc=0, wdgrst=0
   e "sensores respondendo agora": AHT, BMP280, MPU6500.
5. Opcional: desconecte um modulo e reinicie: a tela deve mostrar 2/2 sem erro.

## Como ficara a V12 (para conferir o que a V11 prepara)
Adicionar BH1750 e ENS160 = 2 drivers novos + 2 linhas na tabela + 2 bits na mascara (3 e 4)
+ EEPROM_MAX_RECORD_SIZE 30 -> 40. Registros antigos continuam validos; colunas novas no fim do CSV.
