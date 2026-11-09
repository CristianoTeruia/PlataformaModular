/**
 * @file app_watchdog.c
 * @brief Implementação da supervisão do watchdog (ver app_watchdog.h).
 */
#include "app_watchdog.h"
#include "main.h"

extern IWDG_HandleTypeDef hiwdg;
extern RTC_HandleTypeDef hrtc;

volatile uint32_t wdg_reset_csr = 0U;
volatile uint32_t wdg_iwdg_reset_count = 0U;
volatile uint32_t wdg_last_fault_code = 0U;
volatile uint8_t  wdg_stale_mask = 0U;
volatile uint8_t  wdg_test_hang = 0U;

/* Prazo maximo sem atividade, em ms (a defaultTask nao tem prazo: ela mesma executa a verificacao). */
static const uint32_t wdg_limit_ms[WDG_TASK_COUNT] = { 0U, 30000U, 10000U };
static volatile uint32_t wdg_last_checkin[WDG_TASK_COUNT];
static uint32_t wdg_last_feed;

void Watchdog_CaptureResetCause(void)
{
    wdg_reset_csr = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void Watchdog_Init(void)
{
    uint32_t fault = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2);
    uint32_t count = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR3);

    if ((fault & 0xFF00U) == 0xFA00U)
    {
        wdg_last_fault_code = fault;
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, 0U);
    }
    if ((wdg_reset_csr & RCC_CSR_IWDGRSTF) != 0U)
    {
        count++;
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR3, count);
    }
    wdg_iwdg_reset_count = count;

    uint32_t now = HAL_GetTick();
    for (uint32_t i = 0U; i < (uint32_t)WDG_TASK_COUNT; i++) wdg_last_checkin[i] = now;
    wdg_last_feed = now;
}

void Watchdog_Checkin(wdg_task_t task)
{
    if ((uint32_t)task < (uint32_t)WDG_TASK_COUNT) wdg_last_checkin[task] = HAL_GetTick();
}

void Watchdog_Service(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t stale = 0U;

    wdg_last_checkin[WDG_TASK_COMMAND] = now;
    for (uint32_t i = 1U; i < (uint32_t)WDG_TASK_COUNT; i++)
    {
        if ((uint32_t)(now - wdg_last_checkin[i]) > wdg_limit_ms[i]) stale |= (uint8_t)(1U << i);
    }
    wdg_stale_mask = stale;

    if ((stale == 0U) && ((uint32_t)(now - wdg_last_feed) >= 100U))
    {
        (void)HAL_IWDG_Refresh(&hiwdg);
        wdg_last_feed = now;
    }
}
