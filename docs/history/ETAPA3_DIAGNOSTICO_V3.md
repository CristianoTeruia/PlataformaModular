# Etapa 3 — Diagnóstico V3 do OLED (sem correção)

## Objetivo
Esta versão adiciona observações passivas para investigar a corrupção do texto do SSD1306. Ela **não tenta corrigir**, recuperar o barramento, limpar EEPROMs ou mudar o formato dos registros.

## Instrumentação adicionada
- `RENDER_OVERLAP`: número de vezes em que `UpdateDisplaySnapshot()` entrou enquanto outra execução dessa função ainda estava ativa. Um valor maior que zero é evidência de sobreposição de renderização; zero não exclui toda forma de concorrência no buffer.
- `BUFFER_MUT`: número de transferências de página durante as quais o checksum do buffer SSD1306 mudou entre a medição imediatamente antes e depois da transmissão I2C. Um valor maior que zero é evidência de que o buffer foi alterado durante uma transmissão; zero não prova ausência de todas as corridas.
- `BUF_HASH_PRE` e `BUF_HASH_POST`: os últimos checksums antes/depois de uma transferência de pixels. São úteis junto com `BUFFER_MUT`.
- Os campos aparecem na mensagem de erro de leitura da EEPROM1, junto com os diagnósticos I2C já existentes.

## Procedimento
1. Faça backup do projeto atual no STM32CubeIDE antes de importar qualquer arquivo.
2. Importe/abra este projeto V3 e compile no STM32CubeIDE. Esta versão foi editada estaticamente, mas **não foi compilada nem testada na placa por mim**.
3. Grave o firmware apenas se a compilação terminar sem erros. Não execute `CLEAR`, rotinas de formatação ou testes de escrita.
4. Observe o OLED durante o funcionamento normal e registre se o texto mistura páginas e em quanto tempo isso ocorre.
5. Se a exportação falhar, execute `python exportar_eeprom.py --port COM3` com PuTTY fechado e copie a linha completa `ERRO,FALHA_LEITURA_EEPROM1,...`.
6. Se o firmware travar sem produzir a linha de erro, anote se o LED continua aceso, se o OLED está congelado e se o problema ocorreu com o OLED conectado. Não apague as EEPROMs; reinicie a placa somente depois de registrar os sintomas.

## Limitações
A instrumentação é observacional e não prova, sozinha, a causa raiz. A falha de exportação pode ocorrer antes de a função de exportação começar (por exemplo, enquanto espera o mutex das EEPROMs), caso em que a mensagem detalhada não será emitida. Esta versão não adiciona uma recuperação automática nem altera a sincronização do buffer, justamente para evitar mascarar o problema antes de coletar evidências.
