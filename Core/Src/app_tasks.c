/**
 * @file app_tasks.c
 * @brief Código de aplicação do FreeRTOS: tarefas, mutexes, recuperação dos barramentos I²C e DIAG.
 *
 * Fica FORA do `main.c` de propósito: o STM32CubeMX regenera o `main.c` e apaga tudo que estiver
 * fora dos blocos `USER CODE`. Este arquivo não é tocado pela regeneração.
 *
 * Tarefas (CMSIS-RTOS2):
 *  - `defaultTask` (prioridade normal): USB, RTC, exportação, limpeza e alimentação do watchdog;
 *  - `sensorTask` (abaixo do normal, pilha de 2048 B): aquisição e gravação a cada 10 s;
 *  - `oledTask` (baixa, pilha de 1536 B): atualização do display e botão.
 *
 * Mutexes: EEPROM (serializa gravação, exportação e limpeza), I²C2 (OLED e EEPROMs), dados dos
 * sensores e RTC.
 */
#include "main.h"
#include "cmsis_os.h"
#include "app_tasks.h"
#include "app_watchdog.h"
#include "sensor_registry.h"
#include "oled_power.h"
#include "bh1750.h"
#include "ens160.h"
#include "aht20.h"
#include "bmp280.h"
#include "mpu6500.h"
#include "ssd1306.h"
#include "eeprom24lc512.h"
#include "app_state.h"
#include "i2c2_bus_lock.h"
#include <stdio.h>
#include <string.h>
#include "usbd_cdc_if.h"

/* RTC values retained for the existing RTC read path. */
RTC_TimeTypeDef rtc_test_time;  /**< Última hora lida do RTC pela `sensorTask`. */
RTC_DateTypeDef rtc_test_date;  /**< Última data lida do RTC pela `sensorTask`. */
uint8_t rtc_time_valid = 0;

/* Estado da politica de energia do OLED e contadores (lidos pelo DIAG). */
static OledPower oled_power_state;  /**< Estado da política de energia do OLED (apaga por inatividade). */
static volatile uint32_t oled_on_count = 0U, oled_off_count = 0U;  /**< Quantas vezes o OLED foi aceso e apagado (lidos pelo DIAG). */

/* Tarefas CMSIS-RTOS2. A aquisição é separada; OLED/comandos continuam na tarefa principal. */
extern osThreadId_t defaultTaskHandle; /* definido no main.c (gerado) */
osThreadId_t sensorTaskHandle;  /**< Identificador da `sensorTask`. */
osThreadId_t oledTaskHandle;  /**< Identificador da `oledTask`. */
osMutexId_t eepromMutexHandle;  /**< Mutex que serializa gravação, exportação e limpeza da EEPROM. */
osMutexId_t i2c2MutexHandle;  /**< Mutex do barramento I²C2 (OLED e EEPROMs). */
osMutexId_t sensorDataMutexHandle;  /**< Mutex dos dados dos sensores e das máscaras de disponibilidade. */
osMutexId_t rtcMutexHandle;  /**< Mutex do acesso ao RTC interno. */
static volatile uint8_t oledRefreshRequested = 0U;  /**< 1 quando há pedido de atualização do OLED para a `oledTask`. */

/* Protege o barramento fisico I2C2 compartilhado pelo OLED e pelas EEPROMs.
 * Antes do escalonador, as inicializacoes ocorrem sequencialmente e nao precisam
 * do mutex. Durante o RTOS, todas as transacoes protegidas usam este lock. */
HAL_StatusTypeDef I2C2_BusAcquire(void)
{
    osKernelState_t state = osKernelGetState();
    if ((state == osKernelRunning) || (state == osKernelReady))
    {
        if (i2c2MutexHandle == NULL) return HAL_ERROR;
        return (osMutexAcquire(i2c2MutexHandle, osWaitForever) == osOK) ? HAL_OK : HAL_ERROR;
    }
    return HAL_OK;
}

/* Protege HAL_RTC_Get/Set entre tasks. Mutex "folha": nunca e segurado enquanto se
 * espera por outro mutex, portanto nao cria deadlock. Antes do RTOS nao faz nada. */
void App_RTC_Lock(void)
{
    if ((osKernelGetState() == osKernelRunning) && (rtcMutexHandle != NULL))
        (void)osMutexAcquire(rtcMutexHandle, osWaitForever);
}

void App_RTC_Unlock(void)
{
    if ((osKernelGetState() == osKernelRunning) && (rtcMutexHandle != NULL))
        (void)osMutexRelease(rtcMutexHandle);
}

void I2C2_BusRelease(void)
{
    osKernelState_t state = osKernelGetState();
    if (((state == osKernelRunning) || (state == osKernelReady)) && (i2c2MutexHandle != NULL))
    {
        (void)osMutexRelease(i2c2MutexHandle);
    }
}



/** @brief Atributos da `sensorTask`: pilha de 2048 B e prioridade abaixo da normal. */
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 2048,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/** @brief Atributos da `oledTask`: pilha de 1536 B e prioridade baixa. */
const osThreadAttr_t oledTask_attributes = {
  .name = "oledTask",
  .stack_size = 1536,
  .priority = (osPriority_t) osPriorityLow,
};

/** Recupera somente o periférico I2C1, utilizado pelos sensores.
 * O OLED e as EEPROMs usam I2C2 e não são afetados por esta rotina.
 * Esta recuperação reinicializa o periférico STM32; não gera pulsos GPIO
 * para liberar uma linha fisicamente presa em nível baixo. */
static HAL_StatusTypeDef RecoverSensorI2C1(void)
{
    HAL_StatusTypeDef status;

    status = HAL_I2C_DeInit(&hi2c1);
    if (status != HAL_OK)
        return status;

    /* HAL_I2C_DeInit preserva os parâmetros hi2c1.Init configurados na
     * inicialização original; HAL_I2C_Init também reconfigura os pinos via MSP. */
    status = HAL_I2C_Init(&hi2c1);
    if (status != HAL_OK)
        return status;

    return HAL_OK;
}

/* Recupera o I2C2 (OLED + EEPROMs): libera uma linha possivelmente presa por um escravo
 * (ate 9 pulsos de SCL + STOP), reinicia o periferico (HAL_I2C_Init faz SWRST) e
 * reinicializa o SSD1306. Segura o mutex do I2C2 durante todo o processo. */
/** @brief Atraso curto (cerca de meio período de SCL) para gerar pulsos manuais na linha. */
static void I2C2_BitDelay(void)
{
    for (volatile uint32_t i = 0U; i < 60U; i++) { __NOP(); }
}

/**
 * @brief Recupera o I²C2 (OLED e EEPROMs) e reinicializa o SSD1306.
 *
 * Obtém o mutex do I²C2 durante todo o processo. Libera uma linha possivelmente presa por um
 * escravo (até 9 pulsos de SCL e uma condição de STOP, com os pinos como saída aberta), reinicia o
 * periférico (`HAL_I2C_Init` faz o SWRST) e executa SSD1306_Init(). Atualiza `i2c2_recovery_count`
 * e `i2c2_recovery_status`.
 * @return Status da reinicialização do periférico ou do display.
 */
static HAL_StatusTypeDef RecoverI2C2AndOled(void)
{
    GPIO_InitTypeDef g = {0};
    HAL_StatusTypeDef st;

    if (I2C2_BusAcquire() != HAL_OK) return HAL_ERROR;

    (void)HAL_I2C_DeInit(&hi2c2);

    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = MEMORY_SCL_Pin; HAL_GPIO_Init(MEMORY_SCL_GPIO_Port, &g);
    g.Pin = MEMORY_SDA_Pin; HAL_GPIO_Init(MEMORY_SDA_GPIO_Port, &g);
    HAL_GPIO_WritePin(MEMORY_SDA_GPIO_Port, MEMORY_SDA_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MEMORY_SCL_GPIO_Port, MEMORY_SCL_Pin, GPIO_PIN_SET);
    I2C2_BitDelay();

    for (uint8_t i = 0U; i < 9U; i++)
    {
        if (HAL_GPIO_ReadPin(MEMORY_SDA_GPIO_Port, MEMORY_SDA_Pin) == GPIO_PIN_SET) break;
        HAL_GPIO_WritePin(MEMORY_SCL_GPIO_Port, MEMORY_SCL_Pin, GPIO_PIN_RESET); I2C2_BitDelay();
        HAL_GPIO_WritePin(MEMORY_SCL_GPIO_Port, MEMORY_SCL_Pin, GPIO_PIN_SET);   I2C2_BitDelay();
    }

    /* Condicao de STOP: SDA sobe com SCL alto. */
    HAL_GPIO_WritePin(MEMORY_SDA_GPIO_Port, MEMORY_SDA_Pin, GPIO_PIN_RESET); I2C2_BitDelay();
    HAL_GPIO_WritePin(MEMORY_SCL_GPIO_Port, MEMORY_SCL_Pin, GPIO_PIN_SET);   I2C2_BitDelay();
    HAL_GPIO_WritePin(MEMORY_SDA_GPIO_Port, MEMORY_SDA_Pin, GPIO_PIN_SET);   I2C2_BitDelay();

    HAL_GPIO_DeInit(MEMORY_SCL_GPIO_Port, MEMORY_SCL_Pin);
    HAL_GPIO_DeInit(MEMORY_SDA_GPIO_Port, MEMORY_SDA_Pin);

    st = HAL_I2C_Init(&hi2c2); /* MSP reconfigura os pinos como I2C2 */
    if (st == HAL_OK)
    {
        st = SSD1306_Init();
    }

    i2c2_recovery_count++;
    i2c2_recovery_status = (uint32_t)st;
    I2C2_BusRelease();
    return st;
}

/** Envia uma linha de diagnostico pela USB esperando a transmissao terminar. */
static void Diag_USB_Send(const char *text)
{
    uint16_t len = (uint16_t)strlen(text);
    for (uint32_t i = 0U; i < 2000U; i++)
    {
        uint8_t r = CDC_Transmit_FS((uint8_t *)text, len);
        if (r == USBD_OK)
        {
            for (uint32_t j = 0U; (j < 2000U) && (CDC_IsTxBusy() != 0U); j++) { (void)osDelay(1U); }
            return;
        }
        if (r != USBD_BUSY) return;
        (void)osDelay(1U);
    }
}

/* Solicita atualização do OLED. Depois que o RTOS inicia, somente StartOledTask
 * acessa o driver/display e o barramento I2C2. Antes do kernel, a atualização é direta. */
void UpdateDisplaySynchronized(void)
{
    if (osKernelGetState() == osKernelRunning)
    {
        oledRefreshRequested = 1U;
    }
    else
    {
        UpdateDisplay();
    }
}


/* Primeira tarefa: preserva a lógica sequencial já validada antes de separar tarefas. */
void App_CommandTask(void *argument)
{
    (void)argument;
    for (;;)
    {
      Watchdog_Service(); /* V10: alimenta o IWDG se todas as tasks estao vivas */

      /* Comando DIAG: estado do OLED/I2C2/tasks, para investigar travamentos sem debugger. */
      if (usb_diag_requested != 0U)
      {
          static char diag_line[1320];
          usb_diag_requested = 0U;
          (void)snprintf(diag_line, sizeof(diag_line),
              "DIAG,tick=%lu,stage=%u,loops=%lu,okc=%lu,failc=%lu,consec=%lu,recov=%lu,recst=%lu,"
              "sw=%lu,lockfail=%lu,ssd=%d,i2cst=%lu,i2cerr=%lu,busy=%u,sda=%u,scl=%u,"
              "page=%u,eepst=%u,usbop=%u,stkdef=%lu,stksen=%lu,stkoled=%lu,heap=%lu,heapmin=%lu,ffst=%lu,fferr=%lu,ffstate=%lu,ffsens=%u,rstcsr=%lu,wdgrst=%lu,lastfault=%lu,wdgstale=%u,sens=%u,sdet=%u,bha=%u,bhhal=%lu,ensa=%u,enspid=%u,ensst=%u,enshal=%lu,ensop=%u,ensinit=%lu,enstmo=%lu,enserr=%lu,enscomm=%lu,ensrst=%lu,ensaqi=%u,enstvoc=%u,enseco2=%u,ensval=%u,bhraw=%u,enszero=%lu,ensfw=%u.%u.%u,enset=%u,ensrh=%u,oledon=%u,oledonn=%lu,oledoffn=%lu\r\n",
              (unsigned long)osKernelGetTickCount(), (unsigned int)oled_task_stage,
              (unsigned long)oled_task_loops, (unsigned long)oled_update_ok_count,
              (unsigned long)oled_update_fail_count, (unsigned long)oled_consecutive_fail,
              (unsigned long)i2c2_recovery_count, (unsigned long)i2c2_recovery_status,
              (unsigned long)oled_page_switch_count, (unsigned long)oled_lock_fail_count,
              (int)ssd1306_status, (unsigned long)hi2c2.State, (unsigned long)hi2c2.ErrorCode,
              (unsigned int)((__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) != RESET) ? 1U : 0U),
              (unsigned int)HAL_GPIO_ReadPin(MEMORY_SDA_GPIO_Port, MEMORY_SDA_Pin),
              (unsigned int)HAL_GPIO_ReadPin(MEMORY_SCL_GPIO_Port, MEMORY_SCL_Pin),
              (unsigned int)display_page, (unsigned int)eeprom_status_display,
              (unsigned int)usb_operation_state,
              (unsigned long)osThreadGetStackSpace(defaultTaskHandle),
              (unsigned long)osThreadGetStackSpace(sensorTaskHandle),
              (unsigned long)osThreadGetStackSpace(oledTaskHandle),
              (unsigned long)xPortGetFreeHeapSize(),
              (unsigned long)xPortGetMinimumEverFreeHeapSize(),
              (unsigned long)oled_ff_status, (unsigned long)oled_ff_i2cerr,
              (unsigned long)oled_ff_i2cstate, (unsigned int)oled_ff_sensor_active,
              (unsigned long)wdg_reset_csr, (unsigned long)wdg_iwdg_reset_count,
              (unsigned long)wdg_last_fault_code, (unsigned int)wdg_stale_mask,
              (unsigned int)Sensors_AvailableMask(), (unsigned int)Sensors_DetectedMask(),
              (unsigned int)bh1750_addr, (unsigned long)bh1750_last_hal,
              (unsigned int)ens160_addr, (unsigned int)ens160_part_id,
              (unsigned int)ens160_last_status, (unsigned long)ens160_last_hal,
              (unsigned int)ens160_opmode, (unsigned long)ens160_init_count,
              (unsigned long)ens160_timeout_count, (unsigned long)ens160_error_count,
              (unsigned long)ens160_comm_fail_count, (unsigned long)ens160_reset_count,
              (unsigned int)ens160_last_aqi, (unsigned int)ens160_last_tvoc,
              (unsigned int)ens160_last_eco2, (unsigned int)ens160_last_validity,
              (unsigned int)bh1750_last_raw,
              (unsigned long)ens160_zero_count, (unsigned int)ens160_fw_major,
              (unsigned int)ens160_fw_minor, (unsigned int)ens160_fw_build,
              (unsigned int)ens160_echo_t, (unsigned int)ens160_echo_rh,
              (unsigned int)oled_power_state.on, (unsigned long)oled_on_count, (unsigned long)oled_off_count);
          Diag_USB_Send(diag_line);
          continue;
      }

      /* Ajuste do RTC solicitado pelo PC; executado fora da callback USB. */
      if (usb_rtc_set_requested != 0U)
      {
          unsigned int year, month, day, hour, minute, second;
          char extra;
          int parsed;
          RTC_TimeTypeDef set_time = {0};
          RTC_DateTypeDef set_date = {0};
          static char resposta[64];

          usb_rtc_set_requested = 0U;
          parsed = sscanf(usb_rtc_set_command, "RTC_SET,%4u,%2u,%2u,%2u,%2u,%2u%c",
                          &year, &month, &day, &hour, &minute, &second, &extra);

          if (parsed != 6 || year < 2000U || year > 2099U || month < 1U || month > 12U ||
              day < 1U || day > 31U || hour > 23U || minute > 59U || second > 59U)
          {
              static const uint8_t err[] = "RTC_SET_END,ERRO,FORMATO\r\n";
              (void)CDC_Transmit_FS((uint8_t *)err, sizeof(err) - 1U);
          }
          else
          {
              /* HAL valida os limites basicos; verificamos tambem dias por mes/ano. */
              static const uint8_t days_per_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
              uint8_t max_day = days_per_month[month - 1U];
              uint8_t leap = ((year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U));
              if (month == 2U && leap) max_day = 29U;

              if (day > max_day)
              {
                  static const uint8_t err[] = "RTC_SET_END,ERRO,DATA\r\n";
                  (void)CDC_Transmit_FS((uint8_t *)err, sizeof(err) - 1U);
              }
              else
              {
                  /* Dia da semana gregoriano: 0=domingo, conforme a convencao HAL. */
                  static const uint8_t weekday_offset[12] = {0,3,2,5,0,3,5,1,4,6,2,4};
                  unsigned int y = year;
                  unsigned int weekday;
                  if (month < 3U) y--;
                  weekday = (y + y / 4U - y / 100U + y / 400U +
                             weekday_offset[month - 1U] + day) % 7U;
                  set_date.WeekDay = (uint8_t)(weekday + 1U);
                  set_date.Year = (uint8_t)(year - 2000U);
                  set_date.Month = (uint8_t)month;
                  set_date.Date = (uint8_t)day;
                  set_time.Hours = (uint8_t)hour;
                  set_time.Minutes = (uint8_t)minute;
                  set_time.Seconds = (uint8_t)second;
                  set_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
                  set_time.StoreOperation = RTC_STOREOPERATION_RESET;

                  App_RTC_Lock();
                  uint8_t rtc_set_ok = ((HAL_RTC_SetTime(&hrtc, &set_time, RTC_FORMAT_BIN) == HAL_OK) &&
                                        (HAL_RTC_SetDate(&hrtc, &set_date, RTC_FORMAT_BIN) == HAL_OK)) ? 1U : 0U;
                  App_RTC_Unlock();
                  if (rtc_set_ok != 0U)
                  {
                      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);
                      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0xA55AU);
                      rtc_time_valid = 1U;
                      UpdateDisplaySynchronized();
                      (void)snprintf(resposta, sizeof(resposta), "RTC_SET_END,OK,%04u-%02u-%02u %02u:%02u:%02u\r\n",
                                     year, month, day, hour, minute, second);
                      (void)CDC_Transmit_FS((uint8_t *)resposta, (uint16_t)strlen(resposta));
                  }
                  else
                  {
                      static const uint8_t err[] = "RTC_SET_END,ERRO,HAL\r\n";
                      (void)CDC_Transmit_FS((uint8_t *)err, sizeof(err) - 1U);
                  }
              }
          }
          memset(usb_rtc_set_command, 0, sizeof(usb_rtc_set_command));
          continue;
      }

      /* Limpeza solicitada pelo PC: executada fora da callback USB. */
      if (usb_clear_requested != 0U)
      {
          HAL_StatusTypeDef clear_status;
          usb_clear_requested = 0U;
          usb_operation_state = 2U;
          UpdateDisplaySynchronized();
          if (osMutexAcquire(eepromMutexHandle, osWaitForever) == osOK)
          {
              clear_status = EEPROM24LC512_EraseAll();
              if (clear_status == HAL_OK)
              {
                  eeprom_status_display = 0U;
                  (void)EEPROM_LoadMetadata();
              }
              (void)osMutexRelease(eepromMutexHandle);
          }
          else
          {
              clear_status = HAL_ERROR;
          }

          if (clear_status == HAL_OK)
          {
              usb_operation_state = 0U;
              static const uint8_t resposta[] = "CLEAR_END,OK\r\n";
              (void)CDC_Transmit_FS((uint8_t *)resposta, sizeof(resposta) - 1U);
          }
          else
          {
              usb_operation_state = 3U;
              eeprom_status_display = 1U;
              static const uint8_t resposta[] = "CLEAR_END,ERRO\r\n";
              (void)CDC_Transmit_FS((uint8_t *)resposta, sizeof(resposta) - 1U);
          }
          continue;
      }

      /* A exportacao roda no fluxo principal, nunca dentro da callback USB. */
      if (usb_export_requested != 0U)
      {
          usb_export_requested = 0U;
          usb_operation_state = 1U;
          diagnostic_export_stage = 1U;
          diagnostic_export_result = -1;
          diagnostic_usb_tx_result = -1;
          UpdateDisplaySynchronized();
          if (osMutexAcquire(eepromMutexHandle, osWaitForever) == osOK)
          {
              diagnostic_export_stage = 2U; /* mutex EEPROM adquirido; iniciar rotina */
              HAL_StatusTypeDef export_status = EEPROM_ExportCSV_USB();
              diagnostic_export_result = (export_status == HAL_OK) ? 0 : ((export_status == HAL_TIMEOUT) ? 2 : 1);
              diagnostic_export_stage = 6U; /* rotina retornou */
              (void)osMutexRelease(eepromMutexHandle);
              if (export_status != HAL_OK)
                  usb_operation_state = 3U;
              else
                  usb_operation_state = 0U;
          }
          else
          {
              usb_operation_state = 3U;
          }
          UpdateDisplaySynchronized();
          continue;
      }

      /* A interface OLED possui tarefa própria; esta tarefa mantém o processamento USB. */
      osDelay(20U);
    }
}

/* Tarefa de aquisição: mantém o período de 10 s usando o tick do RTOS.
 * O acesso às variáveis de sensores é protegido contra leitura simultânea pelo OLED.
 * O mutex de EEPROM serializa gravação, exportação e limpeza. */
/* Atualização de baixa prioridade. Copia o estado compartilhado sob mutexes curtos;
 * a renderização e as transações I2C2 ocorrem depois de liberar os mutexes. */
void StartOledTask(void *argument)
{
    (void)argument;
    uint8_t button_count = 0U, usb_prev = 0U;
    OledPower_Init(&oled_power_state, (uint32_t)((uint64_t)osKernelGetTickCount() * 1000U / osKernelGetTickFreq()));
    uint32_t next_page_tick = osKernelGetTickCount() + (3000U * osKernelGetTickFreq()) / 1000U;
    uint32_t last_recovery_tick = 0U;
    for (;;)
    {
        uint32_t now = osKernelGetTickCount();
        oled_task_loops++;
        Watchdog_Checkin(WDG_TASK_OLED);

        /* V13: o OLED apaga sozinho (economia de energia) e acende com o botao (PA0, ativo em nivel
         * baixo, filtrado por 2 leituras seguidas) ou quando uma operacao USB comeca. */
        {
            uint32_t now_ms = (uint32_t)((uint64_t)now * 1000U / osKernelGetTickFreq());
            uint8_t raw = (HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
            uint8_t usb_active = (usb_operation_state != 0U) ? 1U : 0U;
            OledPowerAction action;

            button_count = (raw != 0U) ? ((button_count < 255U) ? (uint8_t)(button_count + 1U) : 255U) : 0U;
            action = OledPower_Update(&oled_power_state, now_ms, (button_count >= 2U) ? 1U : 0U,
                                      ((usb_active != 0U) && (usb_prev == 0U)) ? 1U : 0U, OLED_AUTO_OFF_MS);
            usb_prev = usb_active;

            if (action == OLED_POWER_TURN_ON)
            {
                if (SSD1306_SetPower(true) == HAL_OK) { oled_on_count++; oledRefreshRequested = 1U; }
                else OledPower_ActionFailed(&oled_power_state, action, now_ms);
            }
            else if (action == OLED_POWER_TURN_OFF)
            {
                if (SSD1306_SetPower(false) == HAL_OK) oled_off_count++;
                else OledPower_ActionFailed(&oled_power_state, action, now_ms);
            }
        }
        if (oled_power_state.on == 0U)
        {
            /* Display apagado: nao renderiza nem ocupa o I2C2; so verifica o botao a cada 20 ms. */
            osDelay(20U);
            continue;
        }

        if ((int32_t)(now - next_page_tick) >= 0)
        {
            display_page = (display_page == 0U) ? 1U : 0U;
            oled_page_switch_count++;
            next_page_tick = now + (3000U * osKernelGetTickFreq()) / 1000U;
            oledRefreshRequested = 1U;
        }

        if (oledRefreshRequested != 0U)
        {
            DisplaySnapshot snapshot = {0};
            oledRefreshRequested = 0U;

            oled_task_stage = 1U;
            if (osMutexAcquire(sensorDataMutexHandle, osWaitForever) == osOK)
            {
                snapshot.sensor_present_mask = Sensors_AvailableMask();
                snapshot.sensor_detected_mask = Sensors_DetectedMask();
                snapshot.rtc_time_valid = rtc_time_valid;
                snapshot.ssd1306_status = ssd1306_status;
                (void)osMutexRelease(sensorDataMutexHandle);
            }

            /*
             * Diagnostico V5: a exportacao pode manter eepromMutexHandle ocupado
             * por bastante tempo. A tarefa OLED nao deve ficar bloqueada
             * indefinidamente: se nao conseguir obter o mutex em ate ~100 ms,
             * preserva a tela atual e tenta atualizar novamente no proximo ciclo.
             */
            uint32_t eeprom_wait_ticks =
                (100U * osKernelGetTickFreq() + 999U) / 1000U;
            if (eeprom_wait_ticks == 0U) eeprom_wait_ticks = 1U;

            oled_task_stage = 2U;
            if (osMutexAcquire(eepromMutexHandle, eeprom_wait_ticks) != osOK)
            {
                oledRefreshRequested = 1U;
                osDelay(20U);
                continue;
            }

            snapshot.eeprom_status_display = eeprom_status_display;
            snapshot.eeprom_metadata_load_ok = eeprom_metadata_load_ok;
            snapshot.eeprom_full = eeprom_full;
            snapshot.eeprom_active = eeprom_metadata.reserved;
            snapshot.eeprom_next_address = eeprom_next_address;
            snapshot.usb_operation_state = usb_operation_state;
            snapshot.diagnostic_sensor_stage = diagnostic_sensor_stage;
            snapshot.diagnostic_export_stage = diagnostic_export_stage;
            snapshot.diagnostic_export_result = diagnostic_export_result;
            snapshot.diagnostic_usb_tx_result = diagnostic_usb_tx_result;
            (void)osMutexRelease(eepromMutexHandle);

            snapshot.oled_available = oled_available;
            snapshot.display_page = display_page;
            oled_task_stage = 3U;
            UpdateDisplaySnapshot(&snapshot);
            oled_task_stage = 0U;

            /* V9: uma atualizacao falhando => recupera o I2C2 e o SSD1306
             * (no maximo uma vez a cada 5 s). */
            if ((oled_consecutive_fail >= 1U) &&
                ((last_recovery_tick == 0U) ||
                 ((uint32_t)(osKernelGetTickCount() - last_recovery_tick) >= 5000U)))
            {
                oled_task_stage = 6U;
                (void)RecoverI2C2AndOled();
                last_recovery_tick = osKernelGetTickCount();
                oled_consecutive_fail = 0U;
                oledRefreshRequested = 1U;
                oled_task_stage = 0U;
            }
        }
        osDelay(20U);
    }
}

void StartSensorTask(void *argument)
{
    (void)argument;
    uint32_t period_ticks = (SENSOR_ACQUISITION_INTERVAL_MS * osKernelGetTickFreq()) / 1000U;
    if (period_ticks == 0U)
        period_ticks = 1U;
    uint32_t next_wake_tick = osKernelGetTickCount(); /* primeira amostra imediata */
    uint8_t consecutive_no_sensor_samples = 0U;
    uint32_t last_sensor_i2c_recovery_tick = 0U;

    for (;;)
    {
        uint32_t now;

        Watchdog_Checkin(WDG_TASK_SENSOR);
#if ENABLE_WDT_TEST
        if (wdg_test_hang != 0U)
        {
            /* Comando WDTTEST: simula task travada; o watchdog deve reiniciar a placa. */
            for (;;) { (void)osDelay(1000U); }
        }
#endif

        (void)osMutexAcquire(sensorDataMutexHandle, osWaitForever);
        now = HAL_GetTick();

        sensor_cycle_active = 1U;
        App_RTC_Lock();
        (void)HAL_RTC_GetTime(&hrtc, &rtc_test_time, RTC_FORMAT_BIN);
        (void)HAL_RTC_GetDate(&hrtc, &rtc_test_date, RTC_FORMAT_BIN);
        App_RTC_Unlock();

        /* Le todos os sensores da tabela (sensor_registry.c). Estagios 1..3 como na V10. */
        for (uint8_t si = 0U; si < Sensors_Count(); si++)
        {
            diagnostic_sensor_stage = (uint8_t)(si + 1U);
            Sensors_ReadOne(si);
        }
        Sensors_UpdateDisplayValues();

        diagnostic_sensor_stage = 4U; /* avaliacao e possivel gravacao EEPROM */
        if (Sensors_AvailableMask() != 0U)
        {
            consecutive_no_sensor_samples = 0U;
            /*
             * Diagnostico V4: a tarefa de sensores NAO deve esperar indefinidamente
             * pela EEPROM enquanto mantem sensorDataMutexHandle. Isso poderia
             * bloquear a atualizacao do OLED e impedir a alternancia do LED.
             * Timeout em ticks: aproximadamente 250 ms.
             */
            uint32_t eeprom_wait_ticks =
                (250U * osKernelGetTickFreq() + 999U) / 1000U;
            if (eeprom_wait_ticks == 0U) eeprom_wait_ticks = 1U;

            if (osMutexAcquire(eepromMutexHandle, eeprom_wait_ticks) == osOK)
            {
                if (EEPROM_SaveSensorRecord() != HAL_OK && eeprom_status_display == 0U)
                    eeprom_status_display = 1U;
                (void)osMutexRelease(eepromMutexHandle);
            }
            else if ((eeprom_status_display == 0U) && (usb_operation_state == 0U))
            {
                /* A amostra atual pode ser perdida; nao altera registros existentes.
                 * Durante EXPORT/CLEAR o mutex fica ocupado de proposito: nao e erro da EEPROM. */
                eeprom_status_display = 1U;
            }
        }
        else
        {
            if (consecutive_no_sensor_samples < 255U)
                consecutive_no_sensor_samples++;

            /* Recuperação limitada a uma tentativa por 30 s e somente no I2C1. */
            if ((consecutive_no_sensor_samples >= 3U) &&
                ((last_sensor_i2c_recovery_tick == 0U) ||
                 ((uint32_t)(now - last_sensor_i2c_recovery_tick) >= 30000U)))
            {
                (void)RecoverSensorI2C1();
                last_sensor_i2c_recovery_tick = now;
                consecutive_no_sensor_samples = 0U;
            }
        }

        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        diagnostic_sensor_stage = 5U; /* ciclo chegou ao LED */
        sensor_cycle_active = 0U;
        (void)osMutexRelease(sensorDataMutexHandle);

        /* Mantém o ciclo periódico; se o processamento ultrapassar o prazo,
         * agenda o próximo ciclo a partir do instante atual, sem amostras em rajada. */
        {
            next_wake_tick += period_ticks;
            if ((int32_t)(osKernelGetTickCount() - next_wake_tick) >= 0)   // ciclo estourou o período
                next_wake_tick = osKernelGetTickCount() + period_ticks;
        }
        (void)osDelayUntil(next_wake_tick);
    }
}


/* Cria os mutexes e as tasks adicionais (sensores e OLED). A defaultTask e criada pelo
 * codigo gerado do CubeMX e executa App_CommandTask(). Chamada no bloco USER CODE
 * RTOS_THREADS do main.c, antes de osKernelStart(). */
void App_RTOS_Create(void)
{
    eepromMutexHandle = osMutexNew(NULL);
    i2c2MutexHandle = osMutexNew(NULL);
    sensorDataMutexHandle = osMutexNew(NULL);
    rtcMutexHandle = osMutexNew(NULL);
    if ((eepromMutexHandle == NULL) || (i2c2MutexHandle == NULL) ||
        (sensorDataMutexHandle == NULL) || (rtcMutexHandle == NULL))
    {
        Error_Handler();
    }

    sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);
    oledTaskHandle = osThreadNew(StartOledTask, NULL, &oledTask_attributes);
    if ((defaultTaskHandle == NULL) || (sensorTaskHandle == NULL) || (oledTaskHandle == NULL))
    {
        Error_Handler();
    }

    /* Prazo inicial para o watchdog: a inicializacao anterior ao kernel pode demorar. */
    Watchdog_Checkin(WDG_TASK_SENSOR);
    Watchdog_Checkin(WDG_TASK_OLED);
}
