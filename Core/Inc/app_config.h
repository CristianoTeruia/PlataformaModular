/**
 * @file app_config.h
 * @brief Opções de compilação do firmware.
 *
 * Cada opção pode ser sobrescrita na linha de comando do compilador, por exemplo
 * `-DENABLE_WDT_TEST=1`. Os valores aqui são os da versão de uso normal.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifndef ENABLE_WDT_TEST
/**
 * @brief Habilita (1) ou não (0) o comando USB `WDTTEST`.
 *
 * O comando trava de propósito a `sensorTask` para demonstrar a atuação do watchdog.
 * Mantenha 0 na versão de uso normal.
 */
#define ENABLE_WDT_TEST 0
#endif

#ifndef ENABLE_IDLE_WFI
/**
 * @brief Faz a tarefa ociosa do FreeRTOS dormir com a instrução `WFI` (1) ou não (0).
 *
 * Reduz o consumo, pois o núcleo espera a próxima interrupção em vez de executar o laço
 * ocioso. Use 0 ao depurar com breakpoints, porque o sono do núcleo pode interferir com
 * alguns depuradores.
 */
#define ENABLE_IDLE_WFI 1
#endif

#ifndef OLED_AUTO_OFF_MS
/**
 * @brief Tempo sem atividade, em milissegundos, após o qual o OLED é apagado.
 *
 * O botão (PA0) ou o início de uma operação USB acendem o display novamente.
 * O valor 0 desativa o desligamento automático (display sempre aceso).
 */
#define OLED_AUTO_OFF_MS 30000U
#endif

#endif /* APP_CONFIG_H */
