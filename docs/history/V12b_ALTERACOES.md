# V12b - correcao da inicializacao do ENS160

NAO foi compilada para ARM nem testada com o sensor real. Faca Clean + Build antes de gravar.
Mudanca limitada a Core/Src/ens160.c/.h, ao DIAG (app_tasks.c) e ao diag.py. O restante e igual a V12.

## Problema observado (diag.py da V12)
ENS160 reconhecido (endereco 0x53, PART_ID 0x160, leitura de status OK), mas DEVICE_STATUS = 0x01:
bit 0x80 (modo em execucao) e bit 0x02 (dado novo) desligados e bit 0x01 (NEWGPR) ligado.
O sensor respondia mas nao media. O driver descartava a amostra e o sensor nunca virava
"disponivel" (sens/sdet sem o bit 4; nenhum dado do ENS160 no CSV).

## Causa
A V12 gravava o modo STANDARD (0x02) direto. Os drivers de referencia (ESPHome, SparkFun, driver do
kernel do Linux e outros) usam: [RESET 0xF0] -> IDLE (0x01) -> COMMAND NOP (0x00) + CLRGPR (0xCC)
-> STANDARD (0x02). O CLRGPR apaga o flag NEWGPR, que e o bit ligado no status observado.

## Correcao
- 1a tentativa: IDLE -> NOP + CLRGPR -> STANDARD (sem RESET, para nao interromper um sensor que ja mede).
- Se o sensor nao entrar em operacao (ou der erro), a tentativa seguinte inclui RESET.
- Depois de iniciar o modo, espera ate 1,6 s pelo dado novo (NEWDAT) cedendo a CPU, em vez de ler
  valores antigos. Em regime normal nao ha espera nem reinicializacao.
- Se a placa reiniciar e o sensor continuar em STANDARD, o driver nao mexe nele.
- DIAG novos campos: ensop (OPMODE lido; 2 = standard) e ensinit (quantas inicializacoes foram feitas).

## Teste no PC (nao substitui o teste com o sensor)
Simulador ESTRITO do ENS160: so entra em operacao se receber IDLE e CLRGPR antes de STANDARD e so
gera dado apos 1 s. 840.340 verificacoes, 0 falhas. Com o driver antigo da V12 a mesma suite falha
(sensor nunca mede), reproduzindo o sintoma da bancada. O simulador reflete meu entendimento dos
drivers de referencia, nao o hardware real.

## O que esperar no diag.py apos gravar
- Em ate ~10 s: "sensores respondendo agora" inclui ENS160; OLED "SENSORES: 3/3" (AHT, BH1750, ENS160).
- ENS160: OPMODE 2, inicializacoes >= 1, DEVICE_STATUS com 0x80 ligado e validade (bits 0x0C) 1 ou 2
  nas primeiras horas (ex.: 0x86 ou 0x8A); sera 0x82 depois do aquecimento e da partida inicial.
- Se continuar sem dados, envie o diag.py inteiro. Interpretacao:
  * ensinit sobe a cada ciclo e ensop != 2: a gravacao do modo nao esta pegando (alimentacao/fiacao).
  * ensst com bit 0x40: erro interno do sensor.
  * ensop = 2 e ensst sem 0x02 por muito tempo: o sensor nao esta gerando medicoes (verifique a
    alimentacao: o ENS160 consome dezenas de mA).
