/**
 * @file app_watchdog.h
 * @brief Supervisão do watchdog independente (IWDG) por tarefa.
 *
 * O IWDG só é realimentado quando todas as tarefas supervisionadas registraram atividade
 * dentro do prazo de cada uma. Se alguma travar, a realimentação cessa e o IWDG reinicia o
 * sistema, de 11 s a 31 s depois, conforme a frequência real do oscilador LSI do chip.
 *
 * O motivo do último reinício fica em registradores de backup do RTC, que sobrevivem ao reset:
 * `BKP DR2` guarda o código da última falha fatal e `BKP DR3`, o número de reinícios por IWDG.
 */
#ifndef APP_WATCHDOG_H
#define APP_WATCHDOG_H

#include <stdint.h>

/** @brief Tarefas supervisionadas pelo watchdog. */
typedef enum
{
    WDG_TASK_COMMAND = 0, /**< `defaultTask`: marca a si mesma ao chamar Watchdog_Service(). */
    WDG_TASK_SENSOR  = 1, /**< `sensorTask`: prazo de 30 s (o ciclo normal é de 10 s). */
    WDG_TASK_OLED    = 2, /**< `oledTask`: prazo de 10 s (o laço normal é de cerca de 20 ms). */
    WDG_TASK_COUNT   = 3  /**< Número de tarefas supervisionadas. */
} wdg_task_t;

extern volatile uint32_t wdg_reset_csr;        /**< Valor de `RCC->CSR` lido no boot (flags de reset). */
extern volatile uint32_t wdg_iwdg_reset_count; /**< Reinícios causados pelo IWDG (persistente em `BKP DR3`). */
extern volatile uint32_t wdg_last_fault_code;  /**< Última falha fatal (`BKP DR2`): 0xFA02 pilha, 0xFA03 heap, 0xFA04 HardFault. */
extern volatile uint8_t  wdg_stale_mask;       /**< Bits das tarefas fora do prazo na última verificação. */
extern volatile uint8_t  wdg_test_hang;        /**< 1 após o comando `WDTTEST` (só com ENABLE_WDT_TEST). */

/**
 * @brief Salva a causa do último reinício.
 *
 * Copia `RCC->CSR` para #wdg_reset_csr e limpa as flags de reset. Deve ser a primeira
 * chamada de `main()`, antes que algo as apague.
 */
void Watchdog_CaptureResetCause(void);

/**
 * @brief Inicializa a supervisão.
 *
 * Lê os registradores de backup (código da última falha e contador de reinícios), incrementa
 * o contador se o último reset foi do IWDG e inicia os prazos de todas as tarefas.
 * Chamar depois de `MX_RTC_Init()`.
 */
void Watchdog_Init(void);

/**
 * @brief Registra que uma tarefa está viva.
 * @param[in] task Tarefa que está se apresentando (valores fora do intervalo são ignorados).
 */
void Watchdog_Checkin(wdg_task_t task);

/**
 * @brief Verifica os prazos e, se todas as tarefas estão em dia, realimenta o IWDG.
 *
 * Marca a `defaultTask` como viva, atualiza #wdg_stale_mask e realimenta o IWDG quando nenhuma
 * tarefa está atrasada e já passaram pelo menos 100 ms desde a última realimentação.
 * Deve ser chamada pela `defaultTask` e também dentro de operações longas (exportação e limpeza).
 */
void Watchdog_Service(void);

#endif /* APP_WATCHDOG_H */
