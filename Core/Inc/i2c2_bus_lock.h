/**
 * @file i2c2_bus_lock.h
 * @brief Exclusão mútua do barramento I²C2, compartilhado pelo OLED e pelas EEPROMs.
 */
#ifndef I2C2_BUS_LOCK_H
#define I2C2_BUS_LOCK_H
#include "stm32f4xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Obtém o mutex do barramento I²C2.
 *
 * Antes do escalonador, as inicializações são sequenciais e a função retorna `HAL_OK` sem
 * usar o mutex.
 * @retval HAL_OK    Barramento obtido (ou RTOS ainda não iniciado).
 * @retval HAL_ERROR Mutex inexistente ou falha ao obtê-lo.
 */
HAL_StatusTypeDef I2C2_BusAcquire(void);

/** @brief Libera o mutex do barramento I²C2 obtido por I2C2_BusAcquire(). */
void I2C2_BusRelease(void);
#ifdef __cplusplus
}
#endif
#endif
