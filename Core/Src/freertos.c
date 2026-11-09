/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_config.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* Padrao de piscadas do LED (PC13) quando o sistema para por falha detectada:
 *   2 piscadas + pausa = estouro de stack de uma task (nome em diag_overflow_task_name)
 *   3 piscadas + pausa = falha de alocacao no heap do FreeRTOS
 *   4 piscadas + pausa = HardFault (registradores em diag_fault_*)
 * Se o LED simplesmente parar (sem piscar), o sistema NAO caiu em nenhuma dessas falhas. */
/** @brief Nome da tarefa que estourou a pilha (preenchido por vApplicationStackOverflowHook()). */
volatile char diag_overflow_task_name[configMAX_TASK_NAME_LEN + 1U];
volatile uint32_t diag_fault_cfsr, diag_fault_hfsr, diag_fault_bfar, diag_fault_mmfar;  /**< Registradores de falha capturados pelo HardFault_Handler (CFSR, HFSR, BFAR e MMFAR). */
volatile uint32_t diag_fault_pc, diag_fault_lr;  /**< Valores de PC e LR no momento do HardFault. */

/** @brief Espera ocupada de @p n iterações; usada só no padrão de piscadas, com as interrupções desligadas. */
static void Diag_BusyDelay(volatile uint32_t n) { while (n--) { __NOP(); } }

void Diag_FaultBlink(uint8_t blinks)
{
    __disable_irq();
    /* V10: grava o motivo no registrador de backup (sobrevive ao reset). Sem ninguem
     * alimentando o IWDG, a placa reinicia sozinha em 11 a 31 s. Codigo lido no boot. */
    RTC->BKP2R = (uint32_t)(0xFA00U | blinks);
    for (;;)
    {
        for (uint8_t i = 0U; i < blinks; i++)
        {
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            Diag_BusyDelay(4000000UL);
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            Diag_BusyDelay(4000000UL);
        }
        Diag_BusyDelay(24000000UL);
    }
}

/**
 * @brief Gancho do FreeRTOS chamado no estouro de pilha de uma tarefa.
 *
 * Guarda o nome da tarefa em `diag_overflow_task_name` e para o sistema com 2 piscadas do LED.
 * @param[in] xTask      Tarefa que estourou a pilha (não usada).
 * @param[in] pcTaskName Nome da tarefa.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    for (uint32_t i = 0U; i < configMAX_TASK_NAME_LEN; i++)
    {
        diag_overflow_task_name[i] = (pcTaskName != NULL) ? pcTaskName[i] : '?';
        if ((pcTaskName == NULL) || (pcTaskName[i] == '\0')) break;
    }
    Diag_FaultBlink(2U);
}

#if ENABLE_IDLE_WFI
/* Tarefa ociosa: o nucleo dorme ate a proxima interrupcao (SysTick, USB...). */
/** @brief Gancho da tarefa ociosa: o núcleo dorme (`WFI`) até a próxima interrupção, para economizar energia. */
void vApplicationIdleHook(void)
{
    __WFI();
}
#endif

/** @brief Gancho do FreeRTOS chamado em falha de alocação no heap: para o sistema com 3 piscadas do LED. */
void vApplicationMallocFailedHook(void)
{
    Diag_FaultBlink(3U);
}

/* USER CODE END Application */

