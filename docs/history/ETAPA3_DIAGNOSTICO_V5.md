# Etapa 3 — Diagnóstico V5 (OLED / EEPROM / USB)

## Alteração desta versão

- `Core/Src/main.c`: a tarefa `StartOledTask()` deixa de esperar indefinidamente pelo mutex da EEPROM. A tentativa tem timeout de aproximadamente 100 ms, calculado a partir da frequência do tick do RTOS. Se a EEPROM estiver ocupada (por exemplo, durante `EXPORT`), a tarefa preserva a tela atual, solicita nova atualização e cede tempo ao RTOS antes de tentar novamente.
- Nenhuma rotina de escrita, apagamento ou formatação dos registros da EEPROM foi alterada.
- O timeout de 250 ms da tarefa dos sensores já existente na V4 foi preservado.

## O que esta mudança pretende resolver

Evitar que a tarefa de baixa prioridade do OLED fique presa indefinidamente esperando `eepromMutexHandle` enquanto outra tarefa mantém esse mutex durante uma operação longa.

## Limites do diagnóstico

Esta alteração **não identifica nem corrige por si só a causa da exportação USB sem dados**. A ausência de `EXPORT_BEGIN` ainda pode ocorrer antes ou durante a primeira transmissão USB, e requer diagnóstico específico da validação dos metadados e dos retornos de `CDC_Transmit_FS()`.

## Validação

- Alteração aplicada por substituição exata de um único bloco; o script abortou se o bloco esperado não fosse encontrado.
- ZIP verificada quanto à integridade estrutural.
- **Não compilada no STM32CubeIDE e não testada na BlackPill** neste ambiente.
- Antes de testar no hardware, compilar e verificar avisos/erros no STM32CubeIDE.

## Teste sugerido após compilação

1. Inicializar e confirmar a atualização normal do OLED.
2. Solicitar `EXPORT` uma única vez.
3. Observar se o OLED mantém a última tela sem travar enquanto a EEPROM está ocupada.
4. Observar LED e retorno da exportação; registrar qualquer mensagem recebida.
5. Se a exportação continuar sem dados, não repetir em sequência: guardar o log para o próximo diagnóstico da primeira transmissão USB.

V5 é uma versão de diagnóstico incremental, não uma declaração de correção definitiva.
