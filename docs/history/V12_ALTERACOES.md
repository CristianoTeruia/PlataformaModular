# V12 - BH1750 (luminosidade) e ENS160 (qualidade do ar) na tabela de sensores

NAO foi compilada para ARM nem testada com os sensores reais. Faca Clean + Build antes de gravar.
Base: V11 (leitura/exportacao validadas na placa: CSV identico ao da V10, 4360 registros).

## O que foi adicionado
- Core/Src/bh1750.c/.h: medicao unica de alta resolucao; endereco 0x23 ou 0x5C detectado sozinho.
  Dado no registro: contagem bruta de 16 bits (2 bytes); o CSV mostra lux = bruto / 1,2 (1 casa).
- Core/Src/ens160.c/.h: endereco 0x53 ou 0x52 detectado sozinho e confirmado pelo PART_ID (0x0160);
  coloca o sensor em modo STANDARD; compensa com a temperatura e umidade do AHT (quando ha AHT).
  Dado no registro: AQI (1 B) + TVOC ppb (2 B) + eCO2 ppm (2 B) + validade (1 B) = 6 bytes.
- sensor_registry.c: 2 linhas novas na tabela (bits 3 e 4 da mascara). Nada mais precisou mudar:
  registro, exportacao, task de sensores e OLED percorrem a tabela.
- EEPROM_MAX_RECORD_SIZE 30 -> 40 (o maior registro tem 38 bytes).
- DIAG: bha/bhhal (BH1750), ensa/enspid/ensst/enshal (ENS160). diag.py mostra os enderecos.

## Compatibilidade
- Registros antigos (mascara 1..7) continuam validos e exportam igual.
- CSV: 5 colunas novas NO FIM: bh_lux, ens_aqi, ens_tvoc_ppb, ens_eco2_ppm, ens_validity.
  Sensor ausente = campos vazios. O exportar_eeprom.py ja usa o cabecalho enviado pelo firmware.
- Registros novos sao maiores (ate 38 B): a capacidade total cai de ~4.360 para ~3.440 registros
  (cerca de 9,5 h a 10 s) quando todos os sensores estao presentes.

## Validade do ENS160 (coluna ens_validity)
0 = operacao normal | 1 = aquecimento | 2 = partida inicial (primeira vez) | 3 = saida invalida.
Use so as linhas com validade 0 para analise. Nas primeiras leituras apos ligar, valores baixos
de AQI/TVOC/eCO2 com validade 1 ou 2 sao esperados.

## Cuidados de hardware
- AHT20 e AHT21 usam o mesmo endereco (0x38): conecte apenas UM modulo com AHT por vez.
  O modulo AHT20+BMP280 e o modulo ENS160+AHT21 NAO podem ficar conectados juntos.
- Desligue (bateria e USB) antes de conectar ou retirar modulos.
- O ENS160 mantem um aquecedor ligado o tempo todo: o consumo da bateria tende a subir bastante.

## Testes feitos no PC (nao substituem o teste com o hardware)
840.330 verificacoes, 0 falhas: (1) equivalencia com a V10 para os 3 sensores antigos (bytes e CSV);
(2) drivers contra dispositivos I2C SIMULADOS (deteccao dos dois enderecos, remocao e retorno do
sensor, PART_ID errado, erro do ENS160, compensacao e limites, conversao de lux e arredondamento);
(3) registro e CSV com 5 sensores e mascaras parciais. Erros introduzidos de proposito nos drivers
foram detectados pela suite. Registradores e comandos conferidos em bibliotecas de terceiros e no
indice do datasheet; o datasheet completo nao foi lido.

## Roteiro de teste na placa (nesta ordem)
A) So os modulos atuais (AHT20+BMP280 e MPU6500), sem BH1750 nem ENS160:
   1. Grave a V12. Rode exportar_eeprom.py, guarde o CSV e responda SIM quando perguntar sobre
      limpar a memoria (a memoria esta cheia; assim o firmware volta a gravar).
   2. Deixe 5 a 10 min. Exporte de novo. O CSV deve ter 17 colunas, sensor_mask=7 e as 5 colunas novas
      vazias; os valores dos 3 sensores devem parecer normais. diag.py: sens=7.
B) Desligue tudo, conecte o BH1750, ligue. OLED "SENSORES: 4/4". diag.py: BH1750 endereco 0x23 (ou 0x5C).
   Cubra o sensor com a mao e exporte depois: bh_lux deve cair. sensor_mask=15.
C) Desligue tudo, retire o modulo AHT20+BMP280, conecte o modulo ENS160+AHT21, ligue.
   diag.py: ENS160 endereco 0x53 (ou 0x52), PART_ID 0x160. Exporte depois de alguns minutos.
D) Se algo falhar, rode diag.py e me envie a saida inteira (os campos bha, bhhal, ensa, enspid,
   ensst e enshal mostram em que ponto a comunicacao parou).
