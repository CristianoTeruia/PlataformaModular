/**
 * @file app_tasks.h
 * @brief Tarefas FreeRTOS e funções de aplicação (definidas em app_tasks.c).
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "cmsis_os.h"

/**
 * @brief Cria os mutexes e as tarefas adicionais da aplicação.
 *
 * Cria quatro mutexes (EEPROM, I²C2, dados de sensores e RTC) e as tarefas `sensorTask` e
 * `oledTask`. A `defaultTask` é criada pelo código gerado do CubeMX e executa App_CommandTask().
 * Deve ser chamada antes de `osKernelStart()` (bloco `USER CODE` de threads do `main.c`);
 * chama `Error_Handler()` se alguma criação falhar.
 */
void App_RTOS_Create(void);

/**
 * @brief Corpo da `defaultTask`: alimenta o watchdog e atende os pedidos recebidos pela USB.
 *
 * Executa, fora da callback USB, os comandos `DIAG`, `RTC_SET`, `CLEAR,CONFIRMO` e `EXPORT`.
 * Não retorna.
 * @param[in] argument Não utilizado.
 */
void App_CommandTask(void *argument);

/**
 * @brief Tarefa de aquisição de sensores (prioridade abaixo da normal).
 *
 * A cada #SENSOR_ACQUISITION_INTERVAL_MS lê todos os sensores da tabela, grava um registro na
 * EEPROM se pelo menos um respondeu e alterna o LED. Espera até 250 ms pelo mutex da EEPROM;
 * se não o obtiver, a amostra é descartada. Após 3 ciclos seguidos sem nenhum sensor, tenta
 * recuperar o I²C1, no máximo uma vez a cada 30 s. Não retorna.
 * @param[in] argument Não utilizado.
 */
void StartSensorTask(void *argument);

/**
 * @brief Tarefa do display (prioridade baixa, ciclo de 20 ms).
 *
 * Aplica a política de energia do OLED (OledPower), alterna as telas a cada 3 s, copia o estado
 * compartilhado sob mutexes curtos e só então renderiza. Após uma atualização com falha, recupera
 * o I²C2 e o SSD1306, no máximo uma vez a cada 5 s. Não retorna.
 * @param[in] argument Não utilizado.
 */
void StartOledTask(void *argument);

/**
 * @brief Solicita a atualização do OLED.
 *
 * Com o RTOS em execução apenas marca o pedido, que a `oledTask` atende; antes do início do
 * kernel, atualiza o display diretamente.
 */
void UpdateDisplaySynchronized(void);

#endif /* APP_TASKS_H */
